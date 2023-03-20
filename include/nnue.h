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

#include "position.h"
#include "bitboard.h"

#define FV_SCALE (16)

struct dirty {
	int num;
	int pc[3];
	int from[3];
	int to[3];
};

struct index {
	int size;
	uint16_t values[30];
};

struct accumulator {
	int16_t accumulation[2][256];
	int calculated;
};

struct nnue {
	struct accumulator accumulator;
	struct dirty dirty;
};

enum {
	PS_W_PAWN   =  1,
	PS_B_PAWN   =  1 * 64 + 1,
	PS_W_KNIGHT =  2 * 64 + 1,
	PS_B_KNIGHT =  3 * 64 + 1,
	PS_W_BISHOP =  4 * 64 + 1,
	PS_B_BISHOP =  5 * 64 + 1,
	PS_W_ROOK   =  6 * 64 + 1,
	PS_B_ROOK   =  7 * 64 + 1,
	PS_W_QUEEN  =  8 * 64 + 1,
	PS_B_QUEEN  =  9 * 64 + 1,
	PS_END      = 10 * 64 + 1,
};

static uint32_t piece_to_index[2][13] = {
	{ 0, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, 0,
	     PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, 0, },
	{ 0, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, 0,
	     PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, 0, },
};

/* should use horizontal symmetry */
static inline int orient(int turn, int square) {
	return square ^ (turn == 1 ? 0x0 : 0x3F);
}

static inline uint16_t make_index(int turn, int square, int piece, int king_square) {
	return orient(turn, square) + piece_to_index[turn][piece] + PS_END * king_square;
}

static inline void append_active_indices(struct position *pos, struct index *active, int turn) {
	active->size = 0;
	int king_square = ctz(pos->piece[turn][king]);
	king_square = orient(turn, king_square);
	int square, piece;
	for (square = 0; square < 64; square++) {
		piece = pos->mailbox[square];
		if (piece && piece != white_king && piece != black_king)
			active->values[active->size++] = make_index(turn, square, piece, king_square);
	}
}

int nnue_init(int argc, char **argv);

int16_t evaluate_nnue(struct position *pos);

#endif
