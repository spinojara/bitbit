/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2025 Isak Ellmer
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

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include "position.h"
#include "move.h"
#include "nnue.h"
#include "evaluate.h"
#include "io.h"
#include "magicbitboard.h"
#include "attackgen.h"

struct entry {
	int result;
	uint64_t piece[2][7];
	int turn;
	int fullmove;
	unsigned flag;
	int32_t eval;
};

struct batch {
	size_t size;
	int ind_active;
	int32_t *ind1;
	int32_t *ind2;
	float *eval;
	float *result;

	void *_next;
};

struct dataloader {
	size_t requested_size;
	size_t internal_size;
	int error;
	int stop;

	int jobs;
	pthread_t *thread;

	int num_batches;
	struct batch *first;
	struct batch *last;
	pthread_cond_t condfetch;
	pthread_cond_t condready;
	pthread_mutex_t mutex;
	pthread_mutex_t readmutex;

	double random_skip;
	int ply;
	int wdl_skip;
	int use_result;

	uint64_t baseseed;

	FILE *f;

	struct position pos;
	char result;
};

static inline uint16_t make_index_virtual(int turn, int square, int piece, int king_square) {
	return orient(turn, square, king_square) + piece_to_index[turn][piece] + FT_IN_DIMS;
}

/* From https://github.com/official-stockfish/nnue-pytorch
 * but fitted to bitbit's own data. */
double win_rate_model(int fullmove, int32_t eval, int result) {
	double m = (fullmove < 125 ? fullmove : 125) / 64.0;
	double x = eval / 100.0;

	double a = ((-0.26358 * m + 1.69976) * m + 0.18960) * m + 0.71337;
	double b = ((-0.06160 * m + 0.40556) * m - 0.13854) * m + 0.47889;

	double w = 1.0 / (1.0 + exp((a - x) / b));
	double l = 1.0 / (1.0 + exp((a + x) / b));
	double d = 1.0 - w - l;

	switch (result) {
	case RESULT_WIN:
		return w;
	case RESULT_LOSS:
		return l;
	case RESULT_DRAW:
		return d;
	default:
		return 0.0;
	}
}

int wdl_skip(int fullmove, int32_t eval, int result, uint64_t *seed) {
	return bernoulli(1.0 - win_rate_model(fullmove, eval, result), seed);
}

void *batch_alloc(size_t requested_size) {
	struct batch *batch = calloc(1, sizeof(*batch));
	batch->ind1 = malloc(4 * 32 * requested_size * sizeof(*batch->ind1));
	batch->ind2 = malloc(4 * 32 * requested_size * sizeof(*batch->ind2));
	batch->eval = malloc(requested_size * sizeof(*batch->eval));
	batch->result = malloc(requested_size * sizeof(*batch->result));
	batch->_next = NULL;

	return batch;
}

void batch_free(struct batch *batch) {
	if (!batch)
		return;
	free(batch->ind1);
	free(batch->ind2);
	free(batch->eval);
	free(batch->result);
	free(batch);
}

void batch_append(struct dataloader *dataloader, struct batch *batch) {
	if (!dataloader->first) {
		dataloader->first = dataloader->last = batch;
		pthread_cond_broadcast(&dataloader->condready);
	}
	else {
		dataloader->last->_next = batch;
		dataloader->last = dataloader->last->_next;
	}
}

int entry_fetch(struct dataloader *dataloader, struct entry *entries, size_t n) {
	move_t move;
	int32_t eval;
	unsigned char flag;

	pthread_mutex_lock(&dataloader->readmutex);

	pthread_mutex_lock(&dataloader->mutex);
	if (dataloader->error || dataloader->stop) {
		pthread_mutex_unlock(&dataloader->mutex);
		pthread_mutex_unlock(&dataloader->readmutex);
		return 1;
	}
	pthread_mutex_unlock(&dataloader->mutex);
	int r;
	for (size_t i = 0; i < n; ) {
		if ((r = read_move(dataloader->f, &move))) {
			if (r == 2 && feof(dataloader->f)) {
				fseek(dataloader->f, 0, SEEK_SET);
				continue;
			}
			else {
				pthread_mutex_lock(&dataloader->mutex);
				dataloader->stop = dataloader->error = 1;
				pthread_cond_broadcast(&dataloader->condready);
				pthread_mutex_unlock(&dataloader->mutex);
				pthread_mutex_unlock(&dataloader->readmutex);
				return 1;
			}
		}
		if (move) {
			do_move(&dataloader->pos, &move);
		}
		else {
			if (read_position(dataloader->f, &dataloader->pos) || read_result(dataloader->f, &dataloader->result)) {
				pthread_mutex_lock(&dataloader->mutex);
				dataloader->stop = dataloader->error = 1;
				pthread_cond_broadcast(&dataloader->condready);
				pthread_mutex_unlock(&dataloader->mutex);
				pthread_mutex_unlock(&dataloader->readmutex);
				return 1;
			}
		}

		if (read_eval(dataloader->f, &eval) || read_flag(dataloader->f, &flag)) {
			pthread_mutex_lock(&dataloader->mutex);
			dataloader->stop = dataloader->error = 1;
			pthread_cond_broadcast(&dataloader->condready);
			pthread_mutex_unlock(&dataloader->mutex);
			pthread_mutex_unlock(&dataloader->readmutex);
			return 1;
		}

		struct entry *entry = &entries[i];
		entry->eval = eval;
		entry->flag = flag;
		memcpy(entry->piece, dataloader->pos.piece, sizeof(entry->piece));
		entry->turn = dataloader->pos.turn;
		entry->result = dataloader->result;
		entry->fullmove = dataloader->pos.fullmove;

		i++;
	}
	pthread_mutex_unlock(&dataloader->readmutex);
	return 0;
}

