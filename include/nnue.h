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

#ifndef NNUE_H
#define NNUE_H

#include <stdint.h>

#include "bitboard.h"
#include "position.h"
#include "move.h"

#define K_HALF_DIMENSIONS (256)
#define FT_IN_DIMS (64 * PS_END)
#define FT_OUT_DIMS (K_HALF_DIMENSIONS * 2)

#define SHIFT (6)
#define FT_SHIFT (0)
#define FV_SCALE (16)

typedef int16_t ft_weight_t;
typedef int16_t ft_bias_t;
typedef int8_t weight_t;
typedef int32_t bias_t;

enum {
	PS_W_PAWN   =  0 * 64,
	PS_B_PAWN   =  1 * 64,
	PS_W_KNIGHT =  2 * 64,
	PS_B_KNIGHT =  3 * 64,
	PS_W_BISHOP =  4 * 64,
	PS_B_BISHOP =  5 * 64,
	PS_W_ROOK   =  6 * 64,
	PS_B_ROOK   =  7 * 64,
	PS_W_QUEEN  =  8 * 64,
	PS_B_QUEEN  =  9 * 64,
	PS_END      = 10 * 64,
};

static uint32_t piece_to_index[2][13] = {
	{ 0, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, 0,
	     PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, 0, },
	{ 0, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, 0,
	     PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, 0, },
};

static inline uint16_t make_index(int turn, int square, int piece, int king_square) {
	return orient_horizontal(turn, square) + piece_to_index[turn][piece] + PS_END * king_square;
}

void add_index_slow(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2], int turn);

void refresh_accumulator(struct position *pos, int turn);

void do_update_accumulator(struct position *pos, move_t *m, int turn);
void undo_update_accumulator(struct position *pos, move_t *m, int turn);

void do_accumulator(struct position *pos, move_t *m);

void undo_accumulator(struct position *pos, move_t *m);

int32_t evaluate_nnue(struct position *pos);

int32_t evaluate_accumulator(const struct position *pos);

void nnue_init(void);

#endif
