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

#include "pawn.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "bitboard.h"

/* around 100 KiB with hitrate of ~80% */
#define PAWN_TABLE_SIZE ((uint64_t)1 << 12)

struct pawn {
	uint64_t white;
	uint64_t black;
	int16_t evaluation;
};

struct pawn *pawn_table = NULL;

uint64_t pawn_backward_white(uint64_t white_pawns, uint64_t black_pawns) {
	uint64_t white_attacks = shift_north(shift_west(white_pawns)) | shift_north(shift_east(white_pawns));
	uint64_t black_attacks = shift_south(shift_west(black_pawns)) | shift_south(shift_east(black_pawns));

	uint64_t white_attack_spans = fill_north(white_attacks);
	uint64_t backward = fill_south(~white_attack_spans & black_attacks);
	return backward & white_pawns;
}

uint64_t pawn_backward_black(uint64_t white_pawns, uint64_t black_pawns) {
	uint64_t white_attacks = shift_north(shift_west(white_pawns)) | shift_north(shift_east(white_pawns));
	uint64_t black_attacks = shift_south(shift_west(black_pawns)) | shift_south(shift_east(black_pawns));

	uint64_t black_attack_spans = fill_south(black_attacks);
	uint64_t backward = fill_north(~black_attack_spans & white_attacks);
	return backward & black_pawns;
}

uint64_t pawn_doubled(uint64_t pawns) {
	uint64_t t, r = 0;
	for (int i = 0; i < 8; i++) {
		t = pawns & file(i);
		if (t & (t - 1))
			r |= t;
	}
	return r;
}

uint64_t pawn_isolated(uint64_t pawns) {
	uint64_t r = 0;
	for (int i = 0; i < 8; i++) {
		if ((pawns & file(i)) && !(pawns & adjacent_files(i)))
			r |= pawns & file(i);
	}
	return r;
}

uint64_t pawn_chain_white(uint64_t pawns) {
	return pawns & (shift_north(shift_west(pawns)) | shift_north(shift_east(pawns)));
}

uint64_t pawn_chain_black(uint64_t pawns) {
	return pawns & (shift_south(shift_west(pawns)) | shift_south(shift_east(pawns)));
}

uint16_t hash(uint64_t key) {
	key = key * 0x4cf5ad432745937full;
	key = key ^ (key >> 23) ^ (key >> 46);
	return key % PAWN_TABLE_SIZE;
}

struct pawn *pawn_get(struct position *pos) {
	return pawn_table + hash((pos->piece[white][pawn] |  pos->piece[black][pawn]) >> 8);
}

struct pawn *pawn_attempt_get(struct position *pos) {
	struct pawn *p = pawn_get(pos);
	if (p->white != pos->piece[white][pawn] || p->black != pos->piece[black][pawn])
		return NULL;
	return p;
}

void pawn_store(struct position *pos, int16_t evaluation) {
	struct pawn *e = pawn_get(pos);
	e->white = pos->piece[white][pawn];
	e->black = pos->piece[black][pawn];
	e->evaluation = evaluation;
}

int16_t evaluate_pawns(struct position *pos) {
	struct pawn *e = pawn_attempt_get(pos);
	if (e)
		return e->evaluation;
	int16_t eval = 0, i;

	/* doubled pawns */
	eval -= 25 * popcount(pawn_doubled(pos->piece[white][pawn]));
	eval += 25 * popcount(pawn_doubled(pos->piece[black][pawn]));

	/* isolated pawns */
	eval -= 35 * popcount(pawn_isolated(pos->piece[white][pawn]));
	eval += 35 * popcount(pawn_isolated(pos->piece[black][pawn]));

	/* passed pawns */
	int square;
	uint64_t b;
	for (i = 0; i < 8; i++) {
		/* asymmetric because of bit scan */
		if ((b = (pos->piece[white][pawn] & file(i)))) {
			while (b) {
				square = ctz(b);
				b = clear_ls1b(b);
			}
			if (!(pos->piece[black][pawn] & passed_files_white(square)))
				eval += 20 * (square / 8);
		}
		if ((b = (pos->piece[black][pawn] & file(i)))) {
			square = ctz(b);
			if (!(pos->piece[white][pawn] & passed_files_black(square)))
				eval -= 20 * (7 - square / 8);
		}
	}

	/* backward pawns */
	eval -= 30 * popcount(pawn_backward_white(pos->piece[white][pawn], pos->piece[black][pawn]));
	eval += 30 * popcount(pawn_backward_white(pos->piece[white][pawn], pos->piece[black][pawn]));

	/* center control */
	uint64_t center = 0x3C3C000000;
	eval += 20 * popcount((shift_west(shift_north(pos->piece[white][pawn])) | shift_east(shift_north(pos->piece[white][pawn]))) & center);
	eval -= 20 * popcount((shift_west(shift_south(pos->piece[black][pawn])) | shift_east(shift_south(pos->piece[black][pawn]))) & center);

	pawn_store(pos, eval);
	return eval;
}

int pawn_init(void) {
	pawn_table = malloc(PAWN_TABLE_SIZE * sizeof(struct pawn));
	if (!pawn_table)
		return 1;
	memset(pawn_table, 0, PAWN_TABLE_SIZE * sizeof(struct pawn));
	return 0;
}

void pawn_term(void) {
	free(pawn_table);
}
