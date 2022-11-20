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

#include "attack_gen.h"

#include "init.h"

uint64_t knight_attacks_lookup[64];
uint64_t king_attacks_lookup[64];

uint64_t knight_attacks_calc(int square) {
	uint64_t b = 0;
	uint64_t square_b = bitboard(square);
	b |= shift_north(shift_east(shift_east(square_b)));
	b |= shift_north(shift_north(shift_east(square_b)));
	b |= shift_north(shift_north(shift_west(square_b)));
	b |= shift_north(shift_west(shift_west(square_b)));
	b |= shift_south(shift_west(shift_west(square_b)));
	b |= shift_south(shift_south(shift_west(square_b)));
	b |= shift_south(shift_south(shift_east(square_b)));
	b |= shift_south(shift_east(shift_east(square_b)));
	return b;
}

uint64_t king_attacks_calc(int square) {
	uint64_t b = 0;
	uint64_t square_b = bitboard(square);
	b |= shift_east(square_b);
	b |= shift_north_east(square_b);
	b |= shift_north(square_b);
	b |= shift_north_west(square_b);
	b |= shift_west(square_b);
	b |= shift_south_west(square_b);
	b |= shift_south(square_b);
	b |= shift_south_east(square_b);
	return b;
}

void attack_gen_init(void) {
	for (int i = 0; i < 64; i++) {
		knight_attacks_lookup[i] = knight_attacks_calc(i);
		init_status("populating knight attack table");
	}
	for (int i = 0; i < 64; i++) {
		king_attacks_lookup[i] = king_attacks_calc(i);
		init_status("populating king attack table");
	}
}
