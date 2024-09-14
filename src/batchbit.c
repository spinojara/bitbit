/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2024 Isak Ellmer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>

#include "position.h"
#include "util.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "bitboard.h"
#include "move.h"
#include "nnue.h"
#include "evaluate.h"
#include "io.h"

uint64_t seed;

struct batch {
	size_t size;
	int ind_active;
	int32_t *ind1;
	int32_t *ind2;
	float *eval;
};

struct dataloader {
	size_t requested_size;
	struct batch *batch;
	struct batch *prepared;

	int ready;
	
	pthread_cond_t cond;
	pthread_mutex_t mutex;

	struct position *pos;
	
	double random_skip;

	FILE *f;
};

static inline uint16_t make_index_virtual(int turn, int square, int piece) {
	return orient_horizontal(turn, square) + piece_to_index[turn][piece] + PS_END * 64;
}

void *batch_prepare(void *ptr) {
	struct dataloader *dataloader = ptr;

	pthread_mutex_lock(&dataloader->mutex);

	struct batch *batch = dataloader->prepared;
	batch->size = 0;
	batch->ind_active = 0;

	size_t counter1 = 0, counter2 = 0;

	while (batch->size < dataloader->requested_size) {
		move_t move = 0;
		read_move(dataloader->f, &move);
		if (move) {
			do_move(dataloader->pos, &move);
		}
		else {
			read_position(dataloader->f, dataloader->pos);
			read_result(dataloader->f, NULL);
		}

		int32_t eval = VALUE_NONE;
		read_eval(dataloader->f, &eval);
		if (feof(dataloader->f)) {
			fseek(dataloader->f, 0, SEEK_SET);
			continue;
		}

		int skip = (eval == VALUE_NONE) || bernoulli(dataloader->random_skip, &seed);
		if (skip)
			continue;

		batch->eval[batch->size] = ((float)(FV_SCALE * eval)) / (127 * 64);
		int index, square;
		const int king_square[] = { orient_horizontal(BLACK, ctz(dataloader->pos->piece[BLACK][KING])), orient_horizontal(WHITE, ctz(dataloader->pos->piece[WHITE][KING])) };
		for (int piece = PAWN; piece < KING; piece++) {
			for (int turn = 0; turn <= 1; turn++) {
				uint64_t b = dataloader->pos->piece[turn][piece];
				while (b) {
					batch->ind_active += 2;
					square = ctz(b);
					index = make_index(dataloader->pos->turn, square, colored_piece(piece, turn), king_square[dataloader->pos->turn]);
					batch->ind1[counter1++] = batch->size;
					batch->ind1[counter1++] = index;
					index = make_index(other_color(dataloader->pos->turn), square, colored_piece(piece, turn), king_square[other_color(dataloader->pos->turn)]);
					batch->ind2[counter2++] = batch->size;
					batch->ind2[counter2++] = index;
					index = make_index_virtual(dataloader->pos->turn, square, colored_piece(piece, turn));
					batch->ind1[counter1++] = batch->size;
					batch->ind1[counter1++] = index;
					index = make_index_virtual(other_color(dataloader->pos->turn), square, colored_piece(piece, turn));
					batch->ind2[counter2++] = batch->size;
					batch->ind2[counter2++] = index;
					b = clear_ls1b(b);
				}
			}
		}
		batch->size++;
	}

	dataloader->ready = 1;
	pthread_cond_signal(&dataloader->cond);
	pthread_mutex_unlock(&dataloader->mutex);

	return NULL;
}

int batch_prepare_thread(void *ptr) {
	pthread_t thread;
	return pthread_create(&thread, NULL, &batch_prepare, ptr) || pthread_detach(thread);
}

struct batch *batch_fetch(void *ptr) {
	struct dataloader *dataloader = ptr;

	pthread_mutex_lock(&dataloader->mutex);

	while (!dataloader->ready)
		pthread_cond_wait(&dataloader->cond, &dataloader->mutex);

	struct batch *batch = dataloader->batch;
	struct batch *prepared = dataloader->prepared;

	batch->size = prepared->size;
	batch->ind_active = prepared->ind_active,
	memcpy(batch->ind1, prepared->ind1, 4 * 30 * dataloader->requested_size * sizeof(*dataloader->batch->ind1));
	memcpy(batch->ind2, prepared->ind2, 4 * 30 * dataloader->requested_size * sizeof(*dataloader->batch->ind2));
	memcpy(batch->eval, prepared->eval, dataloader->requested_size * sizeof(*dataloader->batch->eval));
	dataloader->ready = 0;

	batch_prepare_thread(ptr);

	pthread_mutex_unlock(&dataloader->mutex);

	return dataloader->batch;
}

struct batch *balloc(size_t requested_size) {
	struct batch *batch = calloc(1, sizeof(*batch));
	batch->ind1 = malloc(4 * 30 * requested_size * sizeof(*batch->ind1));
	batch->ind2 = malloc(4 * 30 * requested_size * sizeof(*batch->ind2));
	batch->eval = malloc(requested_size * sizeof(*batch->eval));

	return batch;
}

void bfree(struct batch *batch) {
	free(batch->ind1);
	free(batch->ind2);
	free(batch->eval);
	free(batch);
}

void *loader_open(const char *s, size_t requested_size, double random_skip) {
	FILE *f = fopen(s, "rb"); 
	if (!f) {
		fprintf(stderr, "error: failed to open file %s\n", s);
		return NULL;
	}
	struct dataloader *dataloader = calloc(1, sizeof(*dataloader));
	dataloader->pos = calloc(1, sizeof(*dataloader->pos));
	dataloader->batch = balloc(requested_size);
	dataloader->prepared = balloc(requested_size);

	dataloader->requested_size = requested_size;
	dataloader->random_skip = random_skip;

	pthread_mutex_init(&dataloader->mutex, NULL);
	pthread_cond_init(&dataloader->cond, NULL);

	dataloader->f = f;
	fseek(dataloader->f, 0, SEEK_SET);

	dataloader->ready = 0;
	batch_prepare_thread(dataloader);

	return dataloader;
}

void loader_close(void *ptr) {
	struct dataloader *dataloader = ptr;
	fclose(dataloader->f);
	bfree(dataloader->batch);
	bfree(dataloader->prepared);
	free(dataloader->pos);
	
	pthread_mutex_destroy(&dataloader->mutex);
	pthread_cond_destroy(&dataloader->cond);

	free(dataloader);
}

void loader_reset(void *ptr) {
	struct dataloader *dataloader = ptr;
	fseek(dataloader->f, 0, SEEK_SET);
}

void batchbit_init(void) {
	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	position_init();
	seed = time(NULL);
}
