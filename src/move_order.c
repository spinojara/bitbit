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

#include "move_order.h"

#include <stdlib.h>

#include "init.h"
#include "util.h"
#include "bitboard.h"
#include "attack_gen.h"
#include "evaluate.h"
#if defined(TRANSPOSITION)
#include "transposition_table.h"
#endif

struct moveorderinfo {
	uint64_t pawn_attacks;
	uint64_t check_squares[7];
};

int mvv_lva_lookup[13 * 13];

static inline uint64_t mvv_lva(int attacker, int victim) {
	return mvv_lva_lookup[attacker + 13 * victim];
}

unsigned move_order_piece_value[] = { 0, 100, 300, 300, 500, 900, 0 };

void mvv_lva_init() {
	for (int attacker = 0; attacker < 13; attacker++)
		for (int victim = 0; victim < 13; victim++)
			mvv_lva_lookup[attacker + 13 * victim] = move_order_piece_value[victim % 6] -
			                                         move_order_piece_value[attacker % 6];
}

int see_geq(struct position *pos, const move *m, int16_t value) {
	int from = move_from(m);
	int to = move_to(m);
	uint64_t fromb = bitboard(from);
	uint64_t tob = bitboard(to);

	int attacker = pos->mailbox[from] % 6;
	int victim = pos->mailbox[to] % 6;

	int16_t swap = move_order_piece_value[victim] - value;
	if (swap < 0)
		return 0;

	swap = move_order_piece_value[attacker] - swap;
	if (swap <= 0)
		return 1;

	pos->piece[1 - pos->turn][victim] ^= tob;
	pos->piece[1 - pos->turn][all] ^= tob;
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
		turn = 1 - turn;
		
		attackers &= occupied;

		turnattackers = attackers & pos->piece[turn][all];

		if (pinners[1 - turn] & occupied)
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
			pos->piece[1 - pos->turn][victim] ^= tob;
			pos->piece[1 - pos->turn][all] ^= tob;
			pos->piece[pos->turn][attacker] ^= tob | fromb;
			pos->piece[pos->turn][all] ^= tob | fromb;
			/* we lose if other side still has attackers */
			return (attackers & pos->piece[1 - turn][all]) ? 1 - ret : ret;
		}
	}
	pos->piece[1 - pos->turn][victim] ^= tob;
	pos->piece[1 - pos->turn][all] ^= tob;
	pos->piece[pos->turn][attacker] ^= tob | fromb;
	pos->piece[pos->turn][all] ^= tob | fromb;
	return ret;
}

