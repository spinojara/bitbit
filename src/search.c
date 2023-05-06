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
#include <string.h>
#include <math.h>

#include "bitboard.h"
#include "move_gen.h"
#include "move.h"
#include "util.h"
#if defined(TRANSPOSITION)
#include "transposition_table.h"
#endif
#if defined(NNUE)
#include "nnue.h"
#endif
#include "init.h"
#include "position.h"
#include "attack_gen.h"
#include "time_man.h"
#include "interrupt.h"
#include "evaluate.h"
#include "history.h"
#include "move_order.h"
#include "material.h"

static int reductions[256] = { 0 };

/* We choose r(x,y)=Clog(x)log(y)+D because it is increasing and concave.
 * It is also relatively simple. If x is constant, y > y' we have and by
 * decreasing the depth we need to have y-1-r(x,y)>y'-1-r(x,y').
 * Simplyfing and using the mean value theorem gives us 1>Clog(x)/y''
 * where 2<=y'<y''<y. The constant C must satisfy C<y''/log(x) for all such
 * x and y''. x can be at most 255 and y'' can only attain some discrete
 * values as a function of y, y'. The min of y'' occurs for y=3, y'=2 where
 * we get y''=2.46. Thus C<2.46/log(255)=0.44. Scaling C and instead using
 * its square root we get the formula
 * sqrt(C)=sqrt(1024/((log(3)-log(2))log(255))). D is experimental for now.
 */
