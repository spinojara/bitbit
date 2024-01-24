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

#include "attackgen.h"

uint64_t knight_attacks_lookup[64];
uint64_t king_attacks_lookup[64];

uint64_t knight_attacks_calc(int square) {
	uint64_t b = 0;
	uint64_t square_b = bitboard(square);
	b |= shift(shift_twice(square_b, N), E);
	b |= shift(shift_twice(square_b, N), W);
	b |= shift(shift_twice(square_b, S), E);
	b |= shift(shift_twice(square_b, S), W);
	b |= shift(shift_twice(square_b, E), N);
	b |= shift(shift_twice(square_b, E), S);
	b |= shift(shift_twice(square_b, W), N);
	b |= shift(shift_twice(square_b, W), S);
	return b;
}

uint64_t king_attacks_calc(int square) {
	uint64_t b = bitboard(square);
	b |= shift(b, N);
	b |= shift(b, S);
	b |= shift(b, E);
	b |= shift(b, W);
	return b;
}

void attackgen_init(void) {
	for (int i = 0; i < 64; i++) {
		knight_attacks_lookup[i] = knight_attacks_calc(i);
		king_attacks_lookup[i] = king_attacks_calc(i);
	}
}
