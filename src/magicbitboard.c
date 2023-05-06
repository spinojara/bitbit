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

#include "magicbitboard.h"

#include <stdio.h>
#include <string.h>
#include <stdalign.h>

#include "bitboard.h"
#include "util.h"
#include "position.h"
#include "init.h"

alignas(64) uint64_t bishop_attacks_lookup[64 * 512];
alignas(64) uint64_t rook_attacks_lookup[64 * 4096];

uint64_t bishop_magic[64];
uint64_t rook_magic[64];

uint64_t bishop_mask[64];
uint64_t rook_mask[64];

uint64_t bishop_full_mask[64];
uint64_t rook_full_mask[64];

uint64_t bishop_attacks_calc(int square, uint64_t b) {
	uint64_t attacks = 0;
	int x = square % 8;
	int y = square / 8;

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
	int x = square % 8;
	int y = square / 8;

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
	int x = square % 8;
	int y = square / 8;

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
	int x = square % 8;
	int y = square / 8;

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
	int x = square % 8;
	int y = square / 8;

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
	int x = square % 8;
	int y = square / 8;

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
		if (i & bitboard(j))
			occ |= bitboard(ctz(attack_mask));

		attack_mask = clear_ls1b(attack_mask);
		j++;
	}

	return occ;
}

uint64_t bishop_magic_calc(int square) {
	int i, j, k, flag;
	uint64_t occ[512];
	uint64_t attacks[512];
	uint64_t used_attacks[512];
	uint64_t attack_mask = bishop_mask_calc(square);

	for (i = 0; i < 512; i++) {
		occ[i] = block_mask(i, attack_mask);
		attacks[i] = bishop_attacks_calc(square, occ[i]);
	}

	for (i = 0; i < 100000000; i++) {
		uint64_t magic_number = rand_uint64() & rand_uint64() & rand_uint64();
		if (popcount((attack_mask * magic_number) & 0xFF00000000000000) < 6)
			continue;

		memset(used_attacks, 0, sizeof(used_attacks));
		
		for (j = 0, flag = 0; !flag && j < 512; j++) {
			k = (occ[j] * magic_number) >> (64 - 9);

			if (!used_attacks[k])
				used_attacks[k] = attacks[j];
			else if (used_attacks[k] != attacks[j])
				flag = 1;
		}

		if (!flag)
			return magic_number;
	}

	return 0;
}

uint64_t rook_magic_calc(int square) {
	int i, j, k, flag;
	uint64_t occ[4096];
	uint64_t attacks[4096];
	uint64_t used_attacks[4096];
	uint64_t attack_mask = rook_mask_calc(square);

	for (i = 0; i < 4096; i++) {
		occ[i] = block_mask(i, attack_mask);
		attacks[i] = rook_attacks_calc(square, occ[i]);
	}

	for (i = 0; i < 100000000; i++) {
		uint64_t magic_number = rand_uint64() & rand_uint64() & rand_uint64();
		if (popcount((attack_mask * magic_number) & 0xFF00000000000000) < 6)
			continue;

		memset(used_attacks, 0, sizeof(used_attacks));
		
		for (j = 0, flag = 0; !flag && j < 4096; j++) {
			k = (occ[j] * magic_number) >> (64 - 12);

			if (!used_attacks[k])
				used_attacks[k] = attacks[j];
			else if (used_attacks[k] != attacks[j])
				flag = 1;
		}

		if (!flag)
			return magic_number;
	}

	return 0;
}

int magicbitboard_init(void) {
	int square, i;
	uint64_t b;
	char str[3];

	for (square = 0; square < 64; square++) {
		bishop_magic[square] = bishop_magic_calc(square);
		if (!bishop_magic[square]) {
			printf("fatal error: no bishop magic found for %s\n", algebraic(str, square));
			return 1;
		}
		init_status("generating bishop magics");
	}
	for (square = 0; square < 64; square++) {
		rook_magic[square] = rook_magic_calc(square);
		if (!rook_magic[square]) {
			printf("fatal error: no rook magic found for %s\n", algebraic(str, square));
			return 1;
		}
		init_status("generating rook magics");
	}
	for (i = 0; i < 64; i++) {
		bishop_mask[i] = bishop_mask_calc(i);
		rook_mask[i] = rook_mask_calc(i);
		bishop_full_mask[i] = bishop_full_mask_calc(i);
		rook_full_mask[i] = rook_full_mask_calc(i);
		init_status("generating attack masks");
	}
	for (square = 0; square < 64; square++) {
		for (i = 0; i < 512; i++) {
			b = block_mask(i, bishop_mask[square]);
			bishop_attacks_lookup[bishop_index(square, b)] = bishop_attacks_calc(square, b);
			init_status("populating bishop attack table");
		}
	}
	for (square = 0; square < 64; square++) {
		for (i = 0; i < 4096; i++) {
			b = block_mask(i, rook_mask[square]);
			rook_attacks_lookup[rook_index(square, b)] = rook_attacks_calc(square, b);
			init_status("populating rook attack table");
		}
	}
	return 0;
}
