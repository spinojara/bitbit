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

#ifndef BITBASE_H
#define BITBASE_H

#include <stdint.h>

#include "nnue.h"

#define BITBASE_INDEX_MAX (2 * 48 * 64 * 64)
#define BITBASE_PER_ENTRY (64)
#define BITBASE_TABLE_SIZE (BITBASE_INDEX_MAX / BITBASE_PER_ENTRY)

enum {
	BITBASE_DRAW    = 0,
	BITBASE_WIN     = 1,
	BITBASE_UNKNOWN = 2,
};

extern uint64_t bitbase[BITBASE_TABLE_SIZE];

static inline int bitbase_index(const struct position *pos) {
	int strongside = pos->piece[white][pawn] != 0;
	int weakside = 1 - strongside;
	int pawn_square = orient(strongside, ctz(pos->piece[strongside][pawn]));
	int strong_king = orient(strongside, ctz(pos->piece[strongside][king]));
	int weak_king = orient(strongside, ctz(pos->piece[weakside][king]));
	
	return 64 * 64 * 48 * pos->turn + 64 * 64 * (pawn_square - 8) + 64 * strong_king + weak_king;
}

static inline int bitbase_probe(const struct position *pos) {
	int index = bitbase_index(pos);

	int lookup_index = index / BITBASE_PER_ENTRY;
	int bit_index = index % BITBASE_PER_ENTRY;

	return get_bit(bitbase[lookup_index], bit_index);
}

static inline void bitbase_store(const struct position *pos, int win) {
	int index = bitbase_index(pos);

	int lookup_index = index / BITBASE_PER_ENTRY;
	int bit_index = index % BITBASE_PER_ENTRY;

	if (win)
		bitbase[lookup_index] = set_bit(bitbase[lookup_index], bit_index);
	else
		bitbase[lookup_index] = clear_bit(bitbase[lookup_index], bit_index);
}

#endif
