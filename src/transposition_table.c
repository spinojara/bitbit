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

#include "transposition_table.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "util.h"
#include "init.h"
#include "bitboard.h"
#include "history.h"

struct transposition_table *transposition_table;

void transposition_table_size_print(uint64_t t) {
	int s = MIN(t / 10, 3);
	printf("%" PRIu64 "%c", ((uint64_t)1 << t) / ((uint64_t)1 << 10 * s), "BKMG"[s]);
}

struct transposition *get(struct position *pos) {
	return transposition_table->table + (pos->zobrist_key & transposition_table->index);
}

struct transposition *attempt_get(struct position *pos) {
	struct transposition *e = get(pos);
	if (transposition_zobrist_key(e) == pos->zobrist_key)
		return e;
	return NULL;
}

void store(struct transposition *e, struct position *pos, int16_t evaluation, uint8_t depth, uint8_t type, uint16_t m) {
	transposition_set_zobrist_key(e, pos->zobrist_key);
	transposition_set_evaluation(e, evaluation);
	transposition_set_depth(e, depth);
	transposition_set_type(e, type);
	transposition_set_move(e, m);
}

void attempt_store(struct position *pos, int16_t evaluation, uint8_t depth, uint8_t type, uint16_t m) {
	struct transposition *e = get(pos);
	if (transposition_type(e) == 0 ||
			transposition_zobrist_key(e) != pos->zobrist_key ||
			depth >= transposition_depth(e))
		store(e, pos, evaluation, depth, type, m);
}

uint64_t transposition_table_size(void) {
	return transposition_table->size;
}

void transposition_table_clear(void) {
	memset(transposition_table->table, 0, transposition_table->size * sizeof(struct transposition));
}

uint64_t zobrist_piece_key(int piece, int square) {
	return transposition_table->zobrist_key[piece + 12 * square];
}

uint64_t zobrist_turn_key(void) {
	return transposition_table->zobrist_key[12 * 64];
}

uint64_t zobrist_castle_key(int castle) {
	return transposition_table->zobrist_key[12 * 64 + 1 + castle];
}

uint64_t zobrist_en_passant_key(int square) {
	if (square == 0)
		return 0;
	return transposition_table->zobrist_key[12 * 64 + 1 + 16 + square % 8];
}

int allocate_transposition_table(uint64_t t) {
	uint64_t size = (uint64_t)1 << t;
	if (size < sizeof(struct transposition))
		return 2;
	transposition_table->size = size / sizeof(struct transposition);
	transposition_table->table = malloc(transposition_table->size * sizeof(struct transposition));
	
	if (!transposition_table->table)
		return 3;
	transposition_table->index = transposition_table->size - 1;
	transposition_table_clear();
	return 0;
}

int transposition_table_occupancy(void) {
	uint64_t occupied = 0;
	for (uint64_t i = 0; i < transposition_table->size; i++)
		if ((transposition_table->table + i)->zobrist_key)
			occupied++;
	return 100 * occupied / transposition_table->size;
}

void zobrist_key_init(void) {
	for (int i = 0; i < 12 * 64 + 1 + 16 + 8; i++) {
		transposition_table->zobrist_key[i] = rand_uint64();
		init_status("generating zobrist keys");
	}
}

void do_zobrist_key(struct position *pos, const move *m) {
	uint8_t source_square = move_from(m);
	uint8_t target_square = move_to(m);

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

void undo_zobrist_key(struct position *pos, const move *m) {
	uint8_t source_square = move_from(m);
	uint8_t target_square = move_to(m);

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
	else if(move_flag(m) == 2) {
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

void do_null_zobrist_key(struct position *pos, int en_passant) {
	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->zobrist_key ^= zobrist_en_passant_key(en_passant);
	pos->zobrist_key ^= zobrist_turn_key();
}

int transposition_table_init(void) {
	transposition_table = malloc(sizeof(struct transposition_table));
	transposition_table->table = NULL;
	int ret = allocate_transposition_table(TT);
	if (ret) {
		printf("\33[2Kfatal error: could not allocate transposition table\n");
		return ret;
	}

	transposition_table->zobrist_key = malloc((12 * 64 + 1 + 16 + 8) * sizeof(uint64_t));
	zobrist_key_init();

	return 0;
}

void set_zobrist_key(struct position *pos) {
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

void transposition_table_term(void) {
	if (transposition_table) {
		free(transposition_table->zobrist_key);
		free(transposition_table->table);
		free(transposition_table);
	}
}
