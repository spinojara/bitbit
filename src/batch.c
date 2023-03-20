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
#include "transposition_table.h"
#include "move.h"
#include "nnue.h"

struct position *pos;
uint64_t total = 0;
FILE *f = NULL;

struct batch *next_batch(int requested_size) {
	struct batch *batch = malloc(sizeof(struct batch));
	memset(batch, 0, sizeof(struct batch));
	struct index *active_indices1 = malloc(requested_size * sizeof(struct index));
	struct index *active_indices2 = malloc(requested_size * sizeof(struct index));
	float *eval = malloc(requested_size * sizeof(float));
	move m;

	for (int i = 0; i < requested_size; i++) {
		append_active_indices(pos, active_indices1 + i, pos->turn);
		append_active_indices(pos, active_indices2 + i, 1 - pos->turn);
		batch->ind_active += active_indices2[i].size;
		uint16_t t = read_le_uint16(f);
		if (reset_file_pointer(f))
			t = read_le_uint16(f);
		eval[i] = FV_SCALE * (float)*(int16_t *)(&t) / (127 * 64);
		m = read_le_uint16(f);
		//char str[8];
		//move_str_pgn(str, pos, &m);
		if (m)
			do_move(pos, &m);
		else
			startpos(pos);
	}

	batch->ind1 = malloc(2 * batch->ind_active * sizeof(int32_t));
	batch->ind2 = malloc(2 * batch->ind_active * sizeof(int32_t));

	int index = 0;
	batch->size = requested_size;
	batch->eval = malloc(batch->size * sizeof(float));
	memcpy(batch->eval, eval, batch->size * sizeof(float));
	
	for (int i = 0; i < requested_size; i++) {
		for (int j = 0; j < active_indices1[i].size; j++) {
			batch->ind1[index + 2 * j] = i;
			batch->ind1[index + 2 * j + 1] = active_indices1[i].values[j];
			batch->ind2[index + 2 * j] = i;
			batch->ind2[index + 2 * j + 1] = active_indices2[i].values[j];
		}
		index += 2 * active_indices1[i].size;
	}

	free(eval);
	free(active_indices1);
	free(active_indices2);

	return batch;
}

void free_batch(struct batch *batch) {
	free(batch->ind1);
	free(batch->ind2);
	free(batch->eval);
	free(batch);
}

void batch_open(const char *s) {
	startpos(pos);
	f = fopen(s, "rb");
	if (!f)
		printf("Failed to open data file.\n");
}

void batch_close(void) {
	fclose(f);
}

void batch_init(void) {
	util_init();
	magic_bitboard_init();
	attack_gen_init();
	bitboard_init();
	transposition_table_init();
	position_init();
	pos = malloc(sizeof(struct position));
	startpos(pos);
}

void batch_term(void) {
	position_term();
	transposition_table_term();
	free(pos);
}
