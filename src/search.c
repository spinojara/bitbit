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
#include "movegen.h"
#include "move.h"
#include "util.h"
#include "transposition.h"
#include "nnue.h"
#include "init.h"
#include "position.h"
#include "attackgen.h"
#include "timeman.h"
#include "interrupt.h"
#include "evaluate.h"
#include "history.h"
#include "moveorder.h"
#include "material.h"
#include "option.h"

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

/* random drawn score to search more dynamically */
static inline int16_t draw(struct searchinfo *si) {
	return (si->nodes & 0x3);
}

static inline int16_t evaluate(struct position *pos) {
	int16_t evaluation;
	if (option_nnue)
		evaluation = evaluate_accumulator(pos);
	else
		evaluation = evaluate_classical(pos);
	/* damp when shuffling pieces */
	evaluation = (int32_t)evaluation * (200 - pos->halfmove) / 200;

	return evaluation;
}

int16_t quiescence(struct position *pos, uint16_t ply, int16_t alpha, int16_t beta, struct searchinfo *si) {
	uint64_t checkers = generate_checkers(pos, pos->turn);
	int16_t best_eval = -VALUE_INFINITE;
	int16_t eval = evaluate(pos);

	move move_list[MOVES_MAX];
	generate_quiescence(pos, move_list);

	if (!move_list[0])
		return checkers ? -VALUE_MATE + ply : eval;

	if (!checkers) {
		if (eval >= beta) {
			return beta;
		}
		if (eval > alpha)
			alpha = eval;
	}

	uint64_t evaluation_list[MOVES_MAX];
	move *ptr = order_moves(pos, move_list, evaluation_list, 0, 0, NULL, si);
	for (; *ptr; next_move(move_list, evaluation_list, &ptr)) {
		if (is_capture(pos, ptr) && !checkers && evaluation_list[ptr - move_list] < SEE_VALUE_MINUS_100 + 10000)
			continue;
		do_move(pos, ptr);
		si->nodes++;
		do_accumulator(pos, ptr);
		eval = -quiescence(pos, ply + 1, -beta, -alpha, si);
		undo_move(pos, ptr);
		undo_accumulator(pos, ptr);
		if (eval > best_eval) {
			best_eval = eval;
			if (eval > alpha) {
				alpha = eval;
				if (eval >= beta)
					break;
			}
		}
	}
	return best_eval;
}

/* check for ply and depth out of bound */
int16_t negamax(struct position *pos, uint8_t depth, uint16_t ply, int16_t alpha, int16_t beta, int cut_node, int flag, struct searchinfo *si) {
	int16_t best_eval = -VALUE_INFINITE;
	/* value is not used but suppresses compiler warning */
	int16_t eval = -VALUE_INFINITE;

	const int root_node = (ply == 0);
	const int pv_node = (beta != alpha + 1);

	if (interrupt || si->interrupt)
		return 0;
	if (si->nodes % 4096 == 0)
		check_time(si);

	if (!root_node) {
		/* draws */
		if (pos->halfmove >= 100 || is_repetition(pos, si->history, ply, 1 + pv_node))
			return draw(si);

		/* mate distance pruning */
		alpha = MAX(alpha, -VALUE_MATE + ply);
		beta = MIN(beta, VALUE_MATE - ply - 1);
		if (alpha >= beta)
			return alpha;
	}

	struct transposition *e = attempt_get(pos);
	if (e && e->depth >= depth && !pv_node) {
		eval = adjust_value_mate_get(e->evaluation, ply);
		if (e->bound == BOUND_EXACT)
			return eval;
		else if (e->bound == BOUND_LOWER && eval >= beta)
			return beta;
		else if (e->bound == BOUND_UPPER && eval <= alpha)
			return alpha;
	}
	
	uint64_t checkers = generate_checkers(pos, pos->turn);
	if ((depth == 0 && !checkers) || ply >= 2 * si->root_depth)
		return quiescence(pos, ply + 1, alpha, beta, si);

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
		si->history->zobrist_key[si->history->ply + ply] = pos->zobrist_key;
		do_null_zobrist_key(pos, 0);
		eval = -negamax(pos, depth - 3, ply + 1, -beta, -beta + 1, !cut_node, FLAG_NULL_MOVE, si);
		do_null_move(pos, t);
		do_null_zobrist_key(pos, t);
		if (eval >= beta)
			return beta;
	}

	move move_list[MOVES_MAX];
	generate_all(pos, move_list);

	if (!move_list[0])
		return checkers ? -VALUE_MATE + ply : 0;

	uint64_t evaluation_list[MOVES_MAX];
	
	uint16_t m = 0;

	move *ptr = order_moves(pos, move_list, evaluation_list, depth, ply, e, si);
	for (; *ptr; next_move(move_list, evaluation_list, &ptr)) {
		int move_number = ptr - move_list;
		si->history->zobrist_key[si->history->ply + ply] = pos->zobrist_key;
		do_zobrist_key(pos, ptr);
		do_move(pos, ptr);
		si->nodes++;
		do_accumulator(pos, ptr);

		uint8_t new_depth = depth - 1;
		/* extensions */
		uint8_t extensions = 0;
		if (ply < 2 * si->root_depth) {
			extensions = (checkers || !move_list[1]);
		}
		new_depth += extensions;
		int new_flag = move_capture(ptr) ? move_to(ptr) : FLAG_NONE;
		/* late move reductions */
		int full_depth_search = 0;
		if (depth >= 2 && !checkers && move_number >= (1 + pv_node) && (!move_capture(ptr) || cut_node)) {
			uint8_t r = late_move_reduction(move_number, depth);
			r -= pv_node;
			r += !move_capture(ptr);
			r += cut_node;
			uint8_t lmr_depth = CLAMP(new_depth - r, 1, new_depth);

			eval = -negamax(pos, lmr_depth, ply + 1, -alpha - 1, -alpha, 1, new_flag, si);

			if (eval > alpha)
				full_depth_search = 1;
		}
		else {
			full_depth_search = !pv_node || move_number;
		}

		if (full_depth_search)
			eval = -negamax(pos, new_depth, ply + 1, -alpha - 1, -alpha, !cut_node, new_flag, si);

		if (pv_node && (!move_number || (eval > alpha && (root_node || eval < beta))))
			eval = -negamax(pos, new_depth, ply + 1, -beta, -alpha, 0, new_flag, si);

		undo_zobrist_key(pos, ptr);
		undo_move(pos, ptr);
		undo_accumulator(pos, ptr);
		if (interrupt || si->interrupt)
			return 0;

		if (eval > best_eval) {
			best_eval = eval;

			if (eval > alpha) {
				m = *ptr;

				if (pv_node)
					store_pv_move(ptr, ply, si->pv_moves);
				if (!is_capture(pos, ptr) && move_flag(ptr) != 2)
					store_history_move(pos, ptr, depth, si->history_moves);

				if (pv_node && eval < beta) {
					alpha = eval;
				}
				else {
					if (!is_capture(pos, ptr) && move_flag(ptr) != 2)
						store_killer_move(ptr, ply, si->killer_moves);
					break;
				}
			}
		}
	}
	int bound = (best_eval >= beta) ? BOUND_LOWER : (pv_node && m) ? BOUND_EXACT : BOUND_UPPER;
	attempt_store(pos, adjust_value_mate_store(best_eval, ply), depth, bound, m);
	return best_eval;
}

