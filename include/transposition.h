/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2025 Isak Ellmer
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
#include "util.h"

#ifndef NDEBUG
extern int transposition_init_done;
#endif

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
	int8_t depth;
	uint8_t bound;
	uint16_t move;
	uint8_t flags;
};

struct transpositiontable {
	struct transposition *table;
	uint32_t size;
};

extern uint64_t zobrist_keys[];
extern uint64_t *test;

static inline uint64_t transposition_index(uint64_t size, uint64_t key) {
	return ((key & 0xFFFFFFFF) * size) >> 32;
}

static inline struct transposition *transposition_get(const struct transpositiontable *tt, const struct position *pos) {
	assert(transposition_init_done);
	return &tt->table[transposition_index(tt->size, pos->zobrist_key)];
}

static inline struct transposition *transposition_probe(const struct transpositiontable *tt, const struct position *pos) {
	if (!option_transposition)
		return NULL;
	assert(transposition_init_done);
	struct transposition *e = transposition_get(tt, pos);
	if (e->zobrist_key == pos->zobrist_key)
		return e;
	return NULL;
}

static inline void transposition_set(struct transposition *e, const struct position *pos, int32_t evaluation, int depth, int bound, move_t move) {
	assert(transposition_init_done);
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

static inline void transposition_store(struct transpositiontable *tt, const struct position *pos, int32_t evaluation, int depth, int bound, move_t move) {
	if (!option_transposition)
		return;
	assert(transposition_init_done);
	struct transposition *e = transposition_get(tt, pos);
	if (e->zobrist_key != pos->zobrist_key ||
			depth >= e->depth ||
			(bound == BOUND_EXACT && e->bound != BOUND_EXACT))
		transposition_set(e, pos, evaluation, depth, bound, move);
	else if (move && e->flags & TRANSPOSITION_OLD_MOVE)
		e->move = (uint16_t)move;
}

static inline int32_t adjust_score_mate_store(int32_t evaluation, int ply) {
	if (evaluation == VALUE_NONE)
		return VALUE_NONE;

	if (evaluation >= VALUE_MATE_IN_MAX_PLY)
		return evaluation + ply;
	else if (evaluation <= -VALUE_MATE_IN_MAX_PLY)
		return evaluation - ply;
	return evaluation;
}

static inline int32_t adjust_score_mate_get(int32_t evaluation, int ply, int halfmove) {
	if (evaluation == VALUE_NONE)
		return VALUE_NONE;

	if (evaluation >= VALUE_MATE_IN_MAX_PLY) {
		if (VALUE_MATE - evaluation > 100 - halfmove)
			return VALUE_MATE_IN_MAX_PLY - 1;
		return evaluation - ply;
	}
	else if (evaluation <= -VALUE_MATE_IN_MAX_PLY) {
		if (VALUE_MATE + evaluation > 100 - halfmove)
			return -VALUE_MATE_IN_MAX_PLY + 1;
		return evaluation + ply;
	}
	return evaluation;
}

/* Can avoid the multiplication here if we instead do square + 64 * piece. */
static inline uint64_t zobrist_piece_key(int piece, int square) {
	assert(transposition_init_done);
	return zobrist_keys[square + 64 * (piece - 1)];
}

static inline uint64_t zobrist_turn_key(void) {
	assert(transposition_init_done);
	return zobrist_keys[12 * 64];
}

static inline uint64_t zobrist_castle_key(int castle) {
	assert(transposition_init_done);
	return zobrist_keys[12 * 64 + 1 + castle];
}

static inline uint64_t zobrist_en_passant_key(int square) {
	assert(transposition_init_done);
	if (square == 0)
		return 0;
	return zobrist_keys[12 * 64 + 1 + 16 + file_of(square)];
}

void transposition_clear(struct transpositiontable *tt);

int transposition_alloc(struct transpositiontable *tt, size_t bytes);

void transposition_free(struct transpositiontable *tt);

int transposition_occupancy(const struct transpositiontable *tt, int bound);

int hashfull(const struct transpositiontable *tt);

void do_zobrist_key(struct position *pos, const move_t *move);

void undo_zobrist_key(struct position *pos, const move_t *move);

void do_null_zobrist_key(struct position *pos, int en_passant);

void startkey(struct position *pos);

void refresh_zobrist_key(struct position *pos);

void transposition_init(void);

#endif
