/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2024 Isak Ellmer
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

#include "moveorder.h"

#include <stdlib.h>

#include "util.h"
#include "bitboard.h"
#include "attackgen.h"
#include "evaluate.h"
#include "transposition.h"

int mvv_lva_lookup[7 * 7];

unsigned move_order_piece_value[] = { 0, 100, 300, 300, 500, 900, 0 };

void mvv_lva_init(void) {
	for (int attacker = 0; attacker < 7; attacker++)
		for (int victim = 0; victim < 7; victim++)
			mvv_lva_lookup[attacker + 7 * victim] = move_order_piece_value[victim] -
			                                         move_order_piece_value[attacker];
}

int see_geq(struct position *pos, const move_t *move, int32_t value) {
	if (move_flag(move) == MOVE_EN_PASSANT)
		return 0 >= value;
	const int us = pos->turn;
	const int them = other_color(us);

	int from = move_from(move);
	int to = move_to(move);
	uint64_t fromb = bitboard(from);
	uint64_t tob = bitboard(to);

	int attacker = uncolored_piece(pos->mailbox[from]);
	int victim = uncolored_piece(pos->mailbox[to]);

	int32_t swap = move_order_piece_value[victim] - value;
	if (swap < 0)
		return 0;

	swap = move_order_piece_value[attacker] - swap;
	if (swap <= 0)
		return 1;

	pos->piece[them][victim] ^= tob;
	pos->piece[them][ALL] ^= tob;
	pos->piece[us][attacker] ^= tob | fromb;
	pos->piece[us][ALL] ^= tob | fromb;

	int turn = us;

	uint64_t attackers = 0, turnattackers, occupied = pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL];
	/* Speed is more important than accuracy so we only
	 * generate moves which are probably legal.
	 */
	attackers |= (shift(tob, S | E) | shift(tob, S | W)) & pos->piece[WHITE][PAWN];
	attackers |= (shift(tob, N | E) | shift(tob, N | W)) & pos->piece[BLACK][PAWN];

	attackers |= knight_attacks(to, 0) & (pos->piece[WHITE][KNIGHT] | pos->piece[BLACK][KNIGHT]);

	attackers |= bishop_attacks(to, 0, occupied) &
		(pos->piece[WHITE][BISHOP] | pos->piece[BLACK][BISHOP]);

	attackers |= rook_attacks(to, 0, occupied) &
		(pos->piece[WHITE][ROOK] | pos->piece[BLACK][ROOK]);

	attackers |= queen_attacks(to, 0, occupied) &
		(pos->piece[WHITE][QUEEN] | pos->piece[BLACK][QUEEN]);

	attackers |= king_attacks(to, 0) & (pos->piece[WHITE][KING] | pos->piece[BLACK][KING]);

	uint64_t pinned[2]      = { generate_pinned(pos, BLACK) & attackers, generate_pinned(pos, WHITE) & attackers };
	uint64_t discovery[2]   = { pinned[WHITE] & pos->piece[BLACK][ALL], pinned[BLACK] & pos->piece[WHITE][ALL] };
	uint64_t pinners[2]     = { generate_pinners(pos, pinned[BLACK], BLACK), generate_pinners(pos, pinned[WHITE], WHITE) };

	int ret = 1;

	while (1) {
		turn = other_color(turn);
		
		attackers &= occupied;

		turnattackers = attackers & pos->piece[turn][ALL];

		if (pinners[turn] & occupied)
			turnattackers &= ~pinned[turn];

		if (!turnattackers)
			break;

		ret = 1 - ret;

		if (turnattackers & pos->piece[turn][PAWN]) {
			/* x < ret because x < 1 is same as <= 0. */
			if ((swap = move_order_piece_value[PAWN] - swap) < ret)
				break;

			occupied ^= ls1b(turnattackers);
			/* Add x-ray pieces. */
			attackers |= bishop_attacks(to, 0, occupied) & (pos->piece[WHITE][BISHOP] | pos->piece[WHITE][QUEEN] |
									pos->piece[BLACK][BISHOP] | pos->piece[BLACK][QUEEN]);
		}
		else if (turnattackers & pos->piece[turn][KNIGHT]) {
			if ((swap = move_order_piece_value[KNIGHT] - swap) < ret)
				break;

			occupied ^= ls1b(turnattackers);
		}
		else if (turnattackers & pos->piece[turn][BISHOP]) {
			if ((swap = move_order_piece_value[BISHOP] - swap) < ret)
				break;

			occupied ^= ls1b(turnattackers);
			attackers |= bishop_attacks(to, 0, occupied) & (pos->piece[WHITE][BISHOP] | pos->piece[WHITE][QUEEN] |
									pos->piece[BLACK][BISHOP] | pos->piece[BLACK][QUEEN]);
		}
		else if (turnattackers & pos->piece[turn][ROOK]) {
			if ((swap = move_order_piece_value[ROOK] - swap) < ret)
				break;

			occupied ^= ls1b(turnattackers);
			attackers |= rook_attacks(to, 0, occupied) & (pos->piece[WHITE][ROOK] | pos->piece[WHITE][QUEEN] |
									pos->piece[BLACK][ROOK] | pos->piece[BLACK][QUEEN]);
		}
		else if (turnattackers & pos->piece[turn][QUEEN]) {
			if ((swap = move_order_piece_value[QUEEN] - swap) < ret)
				break;

			occupied ^= ls1b(turnattackers);
			attackers |= bishop_attacks(to, 0, occupied) & (pos->piece[WHITE][BISHOP] | pos->piece[WHITE][QUEEN] |
									pos->piece[BLACK][BISHOP] | pos->piece[BLACK][QUEEN]);
			attackers |= rook_attacks(to, 0, occupied) & (pos->piece[WHITE][ROOK] | pos->piece[WHITE][QUEEN] |
									pos->piece[BLACK][ROOK] | pos->piece[BLACK][QUEEN]);
		}
		/* King. */
		else {
			/* We lose if other side still has attackers. */
			if (attackers & pos->piece[other_color(turn)][ALL])
				ret = 1 - ret;
			break;
		}
		/* A discovered check occured. */
		if (discovery[turn] & ~occupied)
			break;
	}
	pos->piece[them][victim] ^= tob;
	pos->piece[them][ALL] ^= tob;
	pos->piece[us][attacker] ^= tob | fromb;
	pos->piece[us][ALL] ^= tob | fromb;
	return ret;
}

/* For quiet moves. */
uint64_t generate_defenders(struct position *pos, const move_t *move) {
	const int us = pos->turn;
	assert(!pos->mailbox[move_to(move)]);

	int from_square = move_from(move);
	int to_square = move_to(move);
	uint64_t from = bitboard(from_square);
	uint64_t to = bitboard(to_square);
	int attacker = uncolored_piece(pos->mailbox[from_square]);

	pos->piece[us][attacker] ^= to | from;
	pos->piece[us][ALL] ^= to | from;

	uint64_t ret = generate_attackers(pos, to_square, us);

	pos->piece[us][attacker] ^= to | from;
	pos->piece[us][ALL] ^= to | from;

	return ret;
}

void moveorder_init(void) {
	mvv_lva_init();
}
