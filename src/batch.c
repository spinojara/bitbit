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
	
	double random_skip;

	FILE *f;
};

static inline uint16_t make_index_virtual(int turn, int square, int piece) {
	return orient_horizontal(turn, square) + piece_to_index[turn][piece] + PS_END * 64;
}

struct batch *next_batch(void *ptr) {
	struct data *data = ptr;
	struct batch *batch = data->batch;
	batch->actual_size = 0;
	batch->ind_active = 0;

	size_t counter1 = 0, counter2 = 0;

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
#if 1
		int index, square;
		const int king_squares[] = { orient_horizontal(black, ctz(data->pos->piece[black][king])), orient_horizontal(white, ctz(data->pos->piece[white][king])) };
		for (int piece = pawn; piece < king; piece++) {
			for (int turn = 0; turn <= 1; turn++) {
				uint64_t b = data->pos->piece[turn][piece];
				while (b) {
					batch->ind_active += 2;
					square = ctz(b);
					index = make_index(data->pos->turn, square, piece + 6 * (1 - turn), king_squares[data->pos->turn]);
					batch->ind1[counter1++] = batch->actual_size;
					batch->ind1[counter1++] = index;
					index = make_index(1 - data->pos->turn, square, piece + 6 * (1 - turn), king_squares[1 - data->pos->turn]);
					batch->ind2[counter2++] = batch->actual_size;
					batch->ind2[counter2++] = index;
					index = make_index_virtual(data->pos->turn, square, piece + 6 * (1 - turn));
					batch->ind1[counter1++] = batch->actual_size;
					batch->ind1[counter1++] = index;
					index = make_index_virtual(1 - data->pos->turn, square, piece + 6 * (1 - turn));
					batch->ind2[counter2++] = batch->actual_size;
					batch->ind2[counter2++] = index;
					b = clear_ls1b(b);
				}
			}
		}
#else
		int32_t *const indw = data->pos->turn ? batch->ind1 : batch->ind2;
		int32_t *const indb = data->pos->turn ? batch->ind2 : batch->ind1;
		int index, square;
		const int king_squares[] = { orient_horizontal(black, ctz(data->pos->piece[black][king])), orient_horizontal(white, ctz(data->pos->piece[white][king])) };
		const int perspective[] = { white, black };
		for (int piece = pawn; piece < king; piece++) {
			/* white perspective */
			for (int p = 0; p <= 1; p++) {
				const int turn = perspective[p];
				const uint64_t c = data->pos->piece[turn][piece];
				uint64_t b = c;
				while (b) {
					batch->ind_active += 2;
					square = ctz(b);
					index = make_index(white, square, piece + 6 * (1 - turn), king_squares[white]);
					indw[counter1++] = batch->actual_size;
					indw[counter1++] = index;
					b = clear_ls1b(b);
				}
			}
			/* black perspective */
			for (int p = 1; p >= 0; p--) {
				const int turn = perspective[p];
				const uint64_t c = data->pos->piece[turn][piece];
				uint64_t rank = RANK_8;
				for (int i = 0; i < 8; i++) {
					uint64_t b = c & rank;
					while (b) {
						square = ctz(b);
						index = make_index(black, square, piece + 6 * (1 - turn), king_squares[black]);
						indb[counter2++] = batch->actual_size;
						indb[counter2++] = index;
						b = clear_ls1b(b);
					}
					rank = shift_south(rank);
				}
			}
		}
		for (int piece = pawn; piece < king; piece++) {
			/* white perspective */
			for (int p = 0; p <= 1; p++) {
				const int turn = perspective[p];
				const uint64_t c = data->pos->piece[turn][piece];
				uint64_t b = c;
				while (b) {
					square = ctz(b);
					index = make_index_virtual(white, square, piece + 6 * (1 - turn));
					indw[counter1++] = batch->actual_size;
					indw[counter1++] = index;
					b = clear_ls1b(b);
				}
			}
			/* black perspective */
			for (int p = 1; p >= 0; p--) {
				const int turn = perspective[p];
				const uint64_t c = data->pos->piece[turn][piece];
				uint64_t rank = RANK_8;
				for (int i = 0; i < 8; i++) {
					uint64_t b = c & rank;
					while (b) {
						square = ctz(b);
						index = make_index_virtual(black, square, piece + 6 * (1 - turn));
						indb[counter2++] = batch->actual_size;
						indb[counter2++] = index;
						b = clear_ls1b(b);
					}
					rank = shift_south(rank);
				}
			}
		}
#endif
		batch->actual_size++;
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
