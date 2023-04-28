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

#include "batch.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "position.h"
#include "util.h"
#include "magic_bitboard.h"
#include "attack_gen.h"
#include "bitboard.h"
#include "move.h"
#include "nnue.h"

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

struct batch *next_batch(void *ptr, int requested_size) {
	struct data *data = (struct data *)ptr;
	struct batch *batch = malloc(sizeof(struct batch));
	memset(batch, 0, sizeof(struct batch));
	batch->requested_size = requested_size;
	struct index *active_indices1 = malloc(requested_size * sizeof(struct index));
	struct index *active_indices2 = malloc(requested_size * sizeof(struct index));
	struct index *active_indices1_p = malloc(requested_size * sizeof(struct index));
	struct index *active_indices2_p = malloc(requested_size * sizeof(struct index));
	float *eval = malloc(requested_size * sizeof(float));
	move m;

	for (int i = 0; i < requested_size; i++) {
		uint16_t t = read_le_uint(data->f, 2);
		if (feof(data->f))
			break;
		append_active_indices(data->pos, active_indices1 + i, data->pos->turn);
		append_active_indices(data->pos, active_indices2 + i, 1 - data->pos->turn);
		append_active_indices_p(data->pos, active_indices1_p + i, data->pos->turn);
		append_active_indices_p(data->pos, active_indices2_p + i, 1 - data->pos->turn);

		batch->ind_active += active_indices1[i].size + active_indices1_p[i].size;
		batch->size++;
		eval[i] = FV_SCALE * (float)*(int16_t *)(&t) / (127 * 64);
		m = read_le_uint(data->f, 2);
		if (m)
			do_move(data->pos, &m);
		else
			startpos(data->pos);
	}

	batch->ind1 = malloc(2 * batch->ind_active * sizeof(int32_t));
	batch->ind2 = malloc(2 * batch->ind_active * sizeof(int32_t));

	int index = 0;
	batch->eval = malloc(batch->size * sizeof(float));
	memcpy(batch->eval, eval, batch->size * sizeof(float));
	
	for (int i = 0; i < batch->size; i++) {
		for (int j = 0; j < active_indices1[i].size; j++) {
			batch->ind1[index + 2 * j] = i;
			batch->ind1[index + 2 * j + 1] = active_indices1[i].values[j];


			batch->ind2[index + 2 * j] = i;
			batch->ind2[index + 2 * j + 1] = active_indices2[i].values[j];
		}
		for (int j = 0; j < active_indices1[i].size; j++) {
			batch->ind1[index + 2 * (j + active_indices1[i].size)] = i;
			batch->ind1[index + 2 * (j + active_indices1[i].size) + 1] = active_indices1_p[i].values[j];
			batch->ind2[index + 2 * (j + active_indices1[i].size)] = i;
			batch->ind2[index + 2 * (j + active_indices1[i].size) + 1] = active_indices2_p[i].values[j];
		}
		index += 4 * active_indices1[i].size;
	}

	free(eval);
	free(active_indices1);
	free(active_indices1_p);
	free(active_indices2);
	free(active_indices2_p);

	return batch;
}

void free_batch(struct batch *batch) {
	free(batch->ind1);
	free(batch->ind2);
	free(batch->eval);
	free(batch);
}

void *batch_open(const char *s) {
	void *ptr = malloc(sizeof(struct data));
	struct data *data = ptr;
	memset(data, 0, sizeof(*data));
	startpos(data->pos);
	data->f = fopen(s, "rb");
	if (!data->f)
		printf("Failed to open data file.\n");
	while (1) {
		read_le_uint(data->f, 4);
		if (!feof(data->f))
			data->total++;
		else
			break;
	}
	fseek(data->f, 0, SEEK_SET);
	return ptr;
}

void batch_close(void *ptr) {
	struct data *data = ptr;
	fclose(data->f);
	free(data);
}

void batch_reset(void *ptr) {
	struct data *data = ptr;
	fseek(data->f, 0, SEEK_SET);
}

uint64_t batch_total(void *ptr) {
	struct data *data = ptr;
	return data->total;
}

void batch_init(void) {
	util_init();
	magic_bitboard_init();
	attack_gen_init();
	bitboard_init();
	position_init();
}
