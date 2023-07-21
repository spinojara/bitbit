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
#include "util.h"
#include "option.h"

/* around 100 KiB with hitrate of ~80% */
#define PAWN_TABLE_SIZE ((uint64_t)1 << 12)

mevalue backward_pawn  = S(-7, 2);
mevalue supported_pawn = S(8, 2);
mevalue passed_pawn    = S(12, 20);
mevalue passed_file    = S(-14, -11);
mevalue isolated_pawn  = S(-11, -14);
mevalue doubled_pawn   = S(-14, -21);
mevalue phalanx_pawn   = S(5, 6);

struct pawn {
	uint64_t pawns[2];
	mevalue evaluation;
};

struct pawn *pawn_table = NULL;

uint16_t hash(uint64_t key) {
	key = key * 0x4cf5ad432745937full;
	key = key ^ (key >> 23) ^ (key >> 46);
	return key % PAWN_TABLE_SIZE;
}

struct pawn *pawn_get(uint64_t *pawns) {
	return pawn_table + hash((pawns[black] | pawns[white]) >> 8);
}

struct pawn *pawn_attempt_get(uint64_t *pawns) {
	if (!option_pawn)
		return NULL;
	struct pawn *p = pawn_get(pawns);
	if (p->pawns[white] != pawns[white] || p->pawns[black] != pawns[black])
		return NULL;
	return p;
}

void pawn_store(uint64_t *pawns, mevalue evaluation) {
	if (!option_pawn)
		return;
	struct pawn *e = pawn_get(pawns);
	e->pawns[white] = pawns[white];
	e->pawns[black] = pawns[black];
	e->evaluation = evaluation;
}

/* mostly inspiration from stockfish */
mevalue evaluate_pawns(const struct position *pos, struct evaluationinfo *ei, int color) {
	UNUSED(ei);
	uint64_t pawns[2];
	if (color) {
		pawns[white] = pos->piece[white][pawn];
		pawns[black] = pos->piece[black][pawn];
	}
	else {
		pawns[white] = rotate_bytes(pos->piece[black][pawn]);
		pawns[black] = rotate_bytes(pos->piece[white][pawn]);
	}
	struct pawn *e = pawn_attempt_get(pawns);
	if (e)
		return e->evaluation;
	/* we are now always evaluation from white's perspective */
	mevalue eval = 0;

	uint64_t b = pawns[white];
	uint64_t neighbours, doubled, stoppers, support, phalanx, lever, leverpush, blockers;
	int backward, passed;
	int square;
	uint64_t squareb;
	while (b) {
		square = ctz(b);
		squareb = bitboard(square);

		int y = square / 8;
		int x = square % 8;
		
		/* uint64_t */
		doubled    = pawns[white] & bitboard(square - 8);
		neighbours = pawns[white] & adjacent_files(square);
		stoppers   = pawns[black] & passed_files(square, white);
		blockers   = pawns[black] & bitboard(square + 8);
		support    = neighbours & rank(square - 8);
		phalanx    = neighbours & rank(square);
		lever      = pawns[black] & (shift_north_west(squareb) | shift_north_east(squareb));
		leverpush  = pawns[black] & (shift_north(shift_north_west(squareb) | shift_north_east(squareb)));

		/* int */
		backward   = !(neighbours & passed_files(square + 8, black)) && (leverpush | blockers);
		passed     = !(stoppers ^ lever) || (!(stoppers ^ lever ^ leverpush) && popcount(phalanx) >= popcount(leverpush));
		passed    &= !(passed_files(square, white) & file(square) & pawns[white]);

		if (backward) {
			ei->backward_pawn[color] += 1;
			eval += backward_pawn;
		}

		if (support) {
			ei->supported_pawn[color] += (y - 1) * popcount(support);
			eval += supported_pawn * (y - 1) * popcount(support);
		}

		if (phalanx) {
			ei->phalanx_pawn[color] += y - 1;
			eval += phalanx_pawn * (y - 1);
		}

		if (passed) {
			ei->passed_pawn[color] += y;
			ei->passed_file[color] += MIN(x, 7 - x);
			eval += passed_pawn * y + passed_file * MIN(x, 7 - x);
		}

		if (!neighbours) {
			ei->isolated_pawn[color] += 1;
			eval += isolated_pawn;
		}
		
		if (!support && doubled) {
			ei->doubled_pawn[color] += 1;
			eval += doubled_pawn;
		}

		b = clear_ls1b(b);
	}

	pawn_store(pawns, eval);
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
