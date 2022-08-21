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

#include "bitboard.h"

#include <stdio.h>

#include "magic_bitboard.h"
#include "attack_gen.h"
#include "init.h"

uint64_t between_lookup[64 * 64];
uint64_t line_lookup[64 * 64];
int castle_lookup[64 * 64 * 16];

void print_bitboard(uint64_t b) {
	printf("\n       a   b   c   d   e   f   g   h\n");
	for (int i = 0; i < 8; i++) {
		printf("     +---+---+---+---+---+---+---+---+\n   %i |", 8 - i);
		for (int j = 0; j < 8; j++) {
			if (get_bit(b, 8 * (7 - i) + j)) {
				printf(" 1 |");
			}
			else {
				printf(" 0 |");
			}
		}
		printf(" %i\n", 8 - i);
	}
	printf("     +---+---+---+---+---+---+---+---+\n");
	printf("       a   b   c   d   e   f   g   h\n\n");
}

void print_binary(uint64_t b) {
	for (int i = 0; i < 64; i++)
		printf("%i", get_bit(b, 63 - i) ? 1 : 0);
}

uint64_t between_calc(int x, int y) {
	int a_x = x % 8;
	int b_x = x / 8;
	int a_y = y % 8;
	int b_y = y / 8;
	if (a_x == a_y || b_x == b_y) {
		return rook_attacks(x, bitboard(y)) & rook_attacks(y, bitboard(x));
	}
	else if (a_x - a_y == b_x - b_y || a_x - a_y == b_y - b_x) {
		return bishop_attacks(x, bitboard(y)) & bishop_attacks(y, bitboard(x));
	}
	return 0;
}

uint64_t line_calc(int x, int y) {
	if (x % 8 == y % 8 || x / 8 == y / 8) {
		return rook_full_mask[x] & rook_full_mask[y];
	}
	else {
		return bishop_full_mask[x] & bishop_full_mask[y];
	}
}

int castle_calc(int source_square, int target_square, int castle) {
	if (source_square == 0) {
		castle = clear_bit(castle, 1);
	}
	else if (source_square == 4) {
		castle = clear_bit(castle, 0);
		castle = clear_bit(castle, 1);
	}
	else if (source_square == 7) {
		castle = clear_bit(castle, 0);
	}
	else if (source_square == 56) {
		castle = clear_bit(castle, 3);
	}
	else if (source_square == 60) {
		castle = clear_bit(castle, 2);
		castle = clear_bit(castle, 3);
	}
	else if (source_square == 63) {
		castle = clear_bit(castle, 2);
	}
	if (target_square == 0) {
		castle = clear_bit(castle, 1);
	}
	else if (target_square == 7) {
		castle = clear_bit(castle, 0);
	}
	else if (target_square == 56) {
		castle = clear_bit(castle, 3);
	}
	else if (target_square == 63) {
		castle = clear_bit(castle, 2);
	}
	return castle;
}

void bitboard_init() {
	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 64; j++) {
			between_lookup[i + 64 * j] = between_calc(i, j);
			line_lookup[i + 64 * j] = line_calc(i, j);
			init_status("populating bitboard lookup table");
		}
	}

	for (int source_square = 0; source_square < 64; source_square++) {
		for (int target_square = 0; target_square < 64; target_square++) {
			for (int castle = 0; castle < 16; castle++) {
				castle_lookup[source_square + 64 * target_square + 64 * 64 * castle] = castle_calc(source_square, target_square, castle);
				init_status("populating castling lookup table");
			}
		}
	}
}

uint64_t file_calc(int square) {
	int x = square % 8;
	uint64_t ret = FILE_A;

	for (int i = 0; i < x; i++)
		ret = shift_east(ret);
	return ret;
}

uint64_t rank_calc(int square) {
	int y = square / 8;
	uint64_t ret = RANK_1;

	for (int i = 0; i < y; i++)
		ret = shift_north(ret);
	return ret;
}

const uint64_t FILE_H = 0x8080808080808080;
const uint64_t FILE_G = 0x4040404040404040;
const uint64_t FILE_F = 0x2020202020202020;
const uint64_t FILE_E = 0x1010101010101010;
const uint64_t FILE_D = 0x808080808080808;
const uint64_t FILE_C = 0x404040404040404;
const uint64_t FILE_B = 0x202020202020202;
const uint64_t FILE_A = 0x101010101010101;
const uint64_t FILE_AB = 0x303030303030303;
const uint64_t FILE_GH = 0xC0C0C0C0C0C0C0C0;
const uint64_t RANK_8 = 0xFF00000000000000;
const uint64_t RANK_7 = 0xFF000000000000;
const uint64_t RANK_6 = 0xFF0000000000;
const uint64_t RANK_5 = 0xFF00000000;
const uint64_t RANK_4 = 0xFF000000;
const uint64_t RANK_3 = 0xFF0000;
const uint64_t RANK_2 = 0xFF00;
const uint64_t RANK_1 = 0xFF;