void *batch_worker(void *ptr) {
	struct dataloader *dataloader = ptr;

	uint64_t seed = dataloader->baseseed + gettid();

	struct entry *entries = calloc(dataloader->internal_size, sizeof(*entries));
	size_t entry_index = dataloader->internal_size;

	while (1) {
		pthread_mutex_lock(&dataloader->mutex);
		while (dataloader->num_batches >= 4 * dataloader->jobs && !dataloader->stop && !dataloader->error)
			pthread_cond_wait(&dataloader->condfetch, &dataloader->mutex);

		if (dataloader->stop || dataloader->error) {
			pthread_mutex_unlock(&dataloader->mutex);
			break;
		}
		dataloader->num_batches++;

		pthread_mutex_unlock(&dataloader->mutex);

		struct batch *batch = batch_alloc(dataloader->requested_size);
		batch->size = 0;
		batch->ind_active = 0;

		size_t counter1 = 0, counter2 = 0;

		while (batch->size < dataloader->requested_size) {
			if (entry_index >= dataloader->internal_size) {
				if (entry_fetch(dataloader, entries, dataloader->internal_size))
					break;
				entry_index = 0;
			}
			struct entry *entry = &entries[entry_index];
			char result = entry->result;
			int32_t eval = entry->eval;
			unsigned char flag = entry->flag;
			if (result != RESULT_UNKNOWN)
				result *= (2 * entry->turn - 1);
			entry_index++;

			if (dataloader->use_result && result == RESULT_UNKNOWN)
				continue;

			int skip = (eval == VALUE_NONE) || (flag & FLAG_SKIP) || bernoulli(dataloader->random_skip, &seed) || (dataloader->wdl_skip && result != RESULT_UNKNOWN && wdl_skip(entry->fullmove, eval, result, &seed));
			if (skip)
				continue;

			batch->eval[batch->size] = ((float)(FV_SCALE * eval)) / (127 * 64);
			batch->result[batch->size] = result != RESULT_UNKNOWN ? (result + 1.0) / 2.0 : 0.5;

			int index, square;
			int king_square[] = { ctz(entry->piece[BLACK][KING]), ctz(entry->piece[WHITE][KING]) };
			size_t counter1_was = counter1;
			size_t counter2_was = counter2;
			for (int piece = PAWN; piece <= KING; piece++) {
				for (int turn = 0; turn <= 1; turn++) {
					if (piece == KING && turn == entry->turn)
						continue;
					uint64_t b = entry->piece[turn][piece];
					while (b) {
						batch->ind_active += 2;
						square = ctz(b);
						index = make_index(entry->turn, square, colored_piece(piece, turn), king_square[entry->turn]);
						batch->ind1[counter1++] = batch->size;
						batch->ind1[counter1++] = index;

						index = make_index(other_color(entry->turn), square, colored_piece(piece, turn), king_square[other_color(entry->turn)]);
						batch->ind2[counter2++] = batch->size;
						batch->ind2[counter2++] = index;

						index = make_index_virtual(entry->turn, square, colored_piece(piece, turn), king_square[entry->turn]);
						batch->ind1[counter1++] = batch->size;
						batch->ind1[counter1++] = index;

						index = make_index_virtual(other_color(entry->turn), square, colored_piece(piece, turn), king_square[other_color(entry->turn)]);
						batch->ind2[counter2++] = batch->size;
						batch->ind2[counter2++] = index;
						b = clear_ls1b(b);
					}
				}
			}
			for (size_t c = counter1_was + 3; c < counter1; c += 2) {
				size_t d = c;

				while (d > counter1_was + 1 && batch->ind1[d - 2] > batch->ind1[d]) {
					int32_t temp = batch->ind1[d - 2];
					batch->ind1[d - 2] = batch->ind1[d];
					batch->ind1[d] = temp;
					d -= 2;
				}
			}
			for (size_t c = counter2_was + 3; c < counter2; c += 2) {
				size_t d = c;

				while (d > counter2_was + 1 && batch->ind2[d - 2] > batch->ind2[d]) {
					int32_t temp = batch->ind2[d - 2];
					batch->ind2[d - 2] = batch->ind2[d];
					batch->ind2[d] = temp;
					d -= 2;
				}
			}
			batch->size++;
		}
		pthread_mutex_lock(&dataloader->mutex);
		if (dataloader->error || dataloader->stop) {
			dataloader->num_batches--;
			batch_free(batch);
			pthread_mutex_unlock(&dataloader->mutex);
			break;
		}
		else {
			batch_append(dataloader, batch);
		}
		pthread_mutex_unlock(&dataloader->mutex);
	}

	free(entries);
	return NULL;
}

