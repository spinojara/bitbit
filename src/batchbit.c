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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "position.h"
#include "move.h"
#include "nnue.h"
#include "evaluate.h"
#include "io.h"
#include "magicbitboard.h"
#include "attackgen.h"

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

	double random_skip;
	int ply;
	int wdl_skip;
	int use_result;

	uint64_t baseseed;

	size_t size;
	const unsigned char *data;
	unsigned char *data_unmap;
	size_t index_size;
	size_t *index;
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

void *batch_worker(void *ptr) {
	struct dataloader *dataloader = ptr;
	struct position pos;

	uint64_t seed = dataloader->baseseed + gettid();
	size_t data_index = 0;

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

		char result = RESULT_UNKNOWN;
		while (batch->size < dataloader->requested_size) {
			move_t move = 0;
			if (data_index >= dataloader->size)
				data_index = 0;

			if (read_move_mem(dataloader->data, &move, &data_index, dataloader->size)) {
				pthread_mutex_lock(&dataloader->mutex);
				dataloader->stop = dataloader->error = 1;
				pthread_cond_broadcast(&dataloader->condready);
				pthread_mutex_unlock(&dataloader->mutex);
				break;
			}
			if (move) {
				do_move(&pos, &move);
			}
			else {
				/* Go to a random location. */
				data_index = dataloader->index[xorshift64(&seed) % dataloader->index_size];
				if (read_position_mem(dataloader->data, &pos, &data_index, dataloader->size) || read_result_mem(dataloader->data, &result, &data_index, dataloader->size)) {
					pthread_mutex_lock(&dataloader->mutex);
					dataloader->stop = dataloader->error = 1;
					pthread_cond_broadcast(&dataloader->condready);
					pthread_mutex_unlock(&dataloader->mutex);
					break;
				}
			}

			int32_t eval = VALUE_NONE;
			unsigned char flag;
			if (read_eval_mem(dataloader->data, &eval, &data_index, dataloader->size) || read_flag_mem(dataloader->data, &flag, &data_index, dataloader->size)) {
				pthread_mutex_lock(&dataloader->mutex);
				dataloader->stop = dataloader->error = 1;
				pthread_cond_broadcast(&dataloader->condready);
				pthread_mutex_unlock(&dataloader->mutex);
				break;
			}

			if (result == RESULT_UNKNOWN && dataloader->use_result)
				continue;

			int skip = (eval == VALUE_NONE) || (flag & FLAG_SKIP) || bernoulli(dataloader->random_skip, &seed) || (dataloader->wdl_skip && result != RESULT_UNKNOWN && wdl_skip(pos.fullmove, eval, pos.turn * result, &seed));
			if (skip)
				continue;

			batch->eval[batch->size] = ((float)(FV_SCALE * eval)) / (127 * 64);
			batch->result[batch->size] = result != RESULT_UNKNOWN ? ((2 * pos.turn - 1) * result + 1.0) / 2.0 : 0.5;

			int index, square;
			int king_square[] = { ctz(pos.piece[BLACK][KING]), ctz(pos.piece[WHITE][KING]) };
			for (int piece = PAWN; piece <= KING; piece++) {
				for (int turn = 0; turn <= 1; turn++) {
					if (piece == KING && turn == pos.turn)
						continue;
					uint64_t b = pos.piece[turn][piece];
					while (b) {
						batch->ind_active += 2;
						square = ctz(b);
						index = make_index(pos.turn, square, colored_piece(piece, turn), king_square[pos.turn]);
						batch->ind1[counter1++] = batch->size;
						batch->ind1[counter1++] = index;
						index = make_index(other_color(pos.turn), square, colored_piece(piece, turn), king_square[other_color(pos.turn)]);
						batch->ind2[counter2++] = batch->size;
						batch->ind2[counter2++] = index;
						index = make_index_virtual(pos.turn, square, colored_piece(piece, turn), king_square[pos.turn]);
						batch->ind1[counter1++] = batch->size;
						batch->ind1[counter1++] = index;
						index = make_index_virtual(other_color(pos.turn), square, colored_piece(piece, turn), king_square[other_color(pos.turn)]);
						batch->ind2[counter2++] = batch->size;
						batch->ind2[counter2++] = index;
						b = clear_ls1b(b);
					}
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

int loader_index(struct dataloader *dataloader) {
	size_t actual_size = 2;
	dataloader->index = malloc(actual_size * sizeof(*dataloader->index));

	for (size_t index = 0; index < dataloader->size - 1; ) {
		int new_position = (dataloader->data[index] == 0) && (dataloader->data[index + 1] == 0);
		if (index == 0 && !new_position)
			return 1;
		index += 2;
		if (new_position) {
			if (dataloader->index_size++ >= actual_size) {
				actual_size *= 2;
				dataloader->index = realloc(dataloader->index, actual_size * sizeof(*dataloader->index));
			}
			dataloader->index[dataloader->index_size - 1] = index;
			/* position (68) + result (1) */
			index += 68 + 1;
		}

		/* eval (2) + flag (1) */
		index += 3;
	}
	return 0;
}

void *loader_open(const char *s, size_t requested_size, int jobs, double random_skip, int wdl_skip, int use_result) {
	if (jobs <= 0)
		jobs = 1;
	int fd = open(s, O_RDONLY, 0);
	if (fd == -1) {
		fprintf(stderr, "error: failed to open file '%s'\n", s);
		return NULL;
	}
	struct stat st;
	if (fstat(fd, &st) == -1) {
		fprintf(stderr, "error: fstat\n");
		close(fd);
		return NULL;
	}
	struct dataloader *dataloader = calloc(1, sizeof(*dataloader));
	dataloader->size = st.st_size;
	dataloader->jobs = jobs;
	dataloader->data = dataloader->data_unmap = mmap(NULL, dataloader->size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (dataloader->data == MAP_FAILED) {
		fprintf(stderr, "error: mmap\n");
		free(dataloader);
		return NULL;
	}
	dataloader->requested_size = requested_size;
	dataloader->random_skip = random_skip;
	dataloader->wdl_skip = wdl_skip;
	dataloader->use_result = use_result;
	dataloader->num_batches = 0;

	dataloader->index = NULL;
	dataloader->index_size = 0;

	time_t t = time(NULL);
	if (loader_index(dataloader)) {
		fprintf(stderr, "error: file contains errors\n");
		munmap(dataloader->data_unmap, dataloader->size);
		free(dataloader->index);
		free(dataloader);
		return NULL;
	}

	pthread_mutex_init(&dataloader->mutex, NULL);
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
	free(dataloader->index);

	munmap(dataloader->data_unmap, dataloader->size);
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
