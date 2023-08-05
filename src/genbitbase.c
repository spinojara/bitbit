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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitbase.h"
#include "position.h"
#include "attackgen.h"

#define BITBASE_MOVES_MAX (10)

uint8_t known[BITBASE_INDEX_MAX] = { 0 };
uint8_t twofold[BITBASE_INDEX_MAX] = { 0 };

struct bitbasemove {
	uint8_t piece;
	uint8_t to;
	uint8_t from;
};

void bitbase_do_move(struct position *pos, const struct bitbasemove *m) {
	pos->piece[pos->turn][m->piece] ^= bitboard(m->from) | bitboard(m->to);
	pos->turn = 1 - pos->turn;
}

void bitbase_undo_move(struct position *pos, const struct bitbasemove *m) {
	pos->piece[1 - pos->turn][m->piece] ^= bitboard(m->from) | bitboard(m->to);
	pos->turn = 1 - pos->turn;
}

static inline void new_bitbasemove(struct bitbasemove *m, int piece, int to, int from) {
	m->piece = piece;
	m->to = to;
	m->from = from;
}

/* BE CAREFUL WITH PROTECTED SQUARES BY PAWN. */
/* Before generating moves we should check if the position
 * is trivially won, i.e. the pawn can be captured. */
void generate_bitbasemoves(const struct position *pos, struct bitbasemove *movelist) {
	int turn = pos->turn;
	/* In this case we have the pawn. */
	if (turn) {
		if (shift_north(pos->piece[white][pawn]) & ~(pos->piece[black][king] | pos->piece[white][king])) {
			int square = ctz(pos->piece[white][pawn]);
			new_bitbasemove(movelist++, pawn, square, square + 8);
		}
	}
	
	int our = ctz(pos->piece[turn][king]);
	int enemy = ctz(pos->piece[1 - turn][king]);
	uint64_t protected = king_attacks(enemy, 0);
	/* We never generate the pawn capture because it will be caught
	 * as a trivial win before move gen. */
	uint64_t attacks = king_attacks(our, pos->piece[white][pawn] | protected);
	while (attacks) {
		int to = ctz(attacks);
		new_bitbasemove(movelist++, king, our, to);
	}
	new_bitbasemove(movelist, 0, 0, 0);
}

int trivial_draw(const struct position *pos) {
	if (pos->turn)
		return 0;
	int pawn_square = ctz(pos->piece[white][pawn]);
	int strong_king = ctz(pos->piece[white][king]);
	int weak_king = ctz(pos->piece[black][king]);
	return distance(weak_king, pawn_square) == 1 && distance(strong_king, pawn_square) > 1;
}

int trivial_win(const struct position *pos) {
	if (!pos->turn)
		return 0;
	int pawn_square = ctz(pos->piece[white][pawn]);
	int promotion_square = pawn_square + 8;
	int strong_king = ctz(pos->piece[white][king]);
	int weak_king = ctz(pos->piece[black][king]);
	if (pawn_square / 8 != 6 || promotion_square == strong_king || promotion_square == weak_king)
		return 0;
	return distance(strong_king, promotion_square) == 1 || distance(weak_king, promotion_square) >= 2;
}

int genbitbase_store(const struct position *pos, int win) {
	bitbase_store(pos, win);
	int index = bitbase_index(pos);
	known[index] = 1;
	return win;
}

int bitbase_search(struct position *pos) {
	int index = bitbase_index(pos);
	if (known[index])
		return bitbase_probe(pos);
	if (twofold[index])
		return BITBASE_DRAW;
	if (trivial_win(pos))
		return genbitbase_store(pos, BITBASE_WIN);
	if (trivial_draw(pos))
		return genbitbase_store(pos, BITBASE_DRAW);
	twofold[index] = 1;

	struct bitbasemove movelist[BITBASE_MOVES_MAX];
	generate_bitbasemoves(pos, movelist);

	for (struct bitbasemove *ptr = movelist; ptr->piece; ptr++) {
		bitbase_do_move(pos, ptr);
		int ret = bitbase_search(pos);
		bitbase_undo_move(pos, ptr);
		if (ret == pos->turn)
			return ret;
	}

	return 1 - pos->turn;
}

void generate_backward(struct position *pos) {
}

int main(void) {
	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	memset(bitbase, 0, sizeof(bitbase));
}
