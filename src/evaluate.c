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

#include "evaluate.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "bitboard.h"
#include "move_gen.h"
#include "move.h"
#include "util.h"
#include "transposition_table.h"
#include "init.h"
#include "position.h"
#include "interrupt.h"

uint64_t nodes = 0;

int pv_flag = 0;

uint64_t mvv_lva_lookup[13 * 13];

int eval_table[13][64];

int piece_value[13] = { 0, 100, 300, 315, 500, 900, 0, -100, -300, -315, -500, -900, 0 };

int white_side_eval_table[6][64] = {
	{
		  0,   0,   0,   0,   0,   0,   0,   0,
		 50,  50,  50,  50,  50,  50,  50,  50,
		 10,  10,  20,  30,  30,  20,  10,  10,
		  5,   5,  10,  25,  25,  10,   5,   5,
		  0,   0,   0,  20,  20,   0,   0,   0,
		  5,  -5, -10,   0,   0, -10,  -5,   5,
		  5,  10,  10, -20, -20,  10,  10,   5,
		  0,   0,   0,   0,   0,   0,   0,   0
	}, {
		-50, -40, -30, -30, -30, -30, -40, -50,
		-40, -20,   0,   0,   0,   0, -20, -40,
		-30,   0,  10,  15,  15,  10,   0, -30,
		-30,   5,  15,  20,  20,  15,   5, -30,
		-30,   0,  15,  20,  20,  15,   0, -30,
		-30,   5,  10,  15,  15,  10,   5, -30,
		-40, -20,   0,   5,   5,   0, -20, -40,
		-50, -40, -30, -30, -30, -30, -40, -50
	}, {
		-20, -10, -10, -10, -10, -10, -10, -20,
		-10,   0,   0,   0,   0,   0,   0, -10,
		-10,   0,   5,  10,  10,   5,   0, -10,
		-10,   5,   5,  10,  10,   5,   5, -10,
		-10,   0,  10,  10,  10,  10,   0, -10,
		-10,  10,  10,  10,  10,  10,  10, -10,
		-10,   5,   0,   0,   0,   0,   5, -10,
		-20, -10, -10, -10, -10, -10, -10, -20
	}, {
		  0,   0,   0,   0,   0,   0,   0,   0,
		  5,  10,  10,  10,  10,  10,  10,   5,
		 -5,   0,   0,   0,   0,   0,   0,  -5,
		 -5,   0,   0,   0,   0,   0,   0,  -5,
		 -5,   0,   0,   0,   0,   0,   0,  -5,
		 -5,   0,   0,   0,   0,   0,   0,  -5,
		 -5,   0,   0,   0,   0,   0,   0,  -5,
		  0,   0,   0,   5,   5,   0,   0,   0
	}, {
		-20, -10, -10,  -5,  -5, -10, -10, -20,
		-10,   0,   0,   0,   0,   0,   0, -10,
		-10,   0,   5,   5,   5,   5,   0, -10,
		 -5,   0,   5,   5,   5,   5,   0,  -5,
		  0,   0,   5,   5,   5,   5,   0,  -5,
		-10,   5,   5,   5,   5,   5,   0, -10,
		-10,   0,   5,   0,   0,   0,   0, -10,
		-20, -10, -10,  -5,  -5, -10, -10, -20
	}, {
		-30, -40, -40, -50, -50, -40, -40, -30,
		-30, -40, -40, -50, -50, -40, -40, -30,
		-30, -40, -40, -50, -50, -40, -40, -30,
		-30, -40, -40, -50, -50, -40, -40, -30,
		-20, -30, -30, -40, -40, -30, -30, -20,
		-10, -20, -20, -20, -20, -20, -20, -10,
		 20,  20,   0,   0,   0,   0,  20,  20,
		 20,  30,  10,   0,   0,  10,  30,  20
	}

};

uint64_t mvv_lva_calc(int attacker, int victim) {
	int a = (attacker - 1) % 6;
	int v = (victim - 1) % 6;
	int lookup_t[6 * 6] = {
		 2, 15, 16, 17, 21,  0,
		 0,  3,  7, 14, 20,  0,
		 0,  1,  4, 13, 19,  0,
		 0,  0,  0,  5, 18,  0,
		 0,  0,  0,  0,  6,  0,
		 8,  9, 10, 11, 12,  0,
	};
	if (!lookup_t[v + 6 * a])
		return 0xFFFFFFFFFFFF0000;
	return lookup_t[v + 6 * a] + 0xFFFFFFFFFFFFFF00;
}

