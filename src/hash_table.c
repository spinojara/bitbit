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

#include "hash_table.h"

#include <stdlib.h>
#include <stdio.h>

#include "util.h"
#include "init.h"

struct hash_table *hash_table;

#ifndef HASH
#define HASH 64M
#endif

uint64_t hash_table_size_bytes() {
	char t[] = MACRO_VALUE(HASH);
	int i;
	int flag;
	uint64_t size;
	for (size = 0, flag = 0, i = 0; t[i] != '\0'; i++) {
		switch (t[i]) {
		case 'G':
			size *= 1024;
			/* fallthrough */
		case 'M':
			size *= 1024;
			/* fallthrough */
		case 'K':
			size *= 1024;
			if (!flag) {
				flag = 1;
				break;
			}
			/* fallthrough */
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (!flag) {
				size *= 10;
				/* will return 0 anyway if t[i] is K, M, G */
				size += find_char("0123456789", t[i]);
				break;
			}
			/* fallthrough */
		default:
			return 0;
		}
	}
	return size;
}

struct hash_entry *table_entry(struct position *pos) {
	return hash_table->table + (pos->zobrist_key % hash_table->size);
}

void store_table_entry(struct position *pos, int16_t evaluation, int8_t depth) {
	table_entry(pos)->evaluation = evaluation;
	table_entry(pos)->depth = depth;
	table_entry(pos)->zobrist_key = pos->zobrist_key;
}

uint64_t hash_table_size() {
	return hash_table->size;
}

void hash_table_clear() {
	for (uint64_t i = 0; i < hash_table->size; i++)
		(hash_table->table + i)->depth = -1;
}

uint64_t zobrist_piece_key(int piece, int square) {
	return hash_table->zobrist_key[piece + 12 * square];
}

uint64_t zobrist_turn_key() {
	return hash_table->zobrist_key[12 * 64];
}

uint64_t zobrist_castle_key(int castle) {
	return hash_table->zobrist_key[12 * 64 + 1 + castle];
}

uint64_t zobrist_en_passant_key(int square) {
	return hash_table->zobrist_key[12 * 64 + 1 + 16 + square % 8];
}

int hash_table_init() {
	uint64_t t = hash_table_size_bytes();
	if (t < sizeof(struct hash_entry)) {
		printf("\33[2Kfatal error: bad hash table size\n");
		return 0;
	}

	hash_table = malloc(sizeof(struct hash_table));
	hash_table->size = t / sizeof(struct hash_entry);
	hash_table->table = malloc(hash_table->size * sizeof(struct hash_entry));

	hash_table_clear();

	hash_table->zobrist_key = malloc((12 * 64 + 1 + 16 + 8) * sizeof(uint64_t));

	for (int i = 0; i < 12 * 64 + 1 + 16 + 8; i++) {
		hash_table->zobrist_key[i] = rand_uint64();
		init_status("generating zobrist keys");
	}
	return 1;
}

void hash_table_term() {
	free(hash_table->zobrist_key);
	free(hash_table->table);
	free(hash_table);
}
