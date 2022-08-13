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

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include <stdint.h>

#include "position.h"

#ifndef TT
#define TT 64M
#endif

struct transposition {
	uint64_t zobrist_key;
	int16_t evaluation;

	/* first 6 bits for depth, last 2 bits for node type
	 * node type, 0: pv, 1: cut, 2: all
	 * <https://www.chessprogramming.org/Node_Types>
	 */
	uint8_t depth_type;

	/* first 12 bits for move, last 4 bits for age */
	uint16_t move_age;
};

static inline uint64_t transposition_zobrist_key(struct transposition *e) { return e->zobrist_key; }
static inline int16_t transposition_evaluation(struct transposition *e) { return e->evaluation; }
static inline uint8_t transposition_depth(struct transposition *e) { return e->depth_type & 0x3F; }
static inline uint8_t transposition_type(struct transposition *e) { return e->depth_type >> 0x6; }
static inline uint16_t transposition_move(struct transposition *e) { return e->move_age & 0xFFF; }
static inline uint16_t transposition_age(struct transposition *e) { return e->move_age >> 0xC; }
static inline void transposition_set_zobrist_key(struct transposition *e, uint64_t t) { e->zobrist_key = t; }
static inline void transposition_set_evaluation(struct transposition *e, int16_t t) { e->evaluation = t; }
static inline void transposition_set_depth(struct transposition *e, uint8_t t) { e->depth_type |= t; }
static inline void transposition_set_type(struct transposition *e, uint8_t t) { e->depth_type |= (t << 0x6); }
static inline void transposition_set_move(struct transposition *e, uint16_t t) { e->move_age |= t; }
static inline void transposition_set_age(struct transposition *e, uint8_t t) { e->move_age |= (t << 0xC); }

struct transposition_table {
	struct transposition *table;
	uint64_t size;

	/* 12 * 64: each piece each square
	 * 1: turn to move is white
	 * 16: each castling combination
	 * 8: en passant on file
	 */
	uint64_t *zobrist_key;
};

void transposition_table_size_print(uint64_t size);

uint64_t transposition_table_size_bytes(char *t);

struct transposition *get(struct position *pos);

struct transposition *attempt_get(struct position *pos);

void attempt_store(struct position *pos, int16_t evaluation, uint8_t depth, uint8_t type, uint16_t m);

uint64_t transposition_table_size();

void transposition_table_clear();

uint64_t zobrist_piece_key(int piece, int square);

uint64_t zobrist_turn_key();

uint64_t zobrist_castle_key(int castle);

uint64_t zobrist_en_passant_key(int square);

int allocate_transposition_table(uint64_t t);

int transposition_table_init();

void transposition_table_term();

#endif