uint64_t evaluate_move(struct position *pos, move *m, uint8_t depth, uint8_t ply, void *e, struct searchinfo *si, struct moveorderinfo *mi) {
	UNUSED(depth);
	/* pv */
	if (si->pv_flag && si->pv_moves[0][ply] == (*m & 0xFFFF))
		return 0xF000000000000001;

	/* transposition table */
#if defined(TRANSPOSITION)
	if (e && *m == transposition_move(e))
		return 0xF000000000000000;
#else
	UNUSED(e);
#endif
	const int piece = pos->mailbox[move_from(m)];
	const int capture = pos->mailbox[move_to(m)];
	const uint64_t to = bitboard(move_to(m));
	const uint64_t from = bitboard(move_from(m));
	int enemy_king_square = ctz(pos->piece[1 - pos->turn][king]);

	/* queen promotion */
	if (move_flag(m) == 2 && move_promote(m) == 3)
		return 0x1000000000000000;

	int discovered_check = 0;
	uint64_t check_squares;
	uint64_t all_pieces = (pos->piece[white][all] | pos->piece[black][all]) ^ from;
	if (!(to & king_attacks(enemy_king_square, 0))) {
		if (piece % 6 == knight) {
			check_squares = bishop_attacks(enemy_king_square, 0, all_pieces);
			if (check_squares & (pos->piece[pos->turn][bishop] | pos->piece[pos->turn][queen]))
				discovered_check = 1;

			check_squares = rook_attacks(enemy_king_square, 0, all_pieces);
			if (check_squares & (pos->piece[pos->turn][rook] | pos->piece[pos->turn][queen]))
				discovered_check = 1;
		}
		else if (piece % 6 == bishop) {
			check_squares = rook_attacks(enemy_king_square, 0, all_pieces);
			if (check_squares & (pos->piece[pos->turn][rook] | pos->piece[pos->turn][queen]))
				discovered_check = 1;
		}
		else if (piece % 6 == rook) {
			check_squares = bishop_attacks(enemy_king_square, 0, all_pieces);
			if (check_squares & (pos->piece[pos->turn][bishop] | pos->piece[pos->turn][queen]))
				discovered_check = 1;
		}
	}

	/* captures */
	if (capture) {
		uint64_t eval = 0;

		if (discovered_check || see_geq(pos, m, 100))
			eval += 0x100000000000000;
		else if (see_geq(pos, m, 0))
			eval += 0x10000000000000;
		else if (see_geq(pos, m, -100))
			eval += SEE_VALUE_MINUS_100;
		else
			eval += 0x100000000000;

		eval += discovered_check ? move_order_piece_value[capture % 6] : mvv_lva(piece, capture);
		return eval;
	}

	/* low evaluation for moves to squares defended by pawns,
	 * unless there is a discovered check
	 */
	if ((to & mi->pawn_attacks) && piece % 6 != pawn && !discovered_check)
		return 0;

	/* killer */
	if (si->killer_moves[ply][0] == *m)
		return 0x8000000000001;
	if (si->killer_moves[ply][1] == *m)
		return 0x8000000000000;

	uint64_t eval = 0x2;
	/* higher evaluation for moves from squares defended by pawns */
	if ((from & mi->pawn_attacks) && piece % 6 != pawn)
		eval += 0x100;

	if (discovered_check)
		eval += 0x10000;
	/* checks */
	if (pos->mailbox[move_from(m)] % 6 && mi->check_squares[piece % 6] & to)
		eval += 0x1000;

	/* history */
	eval += si->history_moves[piece][move_to(m)];
	return eval;
}

void moveorderinfo_init(const struct position *pos, struct moveorderinfo *mi) {
	mi->pawn_attacks = shift_color_west(pos->piece[1 - pos->turn][pawn], 1 - pos->turn) | shift_color_east(pos->piece[1 - pos->turn][pawn], 1 - pos->turn);
	int king_square = ctz(pos->piece[1 - pos->turn][king]);
	mi->check_squares[pawn] = shift_color_west(bitboard(king_square), 1 - pos->turn) | shift_color_east(bitboard(king_square), 1 - pos->turn);
	mi->check_squares[knight] = knight_attacks(king_square, 0);
	mi->check_squares[bishop] = bishop_attacks(king_square, 0, all_pieces(pos));
	mi->check_squares[rook] = rook_attacks(king_square, 0, all_pieces(pos));
	mi->check_squares[queen] = mi->check_squares[bishop] | mi->check_squares[rook];
}

void next_move(move *move_list, uint64_t *evaluation_list, move **ptr) {
	++*ptr;
	int first = *ptr - move_list;
	int index = first;
	uint64_t max_value = 0;
	for (int i = index; move_list[i]; i++) {
		if (evaluation_list[i] > max_value) {
			max_value = evaluation_list[i];
			index = i;
		}
	}
	if (index != first) {
		uint64_t t;
		t = move_list[index];
		move_list[index] = move_list[first];
		move_list[first] = t;
		t = evaluation_list[index];
		evaluation_list[index] = evaluation_list[first];
		evaluation_list[first] = t;
	}
}

move *order_moves(struct position *pos, move *move_list, uint64_t *evaluation_list, uint8_t depth, uint8_t ply, void *e, struct searchinfo *si) {
	struct moveorderinfo mi;
	moveorderinfo_init(pos, &mi);
	for (int i = 0; move_list[i]; i++)
		evaluation_list[i] = evaluate_move(pos, move_list + i, depth, ply, e, si, &mi);
	move *ptr = move_list - 1;
	next_move(move_list, evaluation_list, &ptr);
	return ptr;
}

void move_order_init(void) {
	mvv_lva_init();
}
