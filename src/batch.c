/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022 Isak Ellmer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

uint64_t seed;

struct batch {
	size_t actual_size;
	int ind_active;
	int32_t *ind1;
	int32_t *ind2;
	float *eval;
};

struct data {
	size_t requested_size;
	struct batch *batch;

	struct position *pos;
	
	struct index *active_indices1;
	struct index *active_indices2;
	struct index *active_indices1_virtual;
	struct index *active_indices2_virtual;

	double random_skip;

	FILE *f;
};

static inline uint16_t make_index_virtual(int turn, int square, int piece) {
	return orient(turn, square) + piece_to_index[turn][piece] + PS_END * 64;
}

static inline void append_active_indices_virtual(struct position *pos, struct index *active, int turn) {
	active->size = 0;
	uint64_t b;
	int square;
	for (int color = 0; color < 2; color++) {
		for (int piece = 1; piece < 6; piece++) {
			b = pos->piece[color][piece];
			while (b) {
				square = ctz(b);
				active->values[active->size++] = make_index_virtual(turn, square, piece + 6 * (1 - color));
				b = clear_ls1b(b);
			}
		}
	}
}

struct batch *next_batch(void *ptr) {
	struct data *data = ptr;
	struct batch *batch = data->batch;
	batch->actual_size = 0;
	batch->ind_active = 0;

	while (batch->actual_size < data->requested_size) {
		move m = 0;
		fread(&m, 2, 1, data->f);
		if (m)
			do_move(data->pos, &m);
		else
			fread(data->pos, sizeof(struct partialposition), 1, data->f);

		int16_t eval = VALUE_NONE;
		fread(&eval, 2, 1, data->f);
		if (feof(data->f))
			break;

		int skip = (eval == VALUE_NONE) || bernoulli(data->random_skip, &seed);
		if (skip)
			continue;

		batch->eval[batch->actual_size] = (float)FV_SCALE * eval / (127 * 64);
		append_active_indices(data->pos, &data->active_indices1[batch->actual_size], data->pos->turn);
		append_active_indices(data->pos, &data->active_indices2[batch->actual_size], 1 - data->pos->turn);
		append_active_indices_virtual(data->pos, &data->active_indices1_virtual[batch->actual_size], data->pos->turn);
		append_active_indices_virtual(data->pos, &data->active_indices2_virtual[batch->actual_size], 1 - data->pos->turn);

		assert(data->active_indices1[batch->actual_size].size == data->active_indices2[batch->actual_size].size);
		assert(data->active_indices1[batch->actual_size].size == data->active_indices1_virtual[batch->actual_size].size);
		assert(data->active_indices1[batch->actual_size].size == data->active_indices2_virtual[batch->actual_size].size);

		batch->ind_active += 2 * data->active_indices1[batch->actual_size].size;
		batch->actual_size++;
	}

	int index = 0;
	for (size_t i = 0; i < batch->actual_size; i++) {
		for (int j = 0; j < data->active_indices1[i].size; j++) {
			batch->ind1[index + 2 * j] = i;
			batch->ind1[index + 2 * j + 1] = data->active_indices1[i].values[j];
			batch->ind2[index + 2 * j] = i;
			batch->ind2[index + 2 * j + 1] = data->active_indices2[i].values[j];
		}
		for (int j = 0; j < data->active_indices1[i].size; j++) {
			batch->ind1[index + 2 * (j + data->active_indices1[i].size)] = i;
			batch->ind1[index + 2 * (j + data->active_indices1[i].size) + 1] = data->active_indices1_virtual[i].values[j];
			batch->ind2[index + 2 * (j + data->active_indices1[i].size)] = i;
			batch->ind2[index + 2 * (j + data->active_indices1[i].size) + 1] = data->active_indices2_virtual[i].values[j];
		}
		index += 4 * data->active_indices1[i].size;
	}
	return batch;
}

void *batch_open(const char *s, size_t requested_size, double random_skip) {
	struct data *data = malloc(sizeof(struct data));
	memset(data, 0, sizeof(*data));
	data->pos = malloc(sizeof(*data->pos));
	memset(data->pos, 0, sizeof(*data->pos));
	data->batch = malloc(sizeof(*data->batch));
	memset(data->batch, 0, sizeof(*data->batch));

	data->requested_size = requested_size;
	data->random_skip = random_skip;

	data->active_indices1 = malloc(data->requested_size * sizeof(*data->active_indices1));
	data->active_indices2 = malloc(data->requested_size * sizeof(*data->active_indices2));
	data->active_indices1_virtual = malloc(data->requested_size * sizeof(*data->active_indices1_virtual));
	data->active_indices2_virtual = malloc(data->requested_size * sizeof(*data->active_indices2_virtual));

	data->batch->ind1 = malloc(4 * 30 * data->requested_size * sizeof(*data->batch->ind1));
	data->batch->ind2 = malloc(4 * 30 * data->requested_size * sizeof(*data->batch->ind2));
	data->batch->eval = malloc(data->requested_size * sizeof(*data->batch->eval));

	startpos(data->pos);
	data->f = fopen(s, "rb");
	if (!data->f)
		printf("Failed to open data file.\n");
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
	free(data->active_indices1);
	free(data->active_indices2);
	free(data->active_indices1_virtual);
	free(data->active_indices2_virtual);
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
