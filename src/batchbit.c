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

struct data {
	size_t requested_size;
	struct batch *batch;

	struct position *pos;
	
	double random_skip;

	FILE *f;
};

static inline uint16_t make_index_virtual(int turn, int square, int piece) {
	return orient_horizontal(turn, square) + piece_to_index[turn][piece] + PS_END * 64;
}

struct batch *next_batch(void *ptr) {
	struct data *data = ptr;
	struct batch *batch = data->batch;
	batch->size = 0;
	batch->ind_active = 0;

	size_t counter1 = 0, counter2 = 0;

	while (batch->size < data->requested_size) {
		move_t move = 0;
		read_move(data->f, &move);
		if (move)
			do_move(data->pos, &move);
		else
			read_position(data->f, data->pos);

		int32_t eval = VALUE_NONE;
		read_eval(data->f, &eval);
		if (feof(data->f)) {
			fseek(data->f, 0, SEEK_SET);
			continue;
		}


		int skip = (eval == VALUE_NONE) || bernoulli(data->random_skip, &seed);
		if (skip)
			continue;

		batch->eval[batch->size] = ((float)(FV_SCALE * eval)) / (127 * 64);
		int index, square;
		const int king_square[] = { orient_horizontal(BLACK, ctz(data->pos->piece[BLACK][KING])), orient_horizontal(WHITE, ctz(data->pos->piece[WHITE][KING])) };
		for (int piece = PAWN; piece < KING; piece++) {
			for (int turn = 0; turn <= 1; turn++) {
				uint64_t b = data->pos->piece[turn][piece];
				while (b) {
					batch->ind_active += 2;
					square = ctz(b);
					index = make_index(data->pos->turn, square, colored_piece(piece, turn), king_square[data->pos->turn]);
					batch->ind1[counter1++] = batch->size;
					batch->ind1[counter1++] = index;
					index = make_index(other_color(data->pos->turn), square, colored_piece(piece, turn), king_square[other_color(data->pos->turn)]);
					batch->ind2[counter2++] = batch->size;
					batch->ind2[counter2++] = index;
					index = make_index_virtual(data->pos->turn, square, colored_piece(piece, turn));
					batch->ind1[counter1++] = batch->size;
					batch->ind1[counter1++] = index;
					index = make_index_virtual(other_color(data->pos->turn), square, colored_piece(piece, turn));
					batch->ind2[counter2++] = batch->size;
					batch->ind2[counter2++] = index;
					b = clear_ls1b(b);
				}
			}
		}
		batch->size++;
	}
	return batch;
}

void *batch_open(const char *s, size_t requested_size, double random_skip) {
	struct data *data = calloc(1, sizeof(*data));
	data->pos = calloc(1, sizeof(*data->pos));
	data->batch = calloc(1, sizeof(*data->batch));

	data->requested_size = requested_size;
	data->random_skip = random_skip;

	data->batch->ind1 = malloc(4 * 30 * data->requested_size * sizeof(*data->batch->ind1));
	data->batch->ind2 = malloc(4 * 30 * data->requested_size * sizeof(*data->batch->ind2));
	data->batch->eval = malloc(data->requested_size * sizeof(*data->batch->eval));

	startpos(data->pos);
	data->f = fopen(s, "rb");
	if (!data->f) {
		printf("Failed to open data file.\n");
		exit(1);
	}
	fseek(data->f, 0, SEEK_SET);
	return data;
}

void batch_close(void *ptr) {
	struct data *data = ptr;
	fclose(data->f);
	free(data->batch->ind1);
	free(data->batch->ind2);
	free(data->batch->eval);
	free(data->batch);
	free(data->pos);
	free(data);
}

void batch_reset(void *ptr) {
	struct data *data = ptr;
	fseek(data->f, 0, SEEK_SET);
}

void batch_init(void) {
	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	position_init();
	seed = time(NULL);
}
