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
	struct index *active_indices1_p;
	struct index *active_indices2_p;

	FILE *f;
};

void fast_do_move(struct position *pos, move *m) {
	assert(*m);
	uint8_t source_square = move_from(m);
	uint8_t target_square = move_to(m);
	uint64_t from = bitboard(source_square);
	uint64_t to = bitboard(target_square);
	uint64_t from_to = from | to;

	for (int piece = all; piece <= king; piece++) {
		pos->piece[1 - pos->turn][piece] &= ~to;
		if (pos->piece[pos->turn][piece] & from)
			pos->piece[pos->turn][piece] ^= from_to;
	}

	if (move_flag(m) == 1) {
		int direction = 2 * pos->turn - 1;
		pos->piece[1 - pos->turn][pawn] ^= bitboard(target_square - direction * 8);
		pos->piece[1 - pos->turn][all] ^= bitboard(target_square - direction * 8);
	}
	else if (move_flag(m) == 2) {
		pos->piece[pos->turn][pawn] ^= to;
		pos->piece[pos->turn][move_promote(m) + 2] ^= to;
	}
	else if (move_flag(m) == 3) {
		switch (target_square) {
		case g1:
			pos->piece[white][rook] ^= 0xA0;
			pos->piece[white][all] ^= 0xA0;
			break;
		case c1:
			pos->piece[white][rook] ^= 0x9;
			pos->piece[white][all] ^= 0x9;
			break;
		case g8:
			pos->piece[black][rook] ^= 0xA000000000000000;
			pos->piece[black][all] ^= 0xA000000000000000;
			break;
		case c8:
			pos->piece[black][rook] ^= 0x900000000000000;
			pos->piece[black][all] ^= 0x900000000000000;
			break;
		}
	}
	pos->turn = 1 - pos->turn;
}

static inline uint16_t make_index_p(int turn, int square, int piece) {
	return orient(turn, square) + piece_to_index[turn][piece] + PS_END * 64;
}

static inline void append_active_indices_p(struct position *pos, struct index *active, int turn) {
	active->size = 0;
	uint64_t b;
	int square;
	for (int color = 0; color < 2; color++) {
		for (int piece = 1; piece < 6; piece++) {
			b = pos->piece[color][piece];
			while (b) {
				square = ctz(b);
				active->values[active->size++] = make_index_p(turn, square, piece + 6 * (1 - color));
				b = clear_ls1b(b);
			}
		}
	}
}

static inline int bernoulli(double p) {
	return ((double)rand() / RAND_MAX) < p;
}

static inline int random_skip() {
	return bernoulli(0.75);
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
			fast_do_move(data->pos, &m);
		else
			fread(data->pos, sizeof(struct partialposition), 1, data->f);

		int16_t eval = VALUE_NONE;
		fread(&eval, 2, 1, data->f);
		if (feof(data->f))
			break;

		int skip = (eval == VALUE_NONE) || random_skip();
		if (skip)
			continue;

		batch->eval[batch->actual_size] = (float)FV_SCALE * eval / (127 * 64);
		append_active_indices(data->pos, &data->active_indices1[batch->actual_size], data->pos->turn);
		append_active_indices(data->pos, &data->active_indices2[batch->actual_size], 1 - data->pos->turn);
		append_active_indices_p(data->pos, &data->active_indices1_p[batch->actual_size], data->pos->turn);
		append_active_indices_p(data->pos, &data->active_indices2_p[batch->actual_size], 1 - data->pos->turn);

		assert(data->active_indices1[batch->actual_size].size == data->active_indices2[batch->actual_size].size);
		assert(data->active_indices1[batch->actual_size].size == data->active_indices1_p[batch->actual_size].size);
		assert(data->active_indices1[batch->actual_size].size == data->active_indices2_p[batch->actual_size].size);

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
			batch->ind1[index + 2 * (j + data->active_indices1[i].size) + 1] = data->active_indices1_p[i].values[j];
			batch->ind2[index + 2 * (j + data->active_indices1[i].size)] = i;
			batch->ind2[index + 2 * (j + data->active_indices1[i].size) + 1] = data->active_indices2_p[i].values[j];
		}
		index += 4 * data->active_indices1[i].size;
	}
	return batch;
}

void *batch_open(const char *s, size_t requested_size) {
	struct data *data = malloc(sizeof(struct data));
	memset(data, 0, sizeof(*data));
	data->pos = malloc(sizeof(*data->pos));
	memset(data->pos, 0, sizeof(*data->pos));
	data->batch = malloc(sizeof(*data->batch));
	memset(data->batch, 0, sizeof(*data->batch));

	data->requested_size = requested_size;

	data->active_indices1 = malloc(data->requested_size * sizeof(*data->active_indices1));
	data->active_indices2 = malloc(data->requested_size * sizeof(*data->active_indices2));
	data->active_indices1_p = malloc(data->requested_size * sizeof(*data->active_indices1_p));
	data->active_indices2_p = malloc(data->requested_size * sizeof(*data->active_indices2_p));

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
	free(data->active_indices1_p);
	free(data->active_indices2_p);
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
	srand(time(NULL));
}
