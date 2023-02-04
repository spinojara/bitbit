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

#ifndef ATTACK_GEN_H
#define ATTACK_GEN_H

#include <stdint.h>

#include "bitboard.h"
#include "magic_bitboard.h"

void attack_gen_init(void);

extern uint64_t knight_attacks_lookup[64];
extern uint64_t king_attacks_lookup[64];

static inline uint64_t pawn_capture_e(uint64_t pawns, uint64_t enemy, int color) {
	return pawns & (color ? shift_south_west(enemy) : shift_north_west(enemy));
}
static inline uint64_t pawn_capture_w(uint64_t pawns, uint64_t enemy, int color) {
	return pawns & (color ? shift_south_east(enemy) : shift_north_east(enemy));
}
static inline uint64_t pawn_push(uint64_t pawns, uint64_t allb, int color) {
	return pawns & (color ? ~shift_south(allb) : ~shift_north(allb));
}
static inline uint64_t pawn_double_push(uint64_t pawns, uint64_t allb, int color) {
	return pawn_push(pawns, allb, color) &  (color ? ~shift_south_south(allb) & RANK_2 : ~shift_north_north(allb) & RANK_7);
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
