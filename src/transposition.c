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

/* 12 * 64: each piece each square
 * 1: turn to move_t is white
 * 16: each castling combination
 * 8: en passant on file
 */
uint64_t zobrist_keys[12 * 64 + 1 + 16 + 8];

static uint64_t start;

void transposition_clear(struct transpositiontable *tt) {
	memset(tt->table, 0, tt->size * sizeof(*tt->table));
}

int transposition_alloc(struct transpositiontable *tt, size_t bytes) {
	tt->size = bytes / sizeof(*tt->table);
	tt->table = malloc(tt->size * sizeof(*tt->table));
	if (!tt->table)
		return 1;
	transposition_clear(tt);
	return 0;
}

int transposition_occupancy(struct transpositiontable *tt, int bound) {
	uint64_t occupied = 0;
	for (size_t i = 0; i < tt->size; i++) {
		struct transposition *e = &tt->table[i];
		if (bound ? (e->bound == bound) :
			e->bound > 0)
			occupied++;
	}
	return 1000 * occupied / tt->size;
}

void transposition_init(void) {
	for (int i = 0; i < 12 * 64 + 1 + 16 + 8; i++)
		zobrist_keys[i] = gxorshift64();
	struct position pos;
	startpos(&pos);
	refresh_zobrist_key(&pos);
	start = pos.zobrist_key;
}

/* Should be called before do_move. */
void do_zobrist_key(struct position *pos, const move_t *m) {
	assert(*m);
	assert(pos->mailbox[move_from(m)]);
	if (!option_transposition && !option_history)
		return;
	int source_square = move_from(m);
	int target_square = move_to(m);

	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);

	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	pos->zobrist_key ^= zobrist_castle_key(castle(source_square, target_square, pos->castle));

	pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[source_square] - 1, source_square);
	pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[source_square] - 1, target_square);

	pos->zobrist_key ^= zobrist_turn_key();

	if (is_capture(pos, m))
		pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);

	if (source_square + 16 == target_square && pos->mailbox[source_square] == white_pawn)
		pos->zobrist_key ^= zobrist_en_passant_key(source_square + 8);
	if (source_square - 16 == target_square && pos->mailbox[source_square] == black_pawn)
		pos->zobrist_key ^= zobrist_en_passant_key(source_square - 8);

	if (move_flag(m) == 1) {
		if (pos->turn)
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square - 8);
		else
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square + 8);
	}
	else if (move_flag(m) == 2) {
		if (pos->turn) {
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square);
			pos->zobrist_key ^= zobrist_piece_key(move_promote(m) + 1, target_square);
		}
		else {
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square);
			pos->zobrist_key ^= zobrist_piece_key(move_promote(m) + 7, target_square);
		}
	}
	else if (move_flag(m) == 3) {
		switch (target_square) {
		case g1:
			pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, h1);
			pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, f1);
			break;
		case c1:
			pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, a1);
			pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, d1);
			break;
		case g8:
			pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, h8);
			pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, f8);
			break;
		case c8:
			pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, a8);
			pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, d8);
			break;
		}
	}
}

/* Should be called before undo_move. */
void undo_zobrist_key(struct position *pos, const move_t *m) {
	assert(*m);
	assert(!pos->mailbox[move_from(m)]);
	assert(pos->mailbox[move_to(m)]);
	if (!option_transposition && !option_history)
		return;
	int source_square = move_from(m);
	int target_square = move_to(m);

	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->zobrist_key ^= zobrist_en_passant_key(move_en_passant(m));

	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	pos->zobrist_key ^= zobrist_castle_key(move_castle(m));

	pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
	pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);

	pos->zobrist_key ^= zobrist_turn_key();

	if (move_capture(m)) {
		if (pos->turn)
			pos->zobrist_key ^= zobrist_piece_key(move_capture(m) - 1, target_square);
		else
			pos->zobrist_key ^= zobrist_piece_key(move_capture(m) + 5, target_square);
	}

	if (move_flag(m) == 1) {
		if (pos->turn)
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square + 8);
		else
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square - 8);
	}
	else if (move_flag(m) == 2) {
		if (pos->turn) {
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
		}
		else {
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
		}
	}
	else if (move_flag(m) == 3) {
		switch (target_square) {
		case g1:
			pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, h1);
			pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, f1);
			break;
		case c1:
			pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, a1);
			pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, d1);
			break;
		case g8:
			pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, h8);
			pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, f8);
			break;
		case c8:
			pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, a8);
			pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, d8);
			break;
		}
	}
}

/* should be called before do_null_move and then again before (un)do_null_move */
void do_null_zobrist_key(struct position *pos, int en_passant) {
	if (!option_transposition && !option_history)
		return;
	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->zobrist_key ^= zobrist_en_passant_key(en_passant);
	pos->zobrist_key ^= zobrist_turn_key();
}

void startkey(struct position *pos) {
	pos->zobrist_key = start;
}

void refresh_zobrist_key(struct position *pos) {
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
