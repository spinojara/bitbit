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

#include "magicbitboard.h"
#include "attackgen.h"
#include "init.h"
#include "position.h"
#include "util.h"

uint64_t between_lookup[64 * 64];
uint64_t line_lookup[64 * 64];
uint64_t ray_lookup[64 * 64];
uint64_t file_lookup[64];
uint64_t rank_lookup[64];
uint64_t file_left_lookup[64];
uint64_t file_right_lookup[64];
uint64_t same_colored_squares_lookup[64];
uint64_t adjacent_files_lookup[64];
int distance_lookup[64 * 64];
uint64_t passed_files_lookup[64 * 2];
int castle_lookup[64 * 64 * 16];
uint64_t king_squares_lookup[64 * 2];

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

uint64_t between_calc(int x, int y) {
	if (x == y)
		return 0;
	int a_x = x % 8;
	int b_x = x / 8;
	int a_y = y % 8;
	int b_y = y / 8;
	if (a_x == a_y || b_x == b_y) {
		return rook_attacks(x, 0, bitboard(y)) & rook_attacks(y, 0, bitboard(x));
	}
	else if (a_x - a_y == b_x - b_y || a_x - a_y == b_y - b_x) {
		return bishop_attacks(x, 0, bitboard(y)) & bishop_attacks(y, 0, bitboard(x));
	}
	return 0;
}

uint64_t line_calc(int x, int y) {
	if (x == y)
		return 0;
	if (x % 8 == y % 8 || x / 8 == y / 8) {
		return rook_full_mask[x] & rook_full_mask[y];
	}
	else {
		return bishop_full_mask[x] & bishop_full_mask[y];
	}
}

uint64_t ray_calc(int source, int target) {
	uint64_t ret = 0;

	int x = source % 8;
	int y = source / 8;

	int v_x = (target % 8) - (source % 8);
	int v_y = (target / 8) - (source / 8);

	if (source == target)
		return ret;

	if (v_x != 0 && v_y != 0 && v_x != v_y && v_x != -v_y)
		return ret;

	if (v_x)
		v_x /= ABS(v_x);
	if (v_y)
		v_y /= ABS(v_y);

	while (x <= 7 && x >= 0 && y <= 7 && y >= 0) {
		ret |= bitboard(x + 8 * y);
		x += v_x;
		y += v_y;
	}
	return ret;
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

uint64_t file_left_calc(int square) {
	int x = square % 8;
	if (x == 0)
		return 0;
	return file_calc(square - 1);
}

uint64_t file_right_calc(int square) {
	int x = square % 8;
	if (x == 7)
		return 0;
	return file_calc(square + 1);
}

uint64_t adjacent_files_calc(int square) {
	uint64_t r = 0;
	int x = square % 8;
	
	if (x > 0)
		r |= line_calc(x - 1, x + 7) | bitboard(x - 1) | bitboard(x + 7);
	if (x < 7)
		r |= line_calc(x + 1, x + 9) | bitboard(x + 1) | bitboard(x + 9);
	return r;
}

uint64_t passed_files_calc(int square, int color) {
	uint64_t r = 0;
	int x = square % 8;
	
	if (color) {
		if (x > 0)
			r |= between_calc(square - 1, 55 + x);
		r |= between_calc(square, 56 + x);
		if (x < 7)
			r |= between_calc(square + 1, 57 + x);
	}
	else {
		if (x > 0)
			r |= between_calc(square - 1, x - 1);
		r |= between_calc(square, x);
		if (x < 7)
			r |= between_calc(square + 1, x + 1);
	}
	return r;
}

uint64_t king_squares_calc(int square, int turn) {
	uint64_t attacks = king_attacks(square, 0) | bitboard(square);
	uint64_t ret = attacks;
	ret |= shift_west(ret) | shift_east(ret);
	if (turn) {
		ret |= shift_north(shift_north(ret));
		for (int i = 0; i < 3; i++)
			attacks |= shift_north(attacks);
		ret |= attacks;
	}
	else {
		ret |= shift_south(shift_south(ret));
		for (int i = 0; i < 3; i++)
			attacks |= shift_south(attacks);
		ret |= attacks;
	}
	return ret;
}

uint64_t same_colored_squares_calc(int square) {
	uint64_t b = 0;
	for (int i = 0; i < 32; i++)
		b |= bitboard(2 * i + ((i / 4) % 2 ? 1 : 0));
	return (square + (square / 8)) % 2 ? ~b : b;
}

uint64_t distance_calc(int a, int b) {
	return MAX(ABS((a % 8) - (b % 8)), ABS((a / 8) - (b / 8)));
}

void bitboard_init(void) {
	for (int i = 0; i < 64; i++) {
		file_lookup[i] = file_calc(i);
		rank_lookup[i] = rank_calc(i);
		file_left_lookup[i] = file_left_calc(i);
		file_right_lookup[i] = file_right_calc(i);
		adjacent_files_lookup[i] = adjacent_files_calc(i);
		passed_files_lookup[i] = passed_files_calc(i, white);
		passed_files_lookup[i + 64] = passed_files_calc(i, black);
		king_squares_lookup[i] = king_squares_calc(i, white);
		king_squares_lookup[i + 64] = king_squares_calc(i, black);
		same_colored_squares_lookup[i] = same_colored_squares_calc(i);
		for (int j = 0; j < 64; j++) {
			distance_lookup[i + 64 * j] = distance_calc(i, j);
			between_lookup[i + 64 * j] = between_calc(i, j);
			line_lookup[i + 64 * j] = line_calc(i, j);
			ray_lookup[i + 64 * j] = ray_calc(i, j);
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
