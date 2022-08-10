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

#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>

#include "position.h"

#ifndef HASH
#define HASH 64M
#endif

struct hash_entry {
	uint64_t zobrist_key;
	int16_t evaluation;
	int8_t depth;
};

struct hash_table {
	struct hash_entry *table;
	uint64_t size;

	/* 12 * 64: each piece each square
	 * 1: turn to move is white
	 * 16: each castling combination
	 * 8: en passant on file
	 */
	uint64_t *zobrist_key;
};

void hash_table_size_print(uint64_t size);

uint64_t hash_table_size_bytes(char *t);

struct hash_entry *table_entry(struct position *pos);

void store(struct position *pos, int16_t evaluation, int8_t depth);

uint64_t hash_table_size();

void hash_table_clear();

uint64_t zobrist_piece_key(int piece, int square);

uint64_t zobrist_turn_key();

uint64_t zobrist_castle_key(int castle);

uint64_t zobrist_en_passant_key(int square);

int allocate_hash_table(uint64_t t);

int hash_table_init();

void hash_table_term();

#endif