int16_t aspiration_window(struct position *pos, uint8_t depth, int16_t last, struct searchinfo *si) {
	int16_t evaluation;
	int16_t delta = 25 + last * last / 16384;
	int16_t alpha = MAX(last - delta, -VALUE_MATE);
	int16_t beta = MIN(last + delta, VALUE_MATE);

	while (!si->interrupt || interrupt) {
		evaluation = negamax(pos, depth, 0, alpha, beta, 0, 0, si);

		if (evaluation <= alpha) {
			alpha = MAX(alpha - delta, -VALUE_MATE);
			beta = (alpha + 3 * beta) / 4;
		}
		else if (evaluation >= beta) {
			beta = MIN(beta + delta, VALUE_MATE);
		}
		else {
			return evaluation;
		}

		delta += delta / 3;
	}
	return 0;
}

int16_t search(struct position *pos, uint8_t depth, int verbose, int etime, int movetime, move *m, struct history *history, int iterative) {
	time_point ts = time_now();
	if (etime && !movetime)
		movetime = etime / 5;

	struct searchinfo si = { 0 };
	si.time_start = ts;
	si.time_stop = movetime ? si.time_start + 1000 * movetime : 0;
	si.history = history;

	time_init(pos, etime, &si);

	char str[8];
	int16_t evaluation = 0;

	update_accumulator(pos, pos->accumulation, 0);
	update_accumulator(pos, pos->accumulation, 1);

	if (depth == 0) {
		evaluation = evaluate(pos);
		if (verbose)
			printf("info depth 0 score cp %d nodes 1\n", evaluation);
		return evaluation;
	}

	move bestmove = 0;
	uint16_t d;
	for (d = iterative ? 1 : depth; d <= depth; d++) {
		si.root_depth = d;
		if (verbose)
			reset_seldepth(si.history);

		if (d <= 6 || !iterative)
			evaluation = negamax(pos, d, 0, -VALUE_MATE, VALUE_MATE, 0, 0, &si);
		else
			evaluation = aspiration_window(pos, d, evaluation, &si);

		if (interrupt || si.interrupt) {
			d--;
			break;
		}
		si.evaluation_list[d] = evaluation;

		bestmove = si.pv_moves[0][0];

		time_point tp = time_now() - ts;
		if (verbose) {
			printf("info depth %d seldepth %d score ", d, seldepth(si.history));
			if (evaluation >= VALUE_MATE_IN_MAX_PLY)
				printf("mate %d", (VALUE_MATE - evaluation + 1) / 2);
			else if (evaluation <= -VALUE_MATE_IN_MAX_PLY)
				printf("mate %d", (-VALUE_MATE - evaluation) / 2);
			else
				printf("cp %d", evaluation);
			printf(" nodes %ld time %ld ", si.nodes, tp / 1000);
			printf("nps %ld ", tp ? 1000000 * si.nodes / tp : 0);
			printf("pv");
			print_pv(pos, si.pv_moves[0], 0);
			printf("\n");
		}
		if (etime && stop_searching(&si))
			break;
	}

	if (verbose && !interrupt)
		printf("bestmove %s\n", move_str_algebraic(str, &bestmove));
	if (m && !interrupt)
		*m = bestmove;
	return si.evaluation_list[d - 1];
}

void search_init(void) {
	for (int i = 1; i < 256; i++) {
		/* sqrt(C) * log(i) */
		reductions[i] = (int)(21.34 * log(i));
	}
}