void evaluate_init() {
	for (int i = 0; i < 13; i++) {
		for (int j  = 0; j < 64; j++) {
			if (i == 0)
				eval_table[i][j] = 0;
			else if (i < 7) {
				eval_table[i][j] = white_side_eval_table[i - 1][(7 - j / 8) * 8 + (j % 8)] +
					           piece_value[i];
			}
			else
				eval_table[i][j] = -white_side_eval_table[i - 7][j] +
						   piece_value[i];
			init_status("populating evaluation lookup table");
		}
		for (int j = 0; j < 13; j++) {
			mvv_lva_lookup[j + 13 * i] = mvv_lva_calc(i, j);
			init_status("populating mvv lva lookup table");
		}
	}
}

void print_pv(struct position *pos, move pv_moves[256][256]) {
	int i;
	char str[256][8];
	struct position pos_copy[1];
	copy_position(pos_copy, pos);
	for (i = 0; i < 256 && pv_moves[0][i]; i++) {
		move_str_pgn(str[i], pos_copy, &(pv_moves[0][i]));
		if (!string_to_move(pos_copy, str[i]))
			break;
		printf("%s", str[i]);
		do_move(pos_copy, &(pv_moves[0][i]));
		pv_moves[0][i] = pv_moves[0][i] & 0xFFFF;
		if (i != 255 && pv_moves[0][i + 1])
			printf(" ");
	}
}

int is_threefold(struct position *pos, struct history *history) {
	int count;
	struct history *t;
	for (t = history, count = 0; t; t = t->previous)
		if (pos_are_equal(pos, t->pos))
			count++;
	return count >= 2;
}

static inline uint64_t mvv_lva(int attacker, int victim) {
	return mvv_lva_lookup[victim + 13 * attacker];
}

static inline void store_killer_move(move *m, uint8_t ply, move killer_moves[][2]) {
	if (!killer_moves)
		return;
	killer_moves[ply][1] = killer_moves[ply][0];
	killer_moves[ply][0] = *m & 0xFFFF;
}

static inline void store_history_move(struct position *pos, move *m, uint8_t depth, uint64_t history_moves[13][64]) {
	history_moves[pos->mailbox[move_from(m)]][move_to(m)] += (uint64_t)1 << depth;
}

static inline void store_pv_move(move *m, uint8_t ply, move pv_moves[256][256]) {
	if (!pv_moves)
		return;
	pv_moves[ply][ply] = *m & 0xFFFF;
	for (int i = ply + 1; i < 256 && pv_moves[ply + 1][i]; i++)
		pv_moves[ply][i] = pv_moves[ply + 1][i];
}

int contains_pv_move(move *move_list, uint8_t ply, move pv_moves[256][256]) {
	if (!pv_moves)
		return 0;
	for (move *ptr = move_list; *ptr; ptr++)
		if ((*ptr & 0xFFFF) == (pv_moves[0][ply] & 0xFFFF))
			return 1;
	return 0;
}

uint64_t evaluate_move(struct position *pos, move *m, uint8_t ply, struct transposition *e, move pv_moves[256][256], move killer_moves[][2], uint64_t history_moves[13][64]) {
	/* pv */
	if (pv_flag && pv_moves && (pv_moves[0][ply] & 0xFFFF) == (*m & 0xFFFF))
		return 0xFFFFFFFFFFFFFFFF;

	/* transposition table */
	if (e && *m == transposition_move(e))
		return 0xFFFFFFFFFFFFFFFE;

	/* attack */
	if (pos->mailbox[move_to(m)])
		return mvv_lva(pos->mailbox[move_from(m)], pos->mailbox[move_to(m)]);

	/* promotions */
	if (move_flag(m) == 2)
		return 0xFFFFFFFFFFFFF000 + move_promote(m);

	/* killer */
	if (killer_moves) {
		if (killer_moves[ply][0] == *m)
			return 0xFFFFFFFFFFFF0002;
		if (killer_moves[ply][1] == *m)
			return 0xFFFFFFFFFFFF0001;
	}

	/* history */
	if (history_moves)
		return history_moves[pos->mailbox[move_from(m)]][move_to(m)];
	return 0;
}

/* 1. pv
 * 2. tt
 * 3. mvv lva winning
 * 4. promotions
 * 5. killer
 * 6. mvv lva losing
 * 7. history
 */
void evaluate_moves(struct position *pos, move *move_list, uint8_t depth, struct transposition *e, move pv_moves[256][256], move killer_moves[][2], uint64_t history_moves[13][64]) {
	uint64_t evaluation_list[256];
	int i;
	for (i = 0; move_list[i]; i++)
		evaluation_list[i] = evaluate_move(pos, move_list + i, depth, e, pv_moves, killer_moves, history_moves);

	merge_sort(move_list, evaluation_list, 0, i - 1, 0);
}

