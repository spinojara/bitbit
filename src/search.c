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

#include "search.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include "bitboard.h"
#include "move_gen.h"
#include "move.h"
#include "util.h"
#include "transposition_table.h"
#include "init.h"
#include "position.h"
#include "attack_gen.h"
#include "time_man.h"
#include "interrupt.h"
#include "evaluate.h"

uint64_t nodes = 0;

uint64_t mvv_lva_lookup[13 * 13];

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

int late_move_reduction(int index, int depth) {
	int r = index < 8 ? depth - 2 : depth / 2;
	return r;
}

void print_pv(struct position *pos, move *pv_move, int ply) {
	char str[8];
	if (ply > 255 || !is_legal(pos, pv_move))
		return;
	printf(" %s", move_str_algebraic(str, pv_move));
	do_move(pos, pv_move);
	print_pv(pos, pv_move + 1, ply + 1);
	undo_move(pos, pv_move);
	*pv_move = *pv_move & 0xFFFF;
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
	memcpy(pv_moves[ply] + ply + 1, pv_moves[ply + 1] + ply + 1, sizeof(move) * (255 - ply));
}

int contains_pv_move(move *move_list, uint8_t ply, move pv_moves[256][256]) {
	if (!pv_moves)
		return 0;
	for (move *ptr = move_list; *ptr; ptr++)
		if ((*ptr & 0xFFFF) == (pv_moves[0][ply] & 0xFFFF))
			return 1;
	return 0;
}

