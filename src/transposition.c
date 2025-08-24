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

#include "transposition.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "bitboard.h"
#include "history.h"
#include "position.h"

#ifndef NDEBUG
int transposition_init_done = 0;
#endif

/* 12 * 64: each piece each square.
 * 1: turn to move is white.
 * 16: each castling combination.
 * 8: en passant on file.
 */
uint64_t zobrist_keys[12 * 64 + 1 + 16 + 8];

static uint64_t start;

void transposition_clear(struct transpositiontable *tt) {
	memset(tt->table, 0, tt->size * sizeof(*tt->table));
}

int transposition_alloc(struct transpositiontable *tt, size_t bytes) {
	tt->size = bytes / sizeof(*tt->table);
	tt->table = malloc(tt->size * sizeof(*tt->table));
	if (tt->size && !tt->table)
		return 1;
	transposition_clear(tt);
	return 0;
}

int transposition_occupancy(const struct transpositiontable *tt, int bound) {
	if (tt->size == 0)
		return 0;
	uint64_t occupied = 0;
	for (size_t i = 0; i < tt->size; i++) {
		struct transposition *e = &tt->table[i];
		if (bound ? (e->bound == bound) :
			e->bound > 0)
			occupied++;
	}
	return 1000 * occupied / tt->size;
}

int hashfull(const struct transpositiontable *tt) {
	if (!tt || tt->size < 1000)
		return -1;
	int count = 0;
	for (int i = 0; i < 1000; i++) {
		struct transposition *e = &tt->table[i];
		if (e->zobrist_key)
			count++;
	}

	return count;
}

void transposition_init(void) {
	/* Need a seed where the cuckoo history can be initialized.
	 * Most seeds work, some do not.
	 */
	uint64_t seed = 123;
	for (int i = 0; i < 12 * 64 + 1 + 16 + 8; i++)
		zobrist_keys[i] = xorshift64(&seed);
	struct position pos;
	startpos(&pos);
#ifndef NDEBUG
	transposition_init_done = 1;
#endif
	refresh_zobrist_key(&pos);
	start = pos.zobrist_key;

}

/* Should be called before do_move. */
void do_zobrist_key(struct position *pos, const move_t *move) {
	assert(*move);
	assert(pos->mailbox[move_from(move)]);
	if (!option_transposition && !option_history)
		return;
	assert(transposition_init_done);
	int source_square = move_from(move);
	int target_square = move_to(move);

	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);

	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	pos->zobrist_key ^= zobrist_castle_key(castle(source_square, target_square, pos->castle));

	pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[source_square] - 1, source_square);
	pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[source_square] - 1, target_square);

	pos->zobrist_key ^= zobrist_turn_key();

	if (is_capture(pos, move))
		pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);

	if (source_square + 16 == target_square && pos->mailbox[source_square] == WHITE_PAWN &&
			((target_square != a4 && pos->mailbox[target_square - 1] == BLACK_PAWN) ||
			(target_square != h4 && pos->mailbox[target_square + 1] == BLACK_PAWN)))
		pos->zobrist_key ^= zobrist_en_passant_key(source_square + 8);
	if (source_square - 16 == target_square && pos->mailbox[source_square] == BLACK_PAWN &&
			((target_square != a5 && pos->mailbox[target_square - 1] == WHITE_PAWN) ||
			(target_square != h5 && pos->mailbox[target_square + 1] == WHITE_PAWN)))
		pos->zobrist_key ^= zobrist_en_passant_key(source_square - 8);

	switch (move_flag(move)) {
	case MOVE_EN_PASSANT:
		if (pos->turn)
			pos->zobrist_key ^= zobrist_piece_key(BLACK_PAWN - 1, target_square - 8);
		else
			pos->zobrist_key ^= zobrist_piece_key(WHITE_PAWN - 1, target_square + 8);
		break;
	case MOVE_PROMOTION:
		if (pos->turn) {
			pos->zobrist_key ^= zobrist_piece_key(WHITE_PAWN - 1, target_square);
			pos->zobrist_key ^= zobrist_piece_key(move_promote(move) + 1, target_square);
		}
		else {
			pos->zobrist_key ^= zobrist_piece_key(BLACK_PAWN - 1, target_square);
			pos->zobrist_key ^= zobrist_piece_key(move_promote(move) + 7, target_square);
		}
		break;
	case MOVE_CASTLE:
		switch (target_square) {
		case g1:
			pos->zobrist_key ^= zobrist_piece_key(WHITE_ROOK - 1, h1);
			pos->zobrist_key ^= zobrist_piece_key(WHITE_ROOK - 1, f1);
			break;
		case c1:
			pos->zobrist_key ^= zobrist_piece_key(WHITE_ROOK - 1, a1);
			pos->zobrist_key ^= zobrist_piece_key(WHITE_ROOK - 1, d1);
			break;
		case g8:
			pos->zobrist_key ^= zobrist_piece_key(BLACK_ROOK - 1, h8);
			pos->zobrist_key ^= zobrist_piece_key(BLACK_ROOK - 1, f8);
			break;
		case c8:
			pos->zobrist_key ^= zobrist_piece_key(BLACK_ROOK - 1, a8);
			pos->zobrist_key ^= zobrist_piece_key(BLACK_ROOK - 1, d8);
			break;
		}
		break;
	default:
		break;
	}
}