int pawn_structure(struct position *pos) {
	int eval = 0, i;
	uint64_t t;

	/* doubled pawns */
	for (i = 0; i < 8; i++) {
		if ((t = pos->white_pieces[pawn] & file(i)))
			if (popcount(t) > 1)
				eval -= 40;
		if ((t = pos->black_pieces[pawn] & file(i)))
			if (popcount(t) > 1)
				eval += 40;
	}

	/* isolated pawns */

	/* passed pawns */

	return eval;
}

int16_t count_position(struct position *pos) {
	nodes++;
	int eval = 0, i;
	for (i = 0; i < 64; i++)
		eval += eval_table[pos->mailbox[i]][i];
	eval += 3 * (mobility_white(pos) - mobility_black(pos)) / 2;
	eval += pawn_structure(pos);
	return pos->turn ? eval : -eval;
}

int16_t quiescence(struct position *pos, int16_t alpha, int16_t beta, clock_t clock_stop) {
	if (interrupt)
		return 0;
	if (nodes % 4096 == 0)
		if (clock_stop && clock() > clock_stop)
			interrupt = 1;

	int16_t evaluation;
	evaluation = count_position(pos);
	if (evaluation >= beta)
		return beta;
	if (evaluation > alpha)
		alpha = evaluation;

	move move_list[256];
	generate_quiescence(pos, move_list);
	if (!move_list[0])
		return evaluation;

	evaluate_moves(pos, move_list, 0, NULL, NULL, NULL, NULL);
	for (move *ptr = move_list; *ptr; ptr++) {
		do_move(pos, ptr);
		evaluation = -quiescence(pos, -beta, -alpha, clock_stop);
		undo_move(pos, ptr);
		if (evaluation >= beta)
			return beta;
		if (evaluation > alpha)
			alpha = evaluation;
	}
	return alpha;
}

int16_t evaluate_recursive(struct position *pos, uint8_t depth, uint8_t ply, int16_t alpha, int16_t beta, int null_move, clock_t clock_stop, move pv_moves[][256], move killer_moves[][2], uint64_t history_moves[13][64]) {
	if (interrupt)
		return 0;
	if (nodes % 4096 == 0)
		if (clock_stop && clock() > clock_stop)
			interrupt = 1;

	struct transposition *e = attempt_get(pos);
	if (e && transposition_open(e))
		return 0;
	if (e && transposition_depth(e) >= depth && transposition_type(e) == 0)
		return transposition_evaluation(e);

	int16_t evaluation;
	if (depth <= 0) {
		evaluation = mate(pos);
		/* stalemate */
		if (evaluation == 1)
			return 0;
		/* checkmate */
		if (evaluation == 2)
			return -0x7F00;
		return quiescence(pos, alpha, beta, clock_stop);
	}

	/* null move pruning */
	uint64_t checkers = generate_checkers(pos);
	if (!null_move && !pv_flag && !checkers && depth >= 3 && has_big_piece(pos)) {
		int t = pos->en_passant;
		do_null_move(pos, 0);
		evaluation = -evaluate_recursive(pos, depth - 3, ply + 1, -beta, -beta + 1, 1, clock_stop, NULL, NULL, history_moves);
		do_null_move(pos, t);
		if (evaluation >= beta)
			return beta;
	}

	move move_list[256];
	generate_all(pos, move_list);

	if (!move_list[0]) {
		if (!checkers)
			return 0;
		return -0x7F00;
	}

	if (pv_flag && !contains_pv_move(move_list, ply, pv_moves))
		pv_flag = 0;

	evaluate_moves(pos, move_list, depth, e, pv_moves, killer_moves, history_moves);

	if (pos->halfmove >= 100)
		return 0;
	
	if (e)
		transposition_set_open(e);

	uint16_t m = 0;
	for (move *ptr = move_list; *ptr; ptr++) {
		do_move(pos, ptr);
		if (ptr == move_list || pv_flag) {
			/* -beta - 1 to open the window and search for mate in n */
			evaluation = -evaluate_recursive(pos, depth - 1, ply + 1, -beta - 1, -alpha, 0, clock_stop, pv_moves, killer_moves, history_moves);
		}
		else {
			/* late move reduction */
			if (depth >= 3 && !checkers && ptr - move_list >= 3 && move_flag(ptr) != 2 && !move_capture(ptr)) {
				uint8_t r = ptr - move_list >= 4 ? depth / 3 : 1;
				evaluation = -evaluate_recursive(pos, depth - 1 - MIN(r, depth - 1), ply + 1, -alpha - 1, -alpha, 0, clock_stop, pv_moves, killer_moves, history_moves);
			}
			else {
				evaluation = alpha + 1;
			}
			if (evaluation > alpha) {
				evaluation = -evaluate_recursive(pos, depth - 1, ply + 1, -alpha - 1, -alpha, 0, clock_stop, pv_moves, killer_moves, history_moves);
				if (evaluation > alpha && evaluation < beta)
					evaluation = -evaluate_recursive(pos, depth - 1, ply + 1, -beta - 1, -alpha, 0, clock_stop, pv_moves, killer_moves, history_moves);
			}
		}
		evaluation -= (evaluation > 0x4000);
		undo_move(pos, ptr);
		if (evaluation >= beta) {
			/* quiet */
			if (!pos->mailbox[move_to(ptr)])
				store_killer_move(ptr, ply, killer_moves);
			if (e)
				transposition_set_closed(e);
			/* type cut */
			attempt_store(pos, beta, depth, 1, *ptr);
			return beta;
		}
		if (evaluation > alpha) {
			/* quiet */
			if (!pos->mailbox[move_to(ptr)])
				store_history_move(pos, ptr, depth, history_moves);
			alpha = evaluation;
			store_pv_move(ptr, ply, pv_moves);
			m = *ptr & 0xFFFF;
		}
	}
	if (e)
		transposition_set_closed(e);
	/* type pv or all */
	attempt_store(pos, alpha, depth, m ? 0 : 2, m);
	return alpha;
}

