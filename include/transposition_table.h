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
#include "transposition_table.h"
#include "evaluate.h"
#include "move.h"
#include "search.h"
#include "interrupt.h"

struct transposition {
	uint64_t zobrist_key;
	int16_t evaluation;
	uint8_t depth;
	uint8_t type;
	uint16_t move;
	uint8_t draw;
};

static inline uint64_t transposition_zobrist_key(struct transposition *e) { return e->zobrist_key; }
static inline int16_t transposition_evaluation(struct transposition *e) { return e->evaluation; }
static inline uint8_t transposition_depth(struct transposition *e) { return e->depth; }
static inline uint8_t transposition_type(struct transposition *e) { return e->type; }
static inline uint16_t transposition_move(struct transposition *e) { return e->move; }
static inline uint8_t transposition_open(struct transposition *e) { return e->draw; }
static inline void transposition_set_zobrist_key(struct transposition *e, uint64_t t) { e->zobrist_key = t; }
static inline void transposition_set_evaluation(struct transposition *e, int16_t t) { e->evaluation = t; }
static inline void transposition_set_depth(struct transposition *e, uint8_t t) { e->depth = t; }
static inline void transposition_set_type(struct transposition *e, uint8_t t) { e->type = t; }
static inline void transposition_set_move(struct transposition *e, uint16_t t) { e->move = t; }
static inline void transposition_set_open(struct transposition *e) { e->draw = 1; }
static inline void transposition_set_closed(struct transposition *e) { e->draw = 0; }

struct transposition_table {
	struct transposition *table;
	uint64_t size;
	uint64_t index;

	/* 12 * 64: each piece each square
	 * 1: turn to move is white
	 * 16: each castling combination
	 * 8: en passant on file
	 */
	uint64_t *zobrist_key;
};

extern struct transposition_table *transposition_table;

static inline struct transposition *get(struct position *pos) {
	return transposition_table->table + (pos->zobrist_key & transposition_table->index);
}

static inline struct transposition *attempt_get(struct position *pos) {
	struct transposition *e = get(pos);
	if (transposition_zobrist_key(e) == pos->zobrist_key)
		return e;
	return NULL;
}

static inline void store(struct transposition *e, struct position *pos, int16_t evaluation, uint8_t depth, uint8_t type, move m) {
	transposition_set_zobrist_key(e, pos->zobrist_key);
	transposition_set_evaluation(e, evaluation);
	transposition_set_depth(e, depth);
	transposition_set_type(e, type);
	/* keep old move */
	if (m)
		transposition_set_move(e, m & 0xFFFF);
}

static inline void attempt_store(struct position *pos, int16_t evaluation, uint8_t depth, uint8_t type, move m) {
	if (interrupt)
		return;
	struct transposition *e = get(pos);
	if ((type == NODE_PV && transposition_type(e) != NODE_PV) ||
			transposition_zobrist_key(e) != pos->zobrist_key ||
			depth > transposition_depth(e))
		store(e, pos, evaluation, depth, type, m);
}

static inline int16_t adjust_value_mate_store(int16_t evaluation, uint8_t ply) {
	int adjustment = 0;
	if (evaluation >= VALUE_MATE_IN_MAX_PLY)
		adjustment = ply;
	else if (evaluation <= -VALUE_MATE_IN_MAX_PLY)
		adjustment = -ply;
	return evaluation + adjustment;
}

static inline int16_t adjust_value_mate_get(int16_t evaluation, uint8_t ply) {
	int adjustment = 0;
	/* should probably be more careful as to not return false mates */
	if (evaluation >= VALUE_MATE_IN_MAX_PLY)
		adjustment = -ply;
	else if (evaluation <= -VALUE_MATE_IN_MAX_PLY)
		adjustment = ply;
	return evaluation + adjustment;
}

static inline uint64_t zobrist_piece_key(int piece, int square) {
	return transposition_table->zobrist_key[piece + 12 * square];
}

static inline uint64_t zobrist_turn_key(void) {
	return transposition_table->zobrist_key[12 * 64];
}

static inline uint64_t zobrist_castle_key(int castle) {
	return transposition_table->zobrist_key[12 * 64 + 1 + castle];
}

static inline uint64_t zobrist_en_passant_key(int square) {
	if (square == 0)
		return 0;
	return transposition_table->zobrist_key[12 * 64 + 1 + 16 + square % 8];
}

void transposition_table_size_print(uint64_t t);

uint64_t transposition_table_size_bytes(char *t);

uint64_t transposition_table_size(void);

void transposition_table_clear(void);

int allocate_transposition_table(uint64_t t);

int transposition_table_occupancy(int node_type);

void zobrist_key_init(void);

void do_zobrist_key(struct position *pos, const move *m);

void undo_zobrist_key(struct position *pos, const move *m);

void do_null_zobrist_key(struct position *pos, int en_passant);

void set_zobrist_key(struct position *pos);

int transposition_table_init(void);

void transposition_table_term(void);

#endif