uint64_t evaluate_move(struct position *pos, move *m, uint8_t ply, struct transposition *e, int pv_flag, move pv_moves[256][256], move killer_moves[][2], uint64_t history_moves[13][64]) {
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
void evaluate_moves(struct position *pos, move *move_list, uint8_t depth, struct transposition *e, int pv_flag, move pv_moves[256][256], move killer_moves[][2], uint64_t history_moves[13][64]) {
	uint64_t evaluation_list[MOVES_MAX];
	int i;
	for (i = 0; move_list[i]; i++)
		evaluation_list[i] = evaluate_move(pos, move_list + i, depth, e, pv_flag, pv_moves, killer_moves, history_moves);

	merge_sort(move_list, evaluation_list, 0, i - 1, 0);
}

int16_t quiescence(struct position *pos, int16_t alpha, int16_t beta, clock_t clock_stop) {
	if (interrupt)
		return 0;
	if (nodes % 4096 == 0)
		if (clock_stop && clock() > clock_stop)
			interrupt = 1;

	int16_t evaluation;
	evaluation = evaluate_static(pos, &nodes);
	if (evaluation >= beta)
		return beta;
	if (evaluation > alpha)
		alpha = evaluation;

	move move_list[MOVES_MAX];
	generate_quiescence(pos, move_list);
	if (!move_list[0])
		return evaluation;

	evaluate_moves(pos, move_list, 0, NULL, 0, NULL, NULL, NULL);
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

int16_t evaluate_recursive(struct position *pos, uint8_t depth, uint8_t ply, int16_t alpha, int16_t beta, int null_move, clock_t clock_stop, int *pv_flag, move pv_moves[][256], move killer_moves[][2], uint64_t history_moves[13][64]) {
	int16_t evaluation;

	if (interrupt)
		return 0;
	if (nodes % 4096 == 0)
		if (clock_stop && clock() > clock_stop)
			interrupt = 1;

	struct transposition *e = attempt_get(pos);
	if (e && transposition_open(e))
		return 0;
	if (e && transposition_depth(e) >= depth && !(*pv_flag)) {
		/* pv */
		evaluation = transposition_evaluation(e);
		if (transposition_type(e) == 0)
			return evaluation;
		/* cut */
		else if (transposition_type(e) == 1 && evaluation >= beta)
			return beta;
		/* all */
		else if (transposition_type(e) == 2 && evaluation <= alpha)
			return alpha;
	}

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
	uint64_t checkers = generate_checkers(pos, pos->turn);
	if (!null_move && !(*pv_flag) && !checkers && depth >= 3 && has_big_piece(pos)) {
		int t = pos->en_passant;
		do_null_move(pos, 0);
		evaluation = -evaluate_recursive(pos, depth - 3, ply + 1, -beta, -beta + 1, 1, clock_stop, pv_flag, NULL, NULL, history_moves);
		do_null_move(pos, t);
		if (evaluation >= beta)
			return beta;
	}

	move move_list[MOVES_MAX];
	generate_all(pos, move_list);

	if (!move_list[0])
		return checkers ? -0x7F00 : 0;

	if (*pv_flag && !contains_pv_move(move_list, ply, pv_moves))
		*pv_flag = 0;

	evaluate_moves(pos, move_list, depth, e, *pv_flag, pv_moves, killer_moves, history_moves);

	if (pos->halfmove >= 100)
		return 0;
	
	if (e)
		transposition_set_open(e);

	uint16_t m = 0;
	for (move *ptr = move_list; *ptr; ptr++) {
		do_move(pos, ptr);
		/* late move reduction */
		if (!(*pv_flag) && depth >= 3 && !checkers && ptr - move_list >= 2 && move_flag(ptr) != 2 && !move_capture(ptr)) {
			uint8_t r = late_move_reduction(ptr - move_list, depth);
			evaluation = -evaluate_recursive(pos, r, ply + 1, -alpha - 1, -alpha, 0, clock_stop, pv_flag, pv_moves, killer_moves, history_moves);
		}
		else {
			evaluation = alpha + 1;
		}
		if (evaluation > alpha) {
			/* -beta - 1 to search for mate in <n> */
			evaluation = -evaluate_recursive(pos, depth - 1, ply + 1, -beta - 1, -alpha, 0, clock_stop, pv_flag, pv_moves, killer_moves, history_moves);
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
			m = *ptr;
		}
	}
	if (e)
		transposition_set_closed(e);
	/* type pv or all */
	attempt_store(pos, alpha, depth, m ? 0 : 2, m & 0xFFFF);
	return alpha;
}

int16_t evaluate(struct position *pos, uint8_t depth, int verbose, int etime, int movetime, struct history *history) {
	char str[8];
	int16_t evaluation = 0, saved_evaluation[256] = { 0 };
	move move_list[MOVES_MAX];
	generate_all(pos, move_list);

	if (is_threefold(pos, history)) {
		if (verbose)
			printf("info depth 0 string stalemate\n");
		return 0;
	}

	if (!move_list[0]) {
		uint64_t checkers = generate_checkers(pos, pos->turn);
		if (!checkers)
			evaluation = 0;
		else 
			evaluation = -0x7F00;
		if (verbose) {
			if (evaluation)
				printf("info depth 0 score mate -0\n");
			else
				printf("info depth 0 string stalemate\n");
		}
		return evaluation;
	}

	if (depth == 0) {
		evaluation = evaluate_static(pos, &nodes);
		if (verbose)
			printf("info depth 0 score cp %d nodes 0 time 0\n", evaluation);
		return evaluation;
	}

	clock_t clock_start = clock();
	if (etime && !movetime)
		movetime = etime / 5;
	clock_t clock_stop = movetime ? clock() + (CLOCKS_PER_SEC * movetime) / 1000 : 0;

	move pv_moves[256][256];
	move killer_moves[256][2];
	uint64_t history_moves[13][64];
	memset(pv_moves, 0, sizeof(pv_moves));
	memset(killer_moves, 0, sizeof(killer_moves));
	memset(history_moves, 0, sizeof(history_moves));

	int16_t alpha, beta;
	int pv_flag;
	move bestmove = 0;
	uint8_t d = 0;
	for (d = 1; d <= depth; d++) {
		nodes = 0;
		pv_flag = 1;
		alpha = -0x7F00;
		beta = 0x7F00;

		evaluate_moves(pos, move_list, 0, NULL, pv_flag, pv_moves, killer_moves, history_moves);

		move *ptr;
		for (ptr = move_list; *ptr; ptr++) {
			do_move(pos, ptr);
			evaluation = -evaluate_recursive(pos, d - 1, 1, -beta, -alpha, 0, clock_stop, &pv_flag, pv_moves, killer_moves, history_moves);
			evaluation -= (evaluation > 0x4000);
			undo_move(pos, ptr);
			if (evaluation > alpha) {
				store_pv_move(ptr, 0, pv_moves);
				alpha = evaluation;
			}
		}
		if (interrupt) {
			d--;
			break;
		}
		evaluation = alpha;
		saved_evaluation[d] = evaluation;
		bestmove = pv_moves[0][0];
		if (verbose) {
			printf("info depth %d score ", d);
			if (evaluation < -0x4000)
				printf("mate %d", -0x7F00 - evaluation);
			else if (evaluation > 0x4000)
				printf("mate %d", 0x7F00 - evaluation);
			else
				printf("cp %d", evaluation);
			printf(" nodes %ld time %ld pv", nodes, 1000 * (clock() - clock_start) / CLOCKS_PER_SEC);
			print_pv(pos, pv_moves[0], 0);
			printf("\n");
		}
		if (etime && 3 * 1000 * (clock() - clock_start) > 2 * time_man(etime, saved_evaluation, d) * CLOCKS_PER_SEC)
			break;

		/* stop searching if mate is found */
		if (evaluation < -0x4000 && 2 * (0x7F00 + evaluation) <= d)
			break;
		if (evaluation > 0x4000 && 2 * (0x7F00 - evaluation) - 1 <= d)
			break;
	}
	if (verbose && interrupt != 2)
		printf("bestmove %s\n", move_str_algebraic(str, &bestmove));
	return saved_evaluation[d];
}

void search_init(void) {
	for (int i = 0; i < 13; i++) {
		for (int j = 0; j < 13; j++) {
			mvv_lva_lookup[j + 13 * i] = mvv_lva_calc(i, j);
			init_status("populating mvv lva lookup table");
		}
	}
}
