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

#ifndef KRKP_H
#define KRKP_H

#include "bitboard.h"
#include "position.h"

#define BITBASE_KRKP_INDEX_MAX (2 * 64 * 64 * 64 * 24)
#define BITBASE_KRKP_BITS_PER_POSITION (2)
#define BITBASE_KRKP_BITS_MASK ((1 << BITBASE_KRKP_BITS_PER_POSITION) - 1)
#define BITBASE_KRKP_BITS_PER_ENTRY (8 * sizeof(*bitbase_KRKP))
#define BITBASE_KRKP_POSITIONS_PER_ENTRY (BITBASE_KRKP_BITS_PER_ENTRY / BITBASE_KRKP_BITS_PER_POSITION)
#define BITBASE_KRKP_TABLE_SIZE (BITBASE_KRKP_INDEX_MAX * BITBASE_KRKP_BITS_PER_POSITION / BITBASE_KRKP_BITS_PER_ENTRY)

extern uint32_t bitbase_KRKP[];

static inline long bitbase_KRKP_index_by_square(int turn, int king_white, int rook_white, int king_black, int pawn_black) {
	int orient = file_of(pawn_black) > 3;
	king_white = orient_vertical(orient, king_white);
	rook_white = orient_vertical(orient, rook_white);
	king_black = orient_vertical(orient, king_black);
	pawn_black = orient_vertical(orient, pawn_black);
	return 64 * 64 * 64 * 24 * turn
		  + 64 * 64 * 24 * king_white
		       + 64 * 24 * rook_white
		            + 24 * king_black
			         + (file_of(pawn_black) + (rank_of(pawn_black) - 1) * 4);
}

static inline long bitbase_KRKP_index(const struct position *pos) {
	int white_side = pos->piece[WHITE][ROOK] != 0;
	int black_side = other_color(white_side);
	int turn = pos->turn == white_side;
	int king_white = orient_horizontal(white_side, ctz(pos->piece[white_side][KING]));
	int rook_white = orient_horizontal(white_side, ctz(pos->piece[white_side][ROOK]));
	int king_black = orient_horizontal(white_side, ctz(pos->piece[black_side][KING]));
	int pawn_black = orient_horizontal(white_side, ctz(pos->piece[black_side][PAWN]));
	return bitbase_KRKP_index_by_square(turn, king_white, rook_white, king_black, pawn_black);
}

static inline unsigned bitbase_KRKP_probe_by_index(long index) {
	long lookup_index = index / BITBASE_KRKP_POSITIONS_PER_ENTRY;
	long bit_index = BITBASE_KRKP_BITS_PER_POSITION * (index % BITBASE_KRKP_POSITIONS_PER_ENTRY);
	return (bitbase_KRKP[lookup_index] >> bit_index) & BITBASE_KRKP_BITS_MASK;
}

static inline unsigned bitbase_KRKP_probe(const struct position *pos, int eval_side) {
	int white_side = pos->piece[WHITE][ROOK] != 0;
	unsigned p = bitbase_KRKP_probe_by_index(bitbase_KRKP_index(pos));
	return orient_bitbase_eval(white_side != eval_side, p);
}

static inline void bitbase_KRKP_store_by_index(long index, unsigned eval) {
	long lookup_index = index / BITBASE_KRKP_POSITIONS_PER_ENTRY;
	long bit_index = BITBASE_KRKP_BITS_PER_POSITION * (index % BITBASE_KRKP_POSITIONS_PER_ENTRY);
	bitbase_KRKP[lookup_index] &= ~(BITBASE_KRKP_BITS_MASK << bit_index);
	bitbase_KRKP[lookup_index] |= (eval << bit_index);
}

static inline void bitbase_KRKP_store(const struct position *pos, unsigned eval) {
	bitbase_KRKP_store_by_index(bitbase_KRKP_index(pos), eval);
}

#endif
