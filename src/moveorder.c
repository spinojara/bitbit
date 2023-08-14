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

#include "moveorder.h"

#include <stdlib.h>

#include "util.h"
#include "bitboard.h"
#include "attackgen.h"
#include "evaluate.h"
#include "transposition.h"

int mvv_lva_lookup[13 * 13];

unsigned move_order_piece_value[] = { 0, 100, 300, 300, 500, 900, 0 };

void mvv_lva_init(void) {
	for (int attacker = 0; attacker < 13; attacker++)
		for (int victim = 0; victim < 13; victim++)
			mvv_lva_lookup[attacker + 13 * victim] = move_order_piece_value[uncolored_piece(victim)] -
			                                         move_order_piece_value[uncolored_piece(attacker)];
}

int see_geq(struct position *pos, const move *m, int32_t value) {
	int from = move_from(m);
	int to = move_to(m);
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

	pos->piece[other_color(pos->turn)][victim] ^= tob;
	pos->piece[other_color(pos->turn)][all] ^= tob;
	pos->piece[pos->turn][attacker] ^= tob | fromb;
	pos->piece[pos->turn][all] ^= tob | fromb;

	int turn = pos->turn;

	uint64_t b;
	
	uint64_t attackers = 0, turnattackers, occupied = pos->piece[white][all] | pos->piece[black][all];
	/* Speed is more important than accuracy so we only
	 * generate moves which are probably legal.
	 */
	attackers |= (shift_south_west(tob) | shift_south_east(tob)) & pos->piece[white][pawn];
	attackers |= (shift_north_west(tob) | shift_north_east(tob)) & pos->piece[black][pawn];

	attackers |= knight_attacks(to, 0) & (pos->piece[white][knight] | pos->piece[black][knight]);

	attackers |= bishop_attacks(to, 0, occupied) &
		(pos->piece[white][bishop] | pos->piece[black][bishop]);

	attackers |= rook_attacks(to, 0, occupied) &
		(pos->piece[white][rook] | pos->piece[black][rook]);

	attackers |= queen_attacks(to, 0, occupied) &
		(pos->piece[white][queen] | pos->piece[black][queen]);

	uint64_t pinned[2] = { generate_pinned(pos, black) & attackers, generate_pinned(pos, white) & attackers };
	uint64_t pinners[2] = { generate_pinners(pos, pinned[black], black), generate_pinners(pos, pinned[white], white) };

	int ret = 1;

	while (1) {
		turn = other_color(turn);
		
		attackers &= occupied;

		turnattackers = attackers & pos->piece[turn][all];

		if (pinners[other_color(turn)] & occupied)
			turnattackers &= ~pinned[turn];

		if (!turnattackers)
			break;

		ret = 1 - ret;

		if ((b = turnattackers) & pos->piece[turn][pawn]) {
			/* x < ret because x < 1 is same as <= 0 */
			if ((swap = move_order_piece_value[pawn] - swap) < ret)
				break;

			occupied ^= ls1b(b);
			/* add x-ray pieces */
			attackers |= bishop_attacks(to, 0, occupied) & (pos->piece[white][bishop] | pos->piece[white][queen] |
									pos->piece[black][bishop] | pos->piece[black][queen]);
		}
		else if ((b = turnattackers) & pos->piece[turn][knight]) {
			if ((swap = move_order_piece_value[knight] - swap) < ret)
				break;

			occupied ^= ls1b(b);
		}
		else if ((b = turnattackers) & pos->piece[turn][bishop]) {
			if ((swap = move_order_piece_value[bishop] - swap) < ret)
				break;

			occupied ^= ls1b(b);
			attackers |= bishop_attacks(to, 0, occupied) & (pos->piece[white][bishop] | pos->piece[white][queen] |
									pos->piece[black][bishop] | pos->piece[black][queen]);
		}
		else if ((b = turnattackers) & pos->piece[turn][rook]) {
			if ((swap = move_order_piece_value[rook] - swap) < ret)
				break;

			occupied ^= ls1b(b);
			attackers |= rook_attacks(to, 0, occupied) & (pos->piece[white][rook] | pos->piece[white][queen] |
									pos->piece[black][rook] | pos->piece[black][queen]);
		}
		else if ((b = turnattackers) & pos->piece[turn][queen]) {
			if ((swap = move_order_piece_value[queen] - swap) < ret)
				break;

			occupied ^= ls1b(b);
			attackers |= bishop_attacks(to, 0, occupied) & (pos->piece[white][bishop] | pos->piece[white][queen] |
									pos->piece[black][bishop] | pos->piece[black][queen]);
			attackers |= rook_attacks(to, 0, occupied) & (pos->piece[white][rook] | pos->piece[white][queen] |
									pos->piece[black][rook] | pos->piece[black][queen]);
		}
		/* king */
		else {
			pos->piece[other_color(pos->turn)][victim] ^= tob;
			pos->piece[other_color(pos->turn)][all] ^= tob;
			pos->piece[pos->turn][attacker] ^= tob | fromb;
			pos->piece[pos->turn][all] ^= tob | fromb;
			/* we lose if other side still has attackers */
			return (attackers & pos->piece[other_color(turn)][all]) ? 1 - ret : ret;
		}
	}
	pos->piece[other_color(pos->turn)][victim] ^= tob;
	pos->piece[other_color(pos->turn)][all] ^= tob;
	pos->piece[pos->turn][attacker] ^= tob | fromb;
	pos->piece[pos->turn][all] ^= tob | fromb;
	return ret;
}

void moveorder_init(void) {
	mvv_lva_init();
}