int16_t evaluate(struct position *pos, uint8_t depth, move *m, int verbose, int max_duration, struct history *history) {
	int16_t evaluation = 0, saved_evaluation;
	move move_list[256];
	generate_all(pos, move_list);

	if (m)
		*m = 0;

	if (!move_list[0]) {
		uint64_t checkers = generate_checkers(pos);
		if (!checkers)
			evaluation = 0;
		else 
			evaluation = pos->turn ? -0x7F00 : 0x7F00;
		if (verbose) {
			if (evaluation)
				printf("%i %cm\n", depth,
						pos->turn ? '-' : '+');
			else
				printf("%i s\n", depth);
		}
		return evaluation;
	}

	if (depth == 0) {
		evaluation = count_position(pos);
		if (!pos->turn)
			evaluation = -evaluation;
		if (verbose)
			printf("0 %+.2f\n", (double)evaluation / 100);
		return evaluation;
	}

	clock_t clock_stop;
	if (max_duration >= 0)
		clock_stop = clock() + CLOCKS_PER_SEC * max_duration;
	else
		clock_stop = 0;

	move pv_moves[256][256];
	move killer_moves[256][2];
	uint64_t history_moves[13][64];
	memset(pv_moves, 0, sizeof(pv_moves));
	memset(killer_moves, 0, sizeof(killer_moves));
	memset(history_moves, 0, sizeof(history_moves));

	saved_evaluation = 0;
	int i;
	int16_t alpha, beta;
	for (int d = 1; d <= depth; d++) {
		nodes = 0;
		pv_flag = 1;
		alpha = -0x7F00;
		beta = 0x7F00;

		evaluate_moves(pos, move_list, 0, NULL, pv_moves, killer_moves, history_moves);

		for (i = 0; move_list[i]; i++) {
			do_move(pos, move_list + i);
			evaluation = -evaluate_recursive(pos, d - 1, 1, -beta, -alpha, 0, clock_stop, pv_moves, killer_moves, history_moves);
			evaluation -= (evaluation > 0x4000);
			if (is_threefold(pos, history))
				evaluation = 0;
			undo_move(pos, move_list + i);
			if (evaluation > alpha) {
				store_pv_move(move_list + i, 0, pv_moves);
				alpha = evaluation;
			}
		}
		if (interrupt)
			break;
		evaluation = pos->turn ? alpha : -alpha;
		saved_evaluation = evaluation;
		if (verbose) {
			if (evaluation < -0x4000)
				printf("%i -m%i", d, 0x7F00 + evaluation);
			else if (evaluation > 0x4000)
				printf("%i +m%i", d, 0x7F00 - evaluation);
			else
				printf("%i %+.2f", d, (double)evaluation / 100);
			printf(" nodes %" PRIu64 " pv ", nodes);
			print_pv(pos, pv_moves);
			printf("\n");
			fflush(stdout);
		}
		if (m)
			*m = pv_moves[0][0];
		/* stop searching if mate is found */
		if (evaluation < -0x4000 && 2 * (0x7F00 + evaluation) + pos->turn - 1 <= d)
			break;
		if (evaluation > 0x4000 && 2 * (0x7F00 - evaluation) - pos->turn <= d)
			break;
	}
	return saved_evaluation;
}