struct batch *batch_fetch(void *ptr) {
	struct dataloader *dataloader = ptr;

	pthread_mutex_lock(&dataloader->mutex);

	while (!dataloader->first && !dataloader->error)
		pthread_cond_wait(&dataloader->condready, &dataloader->mutex);

	if (dataloader->error) {
		fprintf(stderr, "error: failed to make batch\n");
		pthread_mutex_unlock(&dataloader->mutex);
		return NULL;
	}

	struct batch *batch = dataloader->first;
	dataloader->num_batches--;
	dataloader->first = dataloader->first->_next;
	if (dataloader->first == NULL)
		dataloader->last = NULL;

	pthread_cond_broadcast(&dataloader->condfetch);
	pthread_mutex_unlock(&dataloader->mutex);

	return batch;
}

void *loader_open(const char *s, size_t requested_size, int jobs, double random_skip, int wdl_skip, int use_result) {
	if (jobs <= 0)
		jobs = 1;
	FILE *f = fopen(s, "rb");
	if (!f) {
		fprintf(stderr, "error: failed to open file '%s'\n", s);
		return NULL;
	}
	struct dataloader *dataloader = calloc(1, sizeof(*dataloader));
	dataloader->jobs = jobs;
	dataloader->requested_size = requested_size;
	dataloader->internal_size = requested_size;
	dataloader->random_skip = random_skip;
	dataloader->wdl_skip = wdl_skip;
	dataloader->use_result = use_result;
	dataloader->num_batches = 0;
	dataloader->f = f;

	pthread_mutex_init(&dataloader->mutex, NULL);
	pthread_mutex_init(&dataloader->readmutex, NULL);
	pthread_cond_init(&dataloader->condready, NULL);
	pthread_cond_init(&dataloader->condfetch, NULL);

	dataloader->error = 0;

	dataloader->thread = malloc(dataloader->jobs * sizeof(*dataloader->thread));

	dataloader->baseseed = time(NULL);

	for (int i = 0; i < dataloader->jobs; i++)
		pthread_create(&dataloader->thread[i], NULL, batch_worker, dataloader);

	return dataloader;
}

void loader_close(void *ptr) {
	struct dataloader *dataloader = ptr;

	pthread_mutex_lock(&dataloader->mutex);
	dataloader->stop = 1;
	pthread_cond_broadcast(&dataloader->condfetch);
	pthread_mutex_unlock(&dataloader->mutex);
	for (int i = 0; i < dataloader->jobs; i++)
		pthread_join(dataloader->thread[i], NULL);

	pthread_mutex_destroy(&dataloader->mutex);
	pthread_mutex_destroy(&dataloader->readmutex);
	pthread_cond_destroy(&dataloader->condready);
	pthread_cond_destroy(&dataloader->condfetch);

	for (struct batch *batch = dataloader->first; batch; ) {
		dataloader->num_batches--;
		struct batch *next = batch->_next;
		batch_free(batch);
		batch = next;
	}
	if (dataloader->num_batches != 0) {
		fprintf(stderr, "error: failed to free all the memory\n");
		fprintf(stderr, "%d\n", dataloader->num_batches);
	}
	if (dataloader->error) {
		fprintf(stderr, "error: an error occured\n");
	}

	free(dataloader->thread);

	fclose(dataloader->f);
	free(dataloader);
}

void batchbit_init(void) {
	magicbitboard_init();
	attackgen_init();
	bitboard_init();
}

int batchbit_version(void) {
	return VERSION_NNUE;
}