static inline int late_move_reduction(int index, int depth) {
	int r = reductions[index] * reductions[depth];
	return (r >> 10) + 1;
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

static inline void store_killer_move(move *m, uint8_t ply, move killer_moves[][2]) {
	if (interrupt)
		return;
	if ((*m & 0xFFFF) == killer_moves[ply][0])
		return;
	killer_moves[ply][1] = killer_moves[ply][0];
	killer_moves[ply][0] = *m & 0xFFFF;
}

static inline void store_history_move(struct position *pos, move *m, uint8_t depth, uint64_t history_moves[13][64]) {
	if (interrupt)
		return;
	history_moves[pos->mailbox[move_from(m)]][move_to(m)] += (uint64_t)1 << depth;
}

static inline void store_pv_move(move *m, uint8_t ply, move pv_moves[256][256]) {
	if (interrupt)
		return;
	pv_moves[ply][ply] = *m & 0xFFFF;
	memcpy(pv_moves[ply] + ply + 1, pv_moves[ply + 1] + ply + 1, sizeof(move) * (255 - ply));
}

static inline int16_t evaluate(struct position *pos) {
	int16_t evaluation;
#if defined(NNUE)
	evaluation = evaluate_accumulator(pos);
#else
	evaluation = evaluate_classical(pos);
#endif
	/* damp when shuffling pieces */
	evaluation = (int32_t)evaluation * (200 - pos->halfmove) / 200;

	return evaluation;
}

int16_t quiescence(struct position *pos, int16_t alpha, int16_t beta, struct searchinfo *si) {
	if (interrupt)
		return 0;
	if (si->nodes % 4096 == 0)
		if (si->clock_stop && clock() > si->clock_stop)
			interrupt = 1;


	uint64_t checkers = generate_checkers(pos, pos->turn);
	int16_t evaluation = evaluate(pos);

	si->nodes++;
	if (!checkers) {
		if (evaluation >= beta) {
			return beta;
		}
		if (evaluation > alpha)
			alpha = evaluation;
	}

	move move_list[MOVES_MAX];
	generate_quiescence(pos, move_list);

	if (!move_list[0])
		return evaluation;

	uint64_t evaluation_list[MOVES_MAX];
	move *ptr = order_moves(pos, move_list, evaluation_list, 0, 0, NULL, si);
	for (; *ptr; next_move(move_list, evaluation_list, &ptr)) {
		if (is_capture(pos, ptr) && !checkers && evaluation_list[ptr - move_list] < SEE_VALUE_MINUS_100 + 10000)
			continue;
		do_move(pos, ptr);
#if defined(NNUE)
		do_accumulator(pos, ptr);
#endif
		evaluation = -quiescence(pos, -beta, -alpha, si);
		undo_move(pos, ptr);
#if defined(NNUE)
		undo_accumulator(pos, ptr);
#endif
		if (evaluation >= beta)
			return beta;
		if (evaluation > alpha)
			alpha = evaluation;
	}
	return alpha;
}

int16_t negamax(struct position *pos, uint8_t depth, uint8_t ply, int16_t alpha, int16_t beta, int node_type, int flag, struct searchinfo *si) {
	/* value is not used but suppresses compiler warning */
	int16_t evaluation = 0;

	int root_node = (node_type == NODE_ROOT);
	int pv_node = root_node || (node_type == NODE_PV);

	if (interrupt)
		return 0;
	if (si->nodes % 4096 == 0)
		if (si->clock_stop && clock() > si->clock_stop)
			interrupt = 1;

	/* draw by repetition or 50 move rule */
	if (!root_node && (((pv_node || ply <= 2) && si->history && is_threefold(pos, si->history)) || pos->halfmove >= 100))
		return 0;

#if defined(TRANSPOSITION)
	void *e = attempt_get(pos);
	if (e && transposition_open(e) && !pv_node)
		return 0;
	if (e && transposition_depth(e) >= depth && !pv_node) {
		evaluation = adjust_value_mate_get(transposition_evaluation(e), ply);
		if (transposition_type(e) == NODE_PV)
			return evaluation;
		else if (transposition_type(e) == NODE_CUT && evaluation >= beta)
			return beta;
		else if (transposition_type(e) == NODE_ALL && evaluation <= alpha)
			return alpha;
	}
#else
	void *e = NULL;
#endif
	
	uint64_t checkers = generate_checkers(pos, pos->turn);
	if (depth <= 0 && (!checkers || ply >= 2 * si->root_depth)) {
		evaluation = mate(pos);
		/* stalemate */
		if (evaluation == 1)
			return 0;
		/* checkmate */
		if (evaluation == 2)
			return ply - VALUE_MATE;
		return quiescence(pos, alpha, beta, si);
	}

#if 0
	int16_t static_evaluation = evaluate(pos);
	if (!pv_node && !checkers && depth <= 6 && static_evaluation + (160 + 90 * depth * depth) < alpha) {
		evaluation = quiescence(pos, alpha - 1, alpha, si);
		if (evaluation < alpha)
			return evaluation;
	}

	if (!pv_node && !checkers && depth == 1 && static_evaluation - 200 > beta)
		return static_evaluation;
#endif

	/* null move pruning */
	if (!pv_node && !checkers && flag != FLAG_NULL_MOVE && depth >= 3 && has_big_piece(pos)) {
		int t = pos->en_passant;
		do_null_move(pos, 0);
#if defined(TRANSPOSITION)
		do_null_zobrist_key(pos, 0);
#endif
		evaluation = -negamax(pos, depth - 3, ply + 1, -beta, -beta + 1, NODE_OTHER, FLAG_NULL_MOVE, si);
		do_null_move(pos, t);
#if defined(TRANSPOSITION)
		do_null_zobrist_key(pos, t);
#endif
		if (evaluation >= beta)
			return beta;
	}

	move move_list[MOVES_MAX];
	generate_all(pos, move_list);

	if (!move_list[0])
		return checkers ? ply - VALUE_MATE : 0;

	uint64_t evaluation_list[MOVES_MAX];
	
#if defined(TRANSPOSITION)
	if (e)
		transposition_set_open(e);
#endif

	uint16_t m = 0;
#if !defined(TRANSPOSITION)
	UNUSED(m);
#endif
	move *ptr = order_moves(pos, move_list, evaluation_list, depth, ply, e, si);
	for (; *ptr; next_move(move_list, evaluation_list, &ptr)) {
		int move_number = ptr - move_list;
#if defined(TRANSPOSITION)
		do_zobrist_key(pos, ptr);
#endif
		do_move(pos, ptr);
#if defined(NNUE)
		do_accumulator(pos, ptr);
#endif

		uint8_t new_depth = depth - 1;
		/* extensions */
		uint8_t extensions = 0;
		if (ply < 2 * si->root_depth) {
			extensions = (checkers || !move_list[1]);
		}
		new_depth += extensions;

		int new_flag = is_capture(pos, ptr) ? move_to(ptr) : FLAG_NONE;
		/* late move reductions */
		int full_depth_search = 0;
		if (depth >= 2 && !checkers && move_number >= 1 && move_flag(ptr) != 2 && !move_capture(ptr)) {
			uint8_t r = late_move_reduction(move_number, depth);
			evaluation = -negamax(pos, MAX(new_depth - r, 0), ply + 1, -alpha - 1, -alpha, NODE_OTHER, new_flag, si);

			if (evaluation > alpha)
				full_depth_search = 1;
		}
		else {
			full_depth_search = !pv_node || move_number;
		}

		if (full_depth_search)
			evaluation = -negamax(pos, new_depth, ply + 1, -alpha - 1, -alpha, NODE_OTHER, new_flag, si);

		if (pv_node && (!move_number || (evaluation > alpha && (root_node || evaluation < beta))))
			evaluation = -negamax(pos, new_depth, ply + 1, -beta, -alpha, NODE_PV, new_flag, si);

#if defined(TRANSPOSITION)
		undo_zobrist_key(pos, ptr);
#endif
		undo_move(pos, ptr);
#if defined(NNUE)
		undo_accumulator(pos, ptr);
#endif
		if (evaluation >= beta) {
			if (pv_node && !is_capture(pos, ptr) && move_flag(ptr) != 2)
				store_killer_move(ptr, ply, si->killer_moves);
#if defined(TRANSPOSITION)
			if (e)
				transposition_set_closed(e);
			/* type cut */
			attempt_store(pos, beta, depth, NODE_CUT, *ptr);
#endif
			return beta;
		}
		if (evaluation > alpha) {
			if (!is_capture(pos, ptr) && move_flag(ptr) != 2)
				store_history_move(pos, ptr, depth, si->history_moves);
			alpha = evaluation;
			if (pv_node)
				store_pv_move(ptr, ply, si->pv_moves);
#if defined(TRANSPOSITION)
			m = *ptr;
#endif
		}
	}
#if defined(TRANSPOSITION)
	if (e)
		transposition_set_closed(e);
	/* type pv or all */
	attempt_store(pos, adjust_value_mate_store(alpha, ply), depth, m ? NODE_PV : NODE_ALL , m);
#endif
	return alpha;
}

int16_t search(struct position *pos, uint8_t depth, int verbose, int etime, int movetime, move *m, struct history *history, int iterative) {
	clock_t clock_start = clock();
	if (etime && !movetime)
		movetime = etime / 5;

	struct searchinfo si = { 0 };
	si.history = history;
	si.clock_stop = movetime ? clock() + (CLOCKS_PER_SEC * movetime) / 1000 : 0;

	char str[8];
	int16_t evaluation = 0, saved_evaluation[256] = { 0 };

#if defined(NNUE)
	update_accumulator(pos, pos->accumulation, 0);
	update_accumulator(pos, pos->accumulation, 1);
#endif

	if (depth == 0) {
		evaluation = evaluate(pos);
		if (verbose)
			printf("info depth 0 score cp %d nodes 1\n", evaluation);
		return evaluation;
	}

	int16_t alpha, beta;
	move bestmove = 0;
	uint8_t d;
	int delta = 30;
	int last = 0;
	int aspiration_window = 0;
	for (d = iterative ? 1 : depth; d <= depth; d++) {
		si.root_depth = d;
		if (!aspiration_window)
			si.nodes = 0;
		alpha = -VALUE_MATE;
		beta = VALUE_MATE;

		if (aspiration_window) {
			alpha = last - delta;
			beta = last + delta;
		}

		evaluation = negamax(pos, d, 0, alpha, beta, NODE_ROOT, 0, &si);

		if (interrupt) {
			d--;
			break;
		}
		saved_evaluation[d] = evaluation;
		if (aspiration_window) {
			if (evaluation <= last - delta || evaluation >= last + delta) {
				if (delta >= 75)
					aspiration_window = 0;
				else
					delta += 30;
				last = evaluation;
				d--;
				continue;
			}
		}
		else if (d >= 6)
			aspiration_window = 1;

		bestmove = si.pv_moves[0][0];

		clock_t clocks = clock() - clock_start;
		if (verbose) {
			printf("info depth %d score ", d);
			if (evaluation >= VALUE_MATE_IN_MAX_PLY)
				printf("mate %d", (VALUE_MATE - evaluation + 1) / 2);
			else if (evaluation <= -VALUE_MATE_IN_MAX_PLY)
				printf("mate %d", (-VALUE_MATE - evaluation) / 2);
			else
				printf("cp %d", evaluation);
			printf(" nodes %ld time %ld ", si.nodes, 1000 * clocks / CLOCKS_PER_SEC);
			printf("nps %ld ", clocks ? si.nodes * CLOCKS_PER_SEC / clocks : 0);
			printf("pv");
			print_pv(pos, si.pv_moves[0], 0);
			printf("\n");
		}
		if (etime && 3 * 1000 * (clock() - clock_start) > 2 * time_man(etime, saved_evaluation, d) * CLOCKS_PER_SEC)
			break;
	}

	if (verbose && interrupt != 2)
		printf("bestmove %s\n", move_str_algebraic(str, &bestmove));
	if (m && interrupt != 2)
		*m = bestmove;
	return saved_evaluation[d - 1];
}

void search_init(void) {
	for (int i = 1; i < 256; i++) {
		/* sqrt(C) * log(i) */
		reductions[i] = (int)(21.34 * log(i));
	}
}
