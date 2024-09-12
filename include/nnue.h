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

#ifndef NNUE_H
#define NNUE_H

#include <stdint.h>

#include "bitboard.h"
#include "position.h"
#include "move.h"
#include "util.h"

#define VERSION_NNUE 2

#define K_HALF_DIMENSIONS 512
#define FT_IN_DIMS (32 * PS_END)
#define FT_OUT_DIMS (K_HALF_DIMENSIONS * 2)

#define SHIFT 6
#define FT_SHIFT 0
#define FV_SCALE 16

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
	PS_KING     = 10 * 64,
	PS_END      = 11 * 64,
};

static const int king_bucket[64] = {
	 0,  1,  2,  3, -1, -1, -1, -1,
	 4,  5,  6,  7, -1, -1, -1, -1,
	 8,  9, 10, 11, -1, -1, -1, -1,
	12, 13, 14, 15, -1, -1, -1, -1,
	16, 17, 18, 19, -1, -1, -1, -1,
	20, 21, 22, 23, -1, -1, -1, -1,
	24, 25, 26, 27, -1, -1, -1, -1,
	28, 29, 30, 31, -1, -1, -1, -1,
};

static const uint32_t piece_to_index[2][13] = {
	{ 0, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, PS_KING,
	     PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, PS_KING, },
	{ 0, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, PS_KING,
	     PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, PS_KING, },
};

static inline int orient(int turn, int square, int king_square) {
	return orient_horizontal(turn, square) ^ ((file_of(king_square) >= 4) * 0x7);
}

static inline uint16_t make_index(int turn, int square, int piece, int king_square) {
	return orient(turn, square, king_square) + piece_to_index[turn][piece] + PS_END * king_bucket[king_square];
}

static inline int get_bucket(const struct position *pos) {
	return min((popcount(all_pieces(pos)) - 1) / 4, 7);
}

void add_index_slow(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2][8], int turn);

void refresh_accumulator(struct position *pos, int turn);

void do_update_accumulator(struct position *pos, move_t *move, int turn);
void undo_update_accumulator(struct position *pos, move_t *move, int turn);

void do_accumulator(struct position *pos, move_t *move);

void undo_accumulator(struct position *pos, move_t *move);

int32_t evaluate_nnue(struct position *pos);

int32_t evaluate_accumulator(const struct position *pos);

void nnue_init(void);

void file_nnue(const char *path);

void builtin_nnue(void);

void print_nnue_info(void);

#endif
