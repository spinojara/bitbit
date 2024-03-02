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

#ifndef KPK_H
#define KPK_H

#include "bitbase.h"
#include "bitboard.h"
#include "position.h"

#define BITBASE_KPK_INDEX_MAX (2 * 64 * 24 * 64)
#define BITBASE_KPK_BITS_PER_POSITION (2)
#define BITBASE_KPK_BITS_MASK ((1 << BITBASE_KPK_BITS_PER_POSITION) - 1)
#define BITBASE_KPK_BITS_PER_ENTRY (8 * sizeof(*bitbase_KPK))
#define BITBASE_KPK_POSITIONS_PER_ENTRY (BITBASE_KPK_BITS_PER_ENTRY / BITBASE_KPK_BITS_PER_POSITION)
#define BITBASE_KPK_TABLE_SIZE (BITBASE_KPK_INDEX_MAX * BITBASE_KPK_BITS_PER_POSITION / BITBASE_KPK_BITS_PER_ENTRY)

extern uint32_t bitbase_KPK[];

static inline long bitbase_KPK_index_by_square(int turn, int king_white, int pawn_white, int king_black) {
	int orient = file_of(pawn_white) > 3;
	king_white = orient_vertical(orient, king_white);
	pawn_white = orient_vertical(orient, pawn_white);
	king_black = orient_vertical(orient, king_black);
	return 64 * 24 * 64 * turn
		  + 24 * 64 * king_white
		       + 64 * (file_of(pawn_white) + (rank_of(pawn_white) - 1) * 4)
		            + king_black;
}

static inline long bitbase_KPK_index(const struct position *pos) {
	int white_side = pos->piece[WHITE][PAWN] != 0;
	int black_side = other_color(white_side);
	int turn = pos->turn == white_side;
	int king_white = orient_horizontal(white_side, ctz(pos->piece[white_side][KING]));
	int pawn_white = orient_horizontal(white_side, ctz(pos->piece[white_side][PAWN]));
	int king_black = orient_horizontal(white_side, ctz(pos->piece[black_side][KING]));
	return bitbase_KPK_index_by_square(turn, king_white, pawn_white, king_black);
}

static inline unsigned bitbase_KPK_probe_by_index(long index) {
	long lookup_index = index / BITBASE_KPK_POSITIONS_PER_ENTRY;
	long bit_index = BITBASE_KPK_BITS_PER_POSITION * (index % BITBASE_KPK_POSITIONS_PER_ENTRY);
	return (bitbase_KPK[lookup_index] >> bit_index) & BITBASE_KPK_BITS_MASK;
}

static inline unsigned bitbase_KPK_probe(const struct position *pos, int eval_side) {
	int white_side = pos->piece[WHITE][PAWN] != 0;
	unsigned p = bitbase_KPK_probe_by_index(bitbase_KPK_index(pos));
	return orient_bitbase_eval(white_side != eval_side, p);
}

static inline unsigned bitbase_KPK_probe_by_square(int turn, int king_white, int pawn_white, int king_black) {
	long index = bitbase_KPK_index_by_square(turn, king_white, pawn_white, king_black);
	unsigned p = bitbase_KPK_probe_by_index(index);
	return p;
}

static inline void bitbase_KPK_store_by_index(long index, unsigned eval) {
	long lookup_index = index / BITBASE_KPK_POSITIONS_PER_ENTRY;
	long bit_index = BITBASE_KPK_BITS_PER_POSITION * (index % BITBASE_KPK_POSITIONS_PER_ENTRY);
	bitbase_KPK[lookup_index] &= ~(BITBASE_KPK_BITS_MASK << bit_index);
	bitbase_KPK[lookup_index] |= (eval << bit_index);
}

static inline void bitbase_KPK_store(const struct position *pos, unsigned eval) {
	bitbase_KPK_store_by_index(bitbase_KPK_index(pos), eval);
}

#endif
