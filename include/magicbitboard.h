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

#ifndef MAGIC_BITBOARD_H
#define MAGIC_BITBOARD_H

#include <stdint.h>

#ifdef PEXT
#include <immintrin.h>
#endif

void magicbitboard_init(void);

struct magic {
	uint64_t *attacks;
	uint64_t mask;
	uint64_t magic;
	unsigned shift;
};

extern struct magic bishop_magic[64];
extern struct magic rook_magic[64];

static inline uint64_t magic_index(const struct magic *magic, uint64_t b) {
#ifdef PEXT
	return _pext_u64(b, magic->mask);
#else
	return ((b & magic->mask) * magic->magic) >> magic->shift;
#endif
}

static inline uint64_t bishop_attacks_pre(int square, uint64_t b) {
	const struct magic *magic = &bishop_magic[square];
	return magic->attacks[magic_index(magic, b)];
}

static inline uint64_t rook_attacks_pre(int square, uint64_t b) {
	const struct magic *magic = &rook_magic[square];
	return magic->attacks[magic_index(magic, b)];
}

uint64_t bishop_full_mask_calc(int square);
uint64_t rook_full_mask_calc(int square);

#endif
