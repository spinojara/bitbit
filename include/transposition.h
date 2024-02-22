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

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include <stdint.h>
#include <assert.h>

#include "position.h"
#include "transposition.h"
#include "evaluate.h"
#include "move.h"
#include "search.h"
#include "interrupt.h"
#include "util.h"

#define SET_ASSOCIATIVITY 2
#define AGE_YOUNG (SET_ASSOCIATIVITY - 1)

extern int option_transposition;
extern int option_history;

enum {
	BOUND_LOWER = 0x1,
	BOUND_UPPER = 0x2,
	BOUND_EXACT = BOUND_LOWER | BOUND_UPPER,
};

enum {
	TRANSPOSITION_OLD_MOVE = 0x1,
};

struct transposition {
	uint64_t zobrist_key;
	int16_t eval;
	uint8_t depth;
	uint8_t bound;
	uint16_t move;
	uint8_t flags;
	uint8_t age;
};

struct transpositionset {
	struct transposition transposition[SET_ASSOCIATIVITY];
};

struct transpositiontable {
	struct transpositionset *transpositionset;
	uint32_t size;
};

extern uint64_t zobrist_keys[];

static inline uint64_t transposition_index(uint64_t size, uint64_t key) {
	return ((key & 0xFFFFFFFF) * size) >> 32;
}

static inline struct transpositionset *transpositionset_get(const struct transpositiontable *tt, const struct position *pos) {
	return &tt->transpositionset[transposition_index(tt->size, pos->zobrist_key)];
}

static inline struct transposition *transposition_probe(const struct transpositiontable *tt, const struct position *pos) {
	if (!option_transposition)
		return NULL;
	struct transpositionset *s = transpositionset_get(tt, pos);
	for (int i = 0; i < SET_ASSOCIATIVITY; i++) {
		struct transposition *e = &s->transposition[i];
		if (e->zobrist_key == pos->zobrist_key) {
#if 0
			for (int j = 0; j < SET_ASSOCIATIVITY; j++) {
				struct transposition *f = &s->transposition[j];
				f->age -= f->age > e->age;
			}
			e->age = AGE_YOUNG;
#endif
			return e;
		}
	}
	return NULL;
}

static inline void transposition_set(struct transposition *e, const struct position *pos, int32_t evaluation, int depth, int bound, move_t move) {
	assert(-VALUE_INFINITE < evaluation && evaluation < VALUE_INFINITE);
	e->flags = 0;
	/* Keep old move if none available. */
	if (move)
		e->move = (uint16_t)move;
	else
		e->flags |= TRANSPOSITION_OLD_MOVE;
	e->zobrist_key = pos->zobrist_key;
	e->eval = evaluation;
	e->depth = depth;
	e->bound = bound;
}

static inline struct transposition *transposition_replace(struct transpositionset *s, const struct position *pos) {
	struct transposition *e = NULL, *f;
	unsigned age = AGE_YOUNG;
	for (int i = 0; i < SET_ASSOCIATIVITY; i++) {
		f = &s->transposition[i];
		if (f->zobrist_key == pos->zobrist_key)
			return f;
		if (f->age <= age) {
			e = f;
			age = e->age;
		}
	}
#if 1
	for (int i = 0; i < SET_ASSOCIATIVITY; i++) {
		f = &s->transposition[i];
		f->age -= f->age > e->age;
	}
	e->age = AGE_YOUNG;
#endif
	return e;
}

static inline void transposition_store(const struct transpositiontable *tt, const struct position *pos, int32_t evaluation, int depth, int bound, move_t move) {
	if (interrupt || !option_transposition)
		return;
	struct transpositionset *s = transpositionset_get(tt, pos);
	struct transposition *e = transposition_replace(s, pos);
	if (e->zobrist_key != pos->zobrist_key ||
			depth >= e->depth ||
			(bound == BOUND_EXACT && e->bound != BOUND_EXACT))
		transposition_set(e, pos, evaluation, depth, bound, move);
	else if (move && e->flags & TRANSPOSITION_OLD_MOVE)
		e->move = (uint16_t)move;
}

static inline int32_t adjust_score_mate_store(int32_t evaluation, int ply) {
	int32_t adjustment = 0;
	if (evaluation >= VALUE_MATE_IN_MAX_PLY)
		adjustment = ply;
	else if (evaluation <= -VALUE_MATE_IN_MAX_PLY)
		adjustment = -ply;
	return evaluation + adjustment;
}

static inline int32_t adjust_score_mate_get(int32_t evaluation, int ply) {
	int32_t adjustment = 0;
	/* Should probably be more careful as to not return false mates. */
	if (evaluation >= VALUE_MATE_IN_MAX_PLY)
		adjustment = -ply;
	else if (evaluation <= -VALUE_MATE_IN_MAX_PLY)
		adjustment = ply;
	return evaluation + adjustment;
}

/* Can avoid the multiplication here if we instead do square + 64 * piece. */
static inline uint64_t zobrist_piece_key(int piece, int square) {
	return zobrist_keys[piece + 12 * square];
}

static inline uint64_t zobrist_turn_key(void) {
	return zobrist_keys[12 * 64];
}

static inline uint64_t zobrist_castle_key(int castle) {
	return zobrist_keys[12 * 64 + 1 + castle];
}

static inline uint64_t zobrist_en_passant_key(int square) {
	if (square == 0)
		return 0;
	return zobrist_keys[12 * 64 + 1 + 16 + file_of(square)];
}

void transposition_clear(struct transpositiontable *tt);

int transposition_alloc(struct transpositiontable *tt, size_t bytes);

void transposition_free(struct transpositiontable *tt);

int transposition_occupancy(const struct transpositiontable *tt, int node_type);
int hashfull(const struct transpositiontable *tt);

void transposition_init(void);

void do_zobrist_key(struct position *pos, const move_t *move);

void undo_zobrist_key(struct position *pos, const move_t *move);

void do_null_zobrist_key(struct position *pos, int en_passant);

void startkey(struct position *pos);

void refresh_zobrist_key(struct position *pos);

void transposition_init(void);

#endif
