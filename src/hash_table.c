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
#include <inttypes.h>

#include "util.h"
#include "init.h"

struct hash_table *hash_table = NULL;

void hash_table_size_print(uint64_t size) {
	/* 0 -> B
	 * 1 -> K
	 * 2 -> M
	 * 3 -> G
	 */
	int t = 0;

	if (size >= (uint64_t)10000)
		t++;
	if (size >= (uint64_t)10000 * 1024)
		t++;
	if (size >= (uint64_t)10000 * 1024 * 1024)
		t++;

	printf("%" PRIu64 "%c", size / power(1024, t), "BKMG"[t]);
}

uint64_t hash_table_size_bytes(char *t) {
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

struct hash_entry *get(struct position *pos) {
	return hash_table->table + (pos->zobrist_key % hash_table->size);
}

struct hash_entry *attempt_get(struct position *pos) {
	struct hash_entry *e = get(pos);
	if (hash_entry_zobrist_key(e) == pos->zobrist_key)
		return e;
	return NULL;
}

void store(struct hash_entry *e, struct position *pos, int16_t evaluation, uint8_t depth, uint8_t type, uint16_t m) {
	hash_entry_set_zobrist_key(e, pos->zobrist_key);
	hash_entry_set_evaluation(e, evaluation);
	e->depth_type = 0;
	hash_entry_set_depth(e, depth);
	hash_entry_set_type(e, type);
	e->move_age = 0;
	hash_entry_set_move(e, m);
	hash_entry_set_age(e, pos->halfmove % 0x10);
}

void attempt_store(struct position *pos, int16_t evaluation, uint8_t depth, uint8_t type, uint16_t m) {
	struct hash_entry *e = get(pos);
	if (depth >= hash_entry_depth(e))
		store(e, pos, evaluation, depth, type, m);
}

uint64_t hash_table_size() {
	return hash_table->size;
}

void hash_entry_clear(struct hash_entry *e) {
	e->depth_type = 0;
}

void hash_table_clear() {
	for (uint64_t i = 0; i < hash_table->size; i++)
		hash_entry_clear(hash_table->table + i);
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
	if (!square)
		return 0;
	return hash_table->zobrist_key[12 * 64 + 1 + 16 + square % 8];
}

int allocate_hash_table(uint64_t t) {
	if (t < sizeof(struct hash_entry))
		return 1;
	hash_table->size = t / sizeof(struct hash_entry);
	free(hash_table->table);
	hash_table->table = malloc(hash_table->size * sizeof(struct hash_entry));
	if (!hash_table->table) {
		printf("\33[2Kfatal error: could not allocate hash table\n");
		return 2;
	}
	hash_table_clear();
	return 0;
}

int hash_table_init() {
	uint64_t t = hash_table_size_bytes(MACRO_VALUE(HASH));

	hash_table = malloc(sizeof(struct hash_table));
	hash_table->table = NULL;
	int ret = allocate_hash_table(t);
	if (ret) {
		if (ret == 1)
			printf("\33[2Kfatal error: could not allocate hash table\n");
		return 1;
	}

	hash_table->zobrist_key = malloc((12 * 64 + 1 + 16 + 8) * sizeof(uint64_t));

	for (int i = 0; i < 12 * 64 + 1 + 16 + 8; i++) {
		hash_table->zobrist_key[i] = rand_uint64();
		init_status("generating zobrist keys");
	}
	return 0;
}

void hash_table_term() {
	if (hash_table) {
		free(hash_table->zobrist_key);
		free(hash_table->table);
	}
	free(hash_table);
}
