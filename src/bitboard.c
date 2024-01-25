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

#include "bitboard.h"

#include <stdio.h>

#include "magicbitboard.h"
#include "attackgen.h"
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

uint64_t between_calc(int source, int target) {
	if (source == target)
		return 0;
	int sourcef = file_of(source);
	int sourcer = rank_of(source);
	int targetf = file_of(target);
	int targetr = rank_of(target);
	if (sourcef == targetf || sourcer == targetr)
		return rook_attacks(source, 0, bitboard(target)) & rook_attacks(target, 0, bitboard(source));
	else if (sourcef - targetf == sourcer - targetr || sourcef - targetf == targetr - sourcer)
		return bishop_attacks(source, 0, bitboard(target)) & bishop_attacks(target, 0, bitboard(source));
	return 0;
}

uint64_t line_calc(int source, int target) {
	if (source == target)
		return 0;
	if (file_of(source) == file_of(target) || rank_of(source) == rank_of(target)) {
		return rook_full_mask[source] & rook_full_mask[target];
	}
	else {
		return bishop_full_mask[source] & bishop_full_mask[target];
	}
}

uint64_t ray_calc(int source, int target) {
	uint64_t ret = 0;

	int f = file_of(source);
	int r = rank_of(source);

	int vf = file_of(target) - file_of(source);
	int vr = rank_of(target) - rank_of(source);

	if (source == target)
		return ret;

	if (vf != 0 && vr != 0 && vf != vr && vf != -vr)
		return ret;

	if (vf)
		vf /= abs(vf);
	if (vr)
		vr /= abs(vr);

	while (f <= 7 && f >= 0 && r <= 7 && r >= 0) {
		ret |= bitboard(f + 8 * r);
		f += vf;
		r += vr;
	}
	return ret;
}

int castle_calc(int source, int target, int castle) {
	if (source == 0) {
		castle = clear_bit(castle, 1);
	}
	else if (source == 4) {
		castle = clear_bit(castle, 0);
		castle = clear_bit(castle, 1);
	}
	else if (source == 7) {
		castle = clear_bit(castle, 0);
	}
	else if (source == 56) {
		castle = clear_bit(castle, 3);
	}
	else if (source == 60) {
		castle = clear_bit(castle, 2);
		castle = clear_bit(castle, 3);
	}
	else if (source == 63) {
		castle = clear_bit(castle, 2);
	}
	if (target == 0) {
		castle = clear_bit(castle, 1);
	}
	else if (target == 7) {
		castle = clear_bit(castle, 0);
	}
	else if (target == 56) {
		castle = clear_bit(castle, 3);
	}
	else if (target == 63) {
		castle = clear_bit(castle, 2);
	}
	return castle;
}

uint64_t file_calc(int square) {
	int f = file_of(square);
	uint64_t ret = FILE_A;

	for (int i = 0; i < f; i++)
		ret = shift(ret, E);
	return ret;
}

uint64_t rank_calc(int square) {
	int r = rank_of(square);
	uint64_t ret = RANK_1;

	for (int i = 0; i < r; i++)
		ret = shift(ret, N);
	return ret;
}

uint64_t file_left_calc(int square) {
	int f = file_of(square);
	if (f == 0)
		return 0;
	return file_calc(square - 1);
}

uint64_t file_right_calc(int square) {
	int f = file_of(square);
	if (f == 7)
		return 0;
	return file_calc(square + 1);
}

uint64_t adjacent_files_calc(int square) {
	uint64_t ret = 0;
	int f = file_of(square);
	
	if (f > 0)
		ret |= file_left_calc(square);
	if (f < 7)
		ret |= file_right_calc(square);
	return ret;
}

uint64_t passed_files_calc(int square, int color) {
	uint64_t ret = 0;
	int f = file_of(square);
	
	if (color) {
		if (f > 0)
			ret |= between_calc(square - 1, 55 + f);
		ret |= between_calc(square, 56 + f);
		if (f < 7)
			ret |= between_calc(square + 1, 57 + f);
	}
	else {
		if (f > 0)
			ret |= between_calc(square - 1, f - 1);
		ret |= between_calc(square, f);
		if (f < 7)
			ret |= between_calc(square + 1, f + 1);
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
	return max(abs(file_of(a) - file_of(b)), abs(rank_of(a) - rank_of(b)));
}

void bitboard_init(void) {
	for (int i = 0; i < 64; i++) {
		file_lookup[i] = file_calc(i);
		rank_lookup[i] = rank_calc(i);
		file_left_lookup[i] = file_left_calc(i);
		file_right_lookup[i] = file_right_calc(i);
		adjacent_files_lookup[i] = adjacent_files_calc(i);
		passed_files_lookup[i] = passed_files_calc(i, WHITE);
		passed_files_lookup[i + 64] = passed_files_calc(i, BLACK);
		same_colored_squares_lookup[i] = same_colored_squares_calc(i);
		for (int j = 0; j < 64; j++) {
			distance_lookup[i + 64 * j] = distance_calc(i, j);
			between_lookup[i + 64 * j] = between_calc(i, j);
			line_lookup[i + 64 * j] = line_calc(i, j);
			ray_lookup[i + 64 * j] = ray_calc(i, j);
		}
	}

	for (int source_square = 0; source_square < 64; source_square++) {
		for (int target_square = 0; target_square < 64; target_square++) {
			for (int castle = 0; castle < 16; castle++) {
				castle_lookup[source_square + 64 * target_square + 64 * 64 * castle] = castle_calc(source_square, target_square, castle);
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
