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
#include "magicbitboard.h"

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

static inline uint64_t knight_attacks(int square, uint64_t own_pieces) {
	return knight_attacks_lookup[square] & ~own_pieces;
}

static inline uint64_t bishop_attacks(int square, uint64_t own, uint64_t allb) {
	return bishop_attacks_lookup[bishop_index(square, allb)] & ~own;
}

static inline uint64_t rook_attacks(int square, uint64_t own, uint64_t allb) {
	return rook_attacks_lookup[rook_index(square, allb)] & ~own;
}

static inline uint64_t queen_attacks(int square, uint64_t own, uint64_t allb) {
	return (bishop_attacks_lookup[bishop_index(square, allb)] | rook_attacks_lookup[rook_index(square, allb)]) & ~own;
}

static inline uint64_t king_attacks(int square, uint64_t own) {
	return king_attacks_lookup[square] & ~own;
}

#endif
