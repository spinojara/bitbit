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

#ifndef BITBOARD_H
#define BITBOARD_H

#include <stdint.h>
#include <assert.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

void bitboard_init(void);

static inline uint64_t ctz(uint64_t b) {
	assert(b);
#if defined(__GNUC__)
	return __builtin_ctzll(b);
#elif defined(_MSC_VER)
	unsigned long ret;
	_BitScanForward64(&ret, b);
	return ret;
#endif
}

static inline uint64_t clz(uint64_t b) {
	assert(b);
#if defined(__GNUC__)
	return __builtin_clzll(b);
#elif defined(_MSC_VER)
	unsigned long ret;
	_BitScanReverse(&ret, b);
	return ret;
#endif
}

static inline uint64_t popcount(uint64_t b) {
#if defined(__GNUC__)
	return __builtin_popcountll(b);
#elif defined(_MSC_VER)
	return __popcnt64(b);
#endif
}

static inline uint64_t bitboard(int i) {
	return (uint64_t)1 << i;
}

static inline uint64_t get_bit(uint64_t b, int i) {
	return b & bitboard(i);
}

static inline uint64_t set_bit(uint64_t b, int i) {
	return b | bitboard(i);
}

static inline uint64_t clear_bit(uint64_t b, int i) {
	return b & ~bitboard(i);
}

static inline uint64_t clear_ls1b(uint64_t b) {
	return b & (b - 1);
}

static inline uint64_t ls1b(uint64_t b) {
	return b & -b;
}

static inline uint64_t rotate_bytes(uint64_t b) {
	return (b >> 56) | ((b & 0xFF000000000000) >> 40) | ((b & 0xFF0000000000) >> 24) | ((b & 0xFF00000000) >> 8) |
		((b & 0xFF000000) << 8) | ((b & 0xFF0000) << 24) | ((b & 0xFF00) << 40) | (b << 56);
}

void print_bitboard(uint64_t b);

static inline uint64_t single(uint64_t b) {
	return !(b & (b - 1));
}

static inline uint64_t insert_zero(uint64_t b, int i) {
	return ((b << 1) & ~(bitboard(i + 1) - 1)) | (b & (bitboard(i) - 1));
}

extern uint64_t between_lookup[64 * 64];
extern uint64_t line_lookup[64 * 64];
extern uint64_t ray_lookup[64 * 64];
extern uint64_t file_lookup[64];
extern uint64_t rank_lookup[64];
extern uint64_t file_left_lookup[64];
extern uint64_t file_right_lookup[64];
extern uint64_t adjacent_files_lookup[64];
extern uint64_t same_colored_squares_lookup[64];
extern uint64_t passed_files_lookup[64 * 2];
extern int distance_lookup[64 * 64];
extern int castle_lookup[64 * 64 * 16];
extern uint64_t king_squares_lookup[64 * 2];

static inline uint64_t between(int source_square, int target_square) {
	return between_lookup[source_square + target_square * 64];
}

static inline uint64_t line(int source_square, int target_square) {
	return line_lookup[source_square + target_square * 64];
}

static inline uint64_t ray(int source_square, int target_square) {
	return ray_lookup[source_square + target_square * 64];
}

static inline int distance(int i, int j) {
	return distance_lookup[i + j * 64];
}

static inline uint64_t same_colored_squares(int square) {
	return same_colored_squares_lookup[square];
}

static inline uint64_t file(int square) {
	return file_lookup[square];
}

static inline uint64_t rank(int square) {
	return rank_lookup[square];
}

static inline uint64_t file_left(int square) {
	return file_left_lookup[square];
}

static inline uint64_t file_right(int square) {
	return file_right_lookup[square];
}

static inline uint64_t adjacent_files(int square) {
	return adjacent_files_lookup[square];
}

static inline uint64_t passed_files(int square, int color) {
	return passed_files_lookup[square + 64 * (1 - color)];
}

static inline int castle(int source_square, int target_square, int castle) {
	return castle_lookup[source_square + 64 * target_square + 64 * 64 * castle];
}

static inline uint64_t king_squares(int square, int color) {
	return king_squares_lookup[square + 64 * (1 - color)];
}

extern const uint64_t FILE_H;
extern const uint64_t FILE_G;
extern const uint64_t FILE_F;
extern const uint64_t FILE_E;
extern const uint64_t FILE_D;
extern const uint64_t FILE_C;
extern const uint64_t FILE_B;
extern const uint64_t FILE_A;
extern const uint64_t FILE_AB;
extern const uint64_t FILE_GH;
extern const uint64_t RANK_8;
extern const uint64_t RANK_7;
extern const uint64_t RANK_6;
extern const uint64_t RANK_5;
extern const uint64_t RANK_4;
extern const uint64_t RANK_3;
extern const uint64_t RANK_2;
extern const uint64_t RANK_1;

static inline uint64_t shift_north(uint64_t b) {
	return b << 8;
}
static inline uint64_t shift_south(uint64_t b) {
	return b >> 8;
}
static inline uint64_t shift_west(uint64_t b) {
	return (b >> 1) & ~FILE_H;
}
static inline uint64_t shift_east(uint64_t b) {
	return (b << 1) & ~FILE_A;
}
static inline uint64_t shift_north_east(uint64_t b) {
	return (b << 9) & ~FILE_A;
}
static inline uint64_t shift_north_west(uint64_t b) {
	return (b << 7) & ~FILE_H;
}
static inline uint64_t shift_south_west(uint64_t b) {
	return (b >> 9) & ~FILE_H;
}
static inline uint64_t shift_south_east(uint64_t b) {
	return (b >> 7) & ~FILE_A;
}
static inline uint64_t shift_north_north(uint64_t b) {
	return b << 16;
}
static inline uint64_t shift_south_south(uint64_t b) {
	return b >> 16;
}
static inline uint64_t shift_color(uint64_t b, int color) {
	return color ? shift_north(b) : shift_south(b);
}
static inline uint64_t shift_color2(uint64_t b, int color) {
	return color ? shift_north_north(b) : shift_south_south(b);
}
static inline uint64_t shift_color_east(uint64_t b, int color) {
	return color ? shift_north_east(b) : shift_south_east(b);
}
static inline uint64_t shift_color_west(uint64_t b, int color) {
	return color ? shift_north_west(b) : shift_south_west(b);
}

static inline uint64_t fill_north(uint64_t b) {
	for (int i = 0; i < 8; i++)
		b |= shift_north(b);
	return b;
}
static inline uint64_t fill_south(uint64_t b) {
	for (int i = 0; i < 8; i++)
		b |= shift_south(b);
	return b;
}

#endif
