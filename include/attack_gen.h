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

static inline uint64_t white_pawn_capture_e(uint64_t pawns, uint64_t black_pieces) {
	return pawns & shift_south_west(black_pieces);
}
static inline uint64_t white_pawn_capture_w(uint64_t pawns, uint64_t black_pieces) {
	return pawns & shift_south_east(black_pieces);
}
static inline uint64_t white_pawn_push(uint64_t pawns, uint64_t pieces) {
	return pawns & ~shift_south(pieces);
}
static inline uint64_t white_pawn_double_push(uint64_t pawns, uint64_t pieces) {
	return white_pawn_push(pawns, pieces) & ~shift_south_south(pieces) & RANK_2;
}
static inline uint64_t black_pawn_capture_e(uint64_t pawns, uint64_t white_pieces) {
	return pawns & shift_north_west(white_pieces);
}
static inline uint64_t black_pawn_capture_w(uint64_t pawns, uint64_t white_pieces) {
	return pawns & shift_north_east(white_pieces);
}
static inline uint64_t black_pawn_push(uint64_t pawns, uint64_t pieces) {
	return pawns & ~shift_north(pieces);
}
static inline uint64_t black_pawn_double_push(uint64_t pawns, uint64_t pieces) {
	return black_pawn_push(pawns, pieces) & ~shift_north_north(pieces) & RANK_7;
}

static inline uint64_t knight_attacks(int square) {
	return knight_attacks_lookup[square];
}
static inline uint64_t white_knight_attacks(int square, uint64_t white_pieces) {
	return knight_attacks(square) & ~white_pieces;
}
static inline uint64_t black_knight_attacks(int square, uint64_t black_pieces) {
	return knight_attacks(square) & ~black_pieces;
}

static inline uint64_t bishop_attacks(int square, uint64_t pieces) {
	return bishop_attacks_lookup[bishop_index(square, pieces)];
}
static inline uint64_t white_bishop_attacks(int square, uint64_t white_pieces, uint64_t pieces) {
	return bishop_attacks(square, pieces) & ~white_pieces;
}
static inline uint64_t black_bishop_attacks(int square, uint64_t black_pieces, uint64_t pieces) {
	return bishop_attacks(square, pieces) & ~black_pieces;
}

static inline uint64_t rook_attacks(int square, uint64_t pieces) {
	return rook_attacks_lookup[rook_index(square, pieces)];
}
static inline uint64_t white_rook_attacks(int square, uint64_t white_pieces, uint64_t pieces) {
	return rook_attacks(square, pieces) & ~white_pieces;
}
static inline uint64_t black_rook_attacks(int square, uint64_t black_pieces, uint64_t pieces) {
	return rook_attacks(square, pieces) & ~black_pieces;
}

static inline uint64_t queen_attacks(int square, uint64_t pieces) {
	return rook_attacks(square, pieces) | bishop_attacks(square, pieces);
}
static inline uint64_t white_queen_attacks(int square, uint64_t white_pieces, uint64_t pieces) {
	return queen_attacks(square, pieces) & ~white_pieces;
}
static inline uint64_t black_queen_attacks(int square, uint64_t black_pieces, uint64_t pieces) {
	return queen_attacks(square, pieces) & ~black_pieces;
}

static inline uint64_t king_attacks(int square) {
	return king_attacks_lookup[square];
}
static inline uint64_t white_king_attacks(int square, uint64_t white_pieces) {
	return king_attacks(square) & ~white_pieces;
}
static inline uint64_t black_king_attacks(int square, uint64_t black_pieces) {
	return king_attacks(square) & ~black_pieces;
}

#endif
