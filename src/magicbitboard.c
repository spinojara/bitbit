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

#include "magicbitboard.h"

#include <stdio.h>
#include <string.h>
#include <stdalign.h>

#include "bitboard.h"
#include "util.h"
#include "position.h"

uint64_t bishop_attacks_lookup[5248];
uint64_t rook_attacks_lookup[102400];

struct magic bishop_magic[64];
struct magic rook_magic[64];

uint64_t bishop_attacks_calc(int square, uint64_t b) {
	uint64_t attacks = 0;
	int x = file_of(square);
	int y = rank_of(square);

	for (int i = x + 1, j = y + 1; i < 8 && j < 8; i++, j++) {
		attacks |= bitboard(i + j * 8);
		if (b & bitboard(i + j * 8)) {
			break;
		}
	}

	for (int i = x - 1, j = y + 1; -1 < i && j < 8; i--, j++) {
		attacks |= bitboard(i + j * 8);
		if (b & bitboard(i + j * 8)) {
			break;
		}
	}

	for (int i = x + 1, j = y - 1; i < 8 && -1 < j; i++, j--) {
		attacks |= bitboard(i + j * 8);
		if (b & bitboard(i + j * 8)) {
			break;
		}
	}

	for (int i = x - 1, j = y - 1; -1 < i && -1 < j; i--, j--) {
		attacks |= bitboard(i + j * 8);
		if (b & bitboard(i + j * 8)) {
			break;
		}
	}

	return attacks;
}

uint64_t rook_attacks_calc(int square, uint64_t b) {
	uint64_t attacks = 0;
	int x = file_of(square);
	int y = rank_of(square);

	for (int i = x + 1; i < 8; i++) {
		attacks |= bitboard(i + 8 * y);
		if (b & bitboard(i + 8 * y)) {
			break;
		}
	}

	for (int i = x - 1; i > -1; i--) {
		attacks |= bitboard(i + 8 * y);
		if (b & bitboard(i + 8 * y)) {
			break;
		}
	}

	for (int i = y + 1;i < 8;i++) {
		attacks |= bitboard(x + 8 * i);
		if (b & bitboard(x + 8 * i)) {
			break;
		}
	}

	for (int i = y - 1;i > -1;i--) {
		attacks |= bitboard(x + 8 * i);
		if (b & bitboard(x + 8 * i)) {
			break;
		}
	}

	return attacks;
}

uint64_t bishop_mask_calc(int square) {
	uint64_t mask = 0;
	int x = file_of(square);
	int y = rank_of(square);

	for (int i = x + 1, j = y + 1; i < 7 && j < 7; i++, j++) {
		mask |= bitboard(i + j * 8);
	}

	for (int i = x - 1, j = y + 1; 0 < i && j < 7; i--, j++) {
		mask |= bitboard(i + j * 8);
	}

	for (int i = x + 1, j = y - 1; i < 7 && 0 < j; i++, j--) {
		mask |= bitboard(i + j * 8);
	}

	for (int i = x - 1, j = y - 1; 0 < i && 0 < j; i--, j--) {
		mask |= bitboard(i + j * 8);
	}

	return mask;
}

uint64_t rook_mask_calc(int square) {
	uint64_t mask = 0;
	int x = file_of(square);
	int y = rank_of(square);

	for (int i = x + 1; i < 7; i++) {
		mask |= bitboard(i + 8 * y);
	}

	for (int i = x - 1; i > 0; i--) {
		mask |= bitboard(i + 8 * y);
	}

	for (int i = y + 1; i < 7; i++) {
		mask |= bitboard(x + 8 * i);
	}

	for (int i = y - 1; i > 0; i--) {
		mask |= bitboard(x + 8 * i);
	}

	return mask;
}

uint64_t bishop_full_mask_calc(int square) {
	uint64_t mask = 0;
	int x = file_of(square);
	int y = rank_of(square);

	for (int i = x + 1, j = y + 1; i < 8 && j < 8; i++, j++) {
		mask |= bitboard(i + j * 8);
	}

	for (int i = x - 1, j = y + 1; -1 < i && j < 8; i--, j++) {
		mask |= bitboard(i + j * 8);
	}

	for (int i = x + 1, j = y - 1; i < 8 && -1 < j; i++, j--) {
		mask |= bitboard(i + j * 8);
	}

	for (int i = x - 1, j = y - 1; -1 < i && -1 < j; i--, j--) {
		mask |= bitboard(i + j * 8);
	}

	return mask;
}

uint64_t rook_full_mask_calc(int square) {
	uint64_t mask = 0;
	int x = file_of(square);
	int y = rank_of(square);

	for (int i = x + 1; i < 8; i++) {
		mask |= bitboard(i + 8 * y);
	}

	for (int i = x - 1; i > -1; i--) {
		mask |= bitboard(i + 8 * y);
	}

	for (int i = y + 1; i < 8; i++) {
		mask |= bitboard(x + 8 * i);
	}

	for (int i = y - 1; i > -1; i--) {
		mask |= bitboard(x + 8 * i);
	}

	return mask;
}

uint64_t block_mask(int i, uint64_t attack_mask) {
	uint64_t occ = 0;
	int j = 0;

	while (attack_mask) {
		if (i & bitboard(j++))
			occ |= bitboard(ctz(attack_mask));

		attack_mask = clear_ls1b(attack_mask);
	}

	return occ;
}

void magic_calc(int square, int piece) {
	struct magic *magic = piece == ROOK ? &rook_magic[square] : &bishop_magic[square];
	uint64_t occ[4096];
	uint64_t attacks[4096];
	int epochs[4096] = { 0 };

	magic->mask = piece == ROOK ? rook_mask_calc(square) : bishop_mask_calc(square);
	magic->shift = 64 - popcount(magic->mask);

	uint64_t seeds[2] = { 5273, 23293 };

	uint64_t seed = seeds[piece == ROOK];

	if (square == a1)
		magic->attacks = piece == ROOK ? rook_attacks_lookup : bishop_attacks_lookup;

	/* <https://www.chessprogramming.org/Traversing_Subsets_of_a_Set> */
	uint64_t b = 0;
	int size = 0;
	do {
		occ[size] = b;
		attacks[size] = piece == ROOK ? rook_attacks_calc(square, b) : bishop_attacks_calc(square, b);

#ifdef PEXT
		magic->attacks[_pext_u64(b, magic->mask)] = attacks[size];
#endif

		b = (b - magic->mask) & magic->mask;
		size++;
	}
	while (b);

	if (square < h8)
		(magic + 1)->attacks = magic->attacks + size;

#ifdef PEXT
	return;
#endif

	int epoch, j = 0;
	for (epoch = 0; j != size; epoch++) {
		magic->magic = xorshift64(&seed) & xorshift64(&seed) & xorshift64(&seed);

		if (popcount((magic->mask * magic->magic) >> 56) < 6)
			continue;

		for (j = 0; j < size; j++) {
			int k = (occ[j] * magic->magic) >> magic->shift;

			if (epochs[k] < epoch) {
				epochs[k] = epoch;
				magic->attacks[k] = attacks[j];
			}
			else if (magic->attacks[k] != attacks[j])
				break;
		}
	}
}

void magicbitboard_init(void) {
	for (int square = 0; square < 64; square++) {
		magic_calc(square, BISHOP);
		magic_calc(square, ROOK);
	}
}