/* Should be called before undo_move. */
void undo_zobrist_key(struct position *pos, const move_t *move) {
	assert(*move);
	assert(!pos->mailbox[move_from(move)]);
	assert(pos->mailbox[move_to(move)]);
	if (!option_transposition && !option_history)
		return;
	assert(transposition_init_done);
	int source_square = move_from(move);
	int target_square = move_to(move);

	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->zobrist_key ^= zobrist_en_passant_key(move_en_passant(move));

	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	pos->zobrist_key ^= zobrist_castle_key(move_castle(move));

	pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
	pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);

	pos->zobrist_key ^= zobrist_turn_key();

	if (move_capture(move) && move_flag(move) != MOVE_EN_PASSANT) {
		if (pos->turn)
			pos->zobrist_key ^= zobrist_piece_key(move_capture(move) - 1, target_square);
		else
			pos->zobrist_key ^= zobrist_piece_key(move_capture(move) + 5, target_square);
	}

	switch (move_flag(move)) {
	case MOVE_EN_PASSANT:
		if (pos->turn)
			pos->zobrist_key ^= zobrist_piece_key(WHITE_PAWN - 1, target_square + 8);
		else
			pos->zobrist_key ^= zobrist_piece_key(BLACK_PAWN - 1, target_square - 8);
		break;
	case MOVE_PROMOTION:
		if (pos->turn) {
			pos->zobrist_key ^= zobrist_piece_key(BLACK_PAWN - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
		}
		else {
			pos->zobrist_key ^= zobrist_piece_key(WHITE_PAWN - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
		}
		break;
	case MOVE_CASTLE:
		switch (target_square) {
		case g1:
			pos->zobrist_key ^= zobrist_piece_key(WHITE_ROOK - 1, h1);
			pos->zobrist_key ^= zobrist_piece_key(WHITE_ROOK - 1, f1);
			break;
		case c1:
			pos->zobrist_key ^= zobrist_piece_key(WHITE_ROOK - 1, a1);
			pos->zobrist_key ^= zobrist_piece_key(WHITE_ROOK - 1, d1);
			break;
		case g8:
			pos->zobrist_key ^= zobrist_piece_key(BLACK_ROOK - 1, h8);
			pos->zobrist_key ^= zobrist_piece_key(BLACK_ROOK - 1, f8);
			break;
		case c8:
			pos->zobrist_key ^= zobrist_piece_key(BLACK_ROOK - 1, a8);
			pos->zobrist_key ^= zobrist_piece_key(BLACK_ROOK - 1, d8);
			break;
		}
		break;
	default:
		break;
	}
}

/* Should be called before do_null_move and then again before (un)do_null_move. */
void do_null_zobrist_key(struct position *pos, int en_passant) {
	if (!option_transposition && !option_history)
		return;
	assert(transposition_init_done);
	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->zobrist_key ^= zobrist_en_passant_key(en_passant);
	pos->zobrist_key ^= zobrist_turn_key();
}

void startkey(struct position *pos) {
	assert(transposition_init_done);
	pos->zobrist_key = start;
}

void refresh_zobrist_key(struct position *pos) {
	assert(transposition_init_done);
	pos->zobrist_key = 0;
	for (int i = 0; i < 64; i++)
		if (pos->mailbox[i])
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[i] - 1, i);
	if (pos->turn)
		pos->zobrist_key ^= zobrist_turn_key();
	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	if (pos->en_passant)
		pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
}

void transposition_free(struct transpositiontable *tt) {
	free(tt->table);
}
