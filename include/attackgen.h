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

#ifndef ATTACK_GEN_H
#define ATTACK_GEN_H

#include <stdint.h>

#include "bitboard.h"
#include "position.h"
#include "magicbitboard.h"

#ifndef NDEBUG
extern int attackgen_init_done;
#endif

void attackgen_init(void);

extern uint64_t knight_attacks_lookup[64];
extern uint64_t king_attacks_lookup[64];

static inline uint64_t pawn_capture_e(uint64_t pawns, uint64_t enemy, int color) {
	unsigned down = color ? S : N;
	return pawns & shift(enemy, down | W);
}
static inline uint64_t pawn_capture_w(uint64_t pawns, uint64_t enemy, int color) {
	unsigned down = color ? S : N;
	return pawns & shift(enemy, down | E);
}
static inline uint64_t pawn_push(uint64_t pawns, uint64_t all, int color) {
	unsigned down = color ? S : N;
	return pawns & ~shift(all, down);
}
static inline uint64_t pawn_double_push(uint64_t pawns, uint64_t all, int color) {
	unsigned down = color ? S : N;
	return pawns & ~shift(all, down) & ~shift_twice(all, down) & (color ? RANK_2 : RANK_7);
}

static inline uint64_t knight_attacks(int square, uint64_t own) {
	assert(attackgen_init_done);
	return knight_attacks_lookup[square] & ~own;
}

static inline uint64_t bishop_attacks(int square, uint64_t own, uint64_t all) {
	return bishop_attacks_pre(square, all) & ~own;
}

static inline uint64_t rook_attacks(int square, uint64_t own, uint64_t all) {
	return rook_attacks_pre(square, all) & ~own;
}

static inline uint64_t queen_attacks(int square, uint64_t own, uint64_t all) {
	return (bishop_attacks_pre(square, all) | rook_attacks_pre(square, all)) & ~own;
}

static inline uint64_t king_attacks(int square, uint64_t own) {
	assert(attackgen_init_done);
	return king_attacks_lookup[square] & ~own;
}

static inline uint64_t attacks(int piece, int square, uint64_t own, uint64_t all) {
	switch (piece) {
	case KNIGHT:
		return knight_attacks(square, own);
	case BISHOP:
		return bishop_attacks(square, own, all);
	case ROOK:
		return rook_attacks(square, own, all);
	case QUEEN:
		return queen_attacks(square, own, all);
	case KING:
		return king_attacks(square, own);
	default:
		assert(0);
		return 0;
	}
}

#endif
