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

#ifndef KPKP_H
#define KPKP_H

#include "bitbase.h"
#include "bitboard.h"
#include "position.h"

#define BITBASE_KPKP_INDEX_MAX (64 * 24 * 64 * 48)
#define BITBASE_KPKP_BITS_PER_POSITION (2)
#define BITBASE_KPKP_BITS_MASK ((1 << BITBASE_KPKP_BITS_PER_POSITION) - 1)
#define BITBASE_KPKP_BITS_PER_ENTRY (8 * sizeof(*bitbase_KPKP))
#define BITBASE_KPKP_POSITIONS_PER_ENTRY (BITBASE_KPKP_BITS_PER_ENTRY / BITBASE_KPKP_BITS_PER_POSITION)
#define BITBASE_KPKP_TABLE_SIZE (BITBASE_KPKP_INDEX_MAX * BITBASE_KPKP_BITS_PER_POSITION / BITBASE_KPKP_BITS_PER_ENTRY)

extern uint32_t bitbase_KPKP[];

static inline long bitbase_KPKP_index_by_square(int king_white, int pawn_white, int king_black, int pawn_black) {
	int orient = file_of(pawn_white) > 3;
	king_white = orient_vertical(orient, king_white);
	pawn_white = orient_vertical(orient, pawn_white);
	king_black = orient_vertical(orient, king_black);
	pawn_black = orient_vertical(orient, pawn_black);
	return 24 * 64 * 48 * king_white
		  + 64 * 48 * (file_of(pawn_white) + (rank_of(pawn_white) - 1) * 4)
		       + 48 * king_black
			    + (pawn_black - 8);
}

static inline long bitbase_KPKP_index(const struct position *pos) {
	int turn = pos->turn;
	int king_white = orient_horizontal(turn, ctz(pos->piece[turn][KING]));
	int pawn_white = orient_horizontal(turn, ctz(pos->piece[turn][PAWN]));
	int king_black = orient_horizontal(turn, ctz(pos->piece[other_color(turn)][KING]));
	int pawn_black = orient_horizontal(turn, ctz(pos->piece[other_color(turn)][PAWN]));
	return bitbase_KPKP_index_by_square(king_white, pawn_white, king_black, pawn_black);
}

static inline unsigned bitbase_KPKP_probe_by_index(long index) {
	long lookup_index = index / BITBASE_KPKP_POSITIONS_PER_ENTRY;
	long bit_index = BITBASE_KPKP_BITS_PER_POSITION * (index % BITBASE_KPKP_POSITIONS_PER_ENTRY);
	return (bitbase_KPKP[lookup_index] >> bit_index) & BITBASE_KPKP_BITS_MASK;
}

static inline unsigned bitbase_KPKP_probe(const struct position *pos, int eval_side) {
	int white_side = pos->turn;
	unsigned p = bitbase_KPKP_probe_by_index(bitbase_KPKP_index(pos));
	return orient_bitbase_eval(white_side != eval_side, p);
}

static inline void bitbase_KPKP_store_by_index(long index, unsigned eval) {
	long lookup_index = index / BITBASE_KPKP_POSITIONS_PER_ENTRY;
	long bit_index = BITBASE_KPKP_BITS_PER_POSITION * (index % BITBASE_KPKP_POSITIONS_PER_ENTRY);
	bitbase_KPKP[lookup_index] &= ~(BITBASE_KPKP_BITS_MASK << bit_index);
	bitbase_KPKP[lookup_index] |= (eval << bit_index);
}

static inline void bitbase_KPKP_store(const struct position *pos, unsigned eval) {
	bitbase_KPKP_store_by_index(bitbase_KPKP_index(pos), eval);
}

#endif
