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
#include <assert.h>

#include "bitboard.h"
#include "movegen.h"
#include "move.h"
#include "util.h"
#include "transposition.h"
#include "nnue.h"
#include "position.h"
#include "attackgen.h"
#include "timeman.h"
#include "interrupt.h"
#include "evaluate.h"
#include "history.h"
#include "movepicker.h"
#include "option.h"
#include "endgame.h"

static int reductions[DEPTH_MAX] = { 0 };

/* We choose r(x,y)=Clog(x)log(y)+D because it is increasing and concave.
 * It is also relatively simple. If x is constant, y > y' and by
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
	assert(0 <= depth && depth < DEPTH_MAX);
	int r = reductions[index] * reductions[depth];
	return (r >> 10) + 1;
}

void print_pv(struct position *pos, move_t *pv_move, int ply) {
	char str[8];
	if (!(ply < DEPTH_MAX) || !is_legal(pos, pv_move))
		return;
	printf(" %s", move_str_algebraic(str, pv_move));
	do_move(pos, pv_move);
	print_pv(pos, pv_move + 1, ply + 1);
	undo_move(pos, pv_move);
	*pv_move = *pv_move & 0xFFFF;
}

static inline void store_killer_move(const move_t *m, int ply, move_t killers[][2]) {
	if (interrupt)
		return;
	assert(0 <= ply && ply < DEPTH_MAX);
	assert(*m);
	if ((*m & 0xFFFF) == killers[ply][0])
		return;
	killers[ply][1] = killers[ply][0];
	killers[ply][0] = *m & 0xFFFF;
}

static inline void store_history_move(const struct position *pos, const move_t *m, int depth, int64_t history_moves[13][64]) {
	if (interrupt)
		return;
	assert(*m);
	assert(0 <= depth && depth < DEPTH_MAX);
	history_moves[pos->mailbox[move_from(m)]][move_to(m)] += (uint64_t)1 << MIN(depth, 32);
}

static inline void store_pv_move(const move_t *m, int ply, move_t pv[DEPTH_MAX][DEPTH_MAX]) {
	if (interrupt)
		return;
	assert(*m);
	pv[ply][ply] = *m & 0xFFFF;
	memcpy(pv[ply] + ply + 1, pv[ply + 1] + ply + 1, sizeof(move_t) * (DEPTH_MAX - (ply + 1)));
}

/* Random drawn score to avoid threefold blindness. */
static inline int32_t draw(const struct searchinfo *si) {
	return (si->nodes & 0x3);
}

static inline int32_t evaluate(const struct position *pos) {
	int32_t evaluation;
	struct endgame *e = option_endgame ? endgame_probe(pos) : NULL;
	if (e && (evaluation = endgame_evaluate(e, pos)) != VALUE_NONE)
		return evaluation;

	evaluation = option_nnue ? evaluate_accumulator(pos)
		                         : evaluate_classical(pos);
	/* Damp when shuffling pieces. */
	if (option_damp)
		evaluation = evaluation * (200 - pos->halfmove) / 200;
	return evaluation;
}

int32_t quiescence(struct position *pos, int ply, int32_t alpha, int32_t beta, struct searchinfo *si) {
	const int pv_node = (beta != alpha + 1);

	uint64_t checkers = generate_checkers(pos, pos->turn);
	int32_t eval = evaluate(pos), best_eval = -VALUE_INFINITE;

	move_t move_list[MOVES_MAX];
	generate_quiescence(pos, move_list);

	if (!move_list[0])
		return checkers ? -VALUE_MATE + ply : eval;

	if (ply >= DEPTH_MAX)
		return checkers ? 0 : eval;

	if (pos->halfmove >= 100 || (option_history && is_repetition(pos, si->history, ply, 1 + pv_node)))
		return draw(si);


	struct transposition *e = transposition_probe(si->tt, pos);
	int32_t tteval = e ? adjust_score_mate_get(e->eval, ply) : VALUE_NONE;
	move_t ttmove = e ? e->move : 0;
	if (!pv_node && e && e->bound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
		return tteval;

	if (!checkers) {
		if (e && e->bound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
			best_eval = tteval;
		else
			best_eval = eval;

		if (best_eval >= beta)
			return beta;
		if (best_eval > alpha)
			alpha = best_eval;
	}

	struct movepicker mp;
	movepicker_init(&mp, pos, move_list, ttmove, 0, 0, si);
	move_t best_move = 0;
	move_t m;
	while ((m = next_move(&mp))) {
		if (!checkers && mp.stage > STAGE_OKCAPTURE && move_promote(&m) != 3)
			continue;
		if (option_history)
			si->history->zobrist_key[si->history->ply + ply] = pos->zobrist_key;
		do_zobrist_key(pos, &m);
		do_endgame_key(pos, &m);
		do_move(pos, &m);
		si->nodes++;
		do_accumulator(pos, &m);
		eval = -quiescence(pos, ply + 1, -beta, -alpha, si);
		undo_zobrist_key(pos, &m);
		undo_endgame_key(pos, &m);
		undo_move(pos, &m);
		undo_accumulator(pos, &m);
		if (eval > best_eval) {
			best_move = m;
			best_eval = eval;
			if (eval > alpha) {
				alpha = eval;
				if (eval >= beta)
					break;
			}
		}
	}
	assert(-VALUE_INFINITE < best_eval && best_eval < VALUE_INFINITE);
	int bound = (best_eval >= beta) ? BOUND_LOWER : (pv_node && best_move) ? BOUND_EXACT : BOUND_UPPER;
	transposition_store(si->tt, pos, adjust_score_mate_store(best_eval, ply), 0, bound, best_move);
	return best_eval;
}

/* check for ply and depth out of bound */
int32_t negamax(struct position *pos, int depth, int ply, int32_t alpha, int32_t beta, int cut_node, int flag, struct searchinfo *si) {
	if (interrupt || si->interrupt)
		return 0;
	if ((si->nodes & (0x1000 - 1)) == 0)
		check_time(si);

	int32_t eval = VALUE_NONE, best_eval = -VALUE_INFINITE;
	depth = MAX(0, depth);


	const int root_node = (ply == 0);
	const int pv_node = (beta != alpha + 1);

	assert(!(pv_node && cut_node));

	if (!root_node) {
		/* draws */
		if (pos->halfmove >= 100 || (option_history && is_repetition(pos, si->history, ply, 1 + pv_node)))
			return draw(si);

		/* mate distance pruning */
		alpha = MAX(alpha, -VALUE_MATE + ply);
		beta = MIN(beta, VALUE_MATE - ply - 1);
		if (alpha >= beta)
			return alpha;
	}

	struct transposition *e = transposition_probe(si->tt, pos);
	int32_t tteval = e ? adjust_score_mate_get(e->eval, ply) : VALUE_NONE;
	move_t ttmove = root_node ? si->pv[0][0] : e ? e->move : 0;
	if (!pv_node && e && e->depth >= depth && e->bound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
		return tteval;
	
	uint64_t checkers = generate_checkers(pos, pos->turn);
	if (depth == 0 && !checkers)
		return quiescence(pos, ply, alpha, beta, si);

	int32_t static_eval = evaluate(pos);
#if 0
	/* Futility pruning. */
	if (!pv_node && !checkers && depth <= 6 && static_eval - 200 > beta)
		return static_eval;
#endif
#if 0
	/* Null move pruning. */
	if (!pv_node && !checkers && flag != FLAG_NULL_MOVE && static_eval >= beta && depth >= 3 && has_sliding_piece(pos)) {
		int reduction = 3;
		int new_depth = CLAMP(depth - reduction, 1, depth);
		int ep = pos->en_passant;
		do_null_zobrist_key(pos, 0);
		do_null_move(pos, 0);
		if (option_history)
			si->history->zobrist_key[si->history->ply + ply] = pos->zobrist_key;
		eval = -negamax(pos, new_depth, ply + 1, -beta, -beta + 1, !cut_node, FLAG_NULL_MOVE, si);
		do_null_zobrist_key(pos, ep);
		do_null_move(pos, ep);
		if (eval >= beta)
			return beta;
	}
#endif
#if 0
	/* Internal iterative deepening. */
	if (cut_node && depth >= 2 && !ttmove) {
		int reduction = 1;
		int new_depth = depth - reduction;
		negamax(pos, new_depth, ply, alpha, beta, cut_node, FLAG_NONE, si);
		struct transposition *ne = transposition_probe(si->tt, pos);
		if (ne)
			ttmove = ne->m;
	}
#endif
#if 0
	if (pv_node && depth >= 3 && !ttmove)
        	depth -= 2;

	if (cut_node && depth >= 8 && !ttmove)
        	depth--;
#endif
#if 0
	/* Razoring. */
	if (!pv_node && !cut_node && !checkers && depth <= 1 && static_eval + 26 < alpha) {
		eval = quiescence(pos, ply + 1, alpha - 1, alpha, si);
		if (eval < alpha)
			return eval;
	}
#endif

	move_t move_list[MOVES_MAX];
	generate_all(pos, move_list);

	if (!move_list[0])
		return checkers ? -VALUE_MATE + ply : 0;

	move_t best_move = 0;

	struct movepicker mp;
	movepicker_init(&mp, pos, move_list, ttmove, si->killers[ply][0], si->killers[ply][1], si);
	move_t m;
	while ((m = next_move(&mp))) {
		int move_number = mp.index - 1;
		if (option_history)
			si->history->zobrist_key[si->history->ply + ply] = pos->zobrist_key;
		do_zobrist_key(pos, &m);
		do_endgame_key(pos, &m);
		do_move(pos, &m);
		si->nodes++;
		do_accumulator(pos, &m);

		int new_depth = depth - 1;
		/* extensions */
		int extensions = 0;
		if (ply < 2 * si->root_depth) {
			extensions = (checkers || !move_list[1]);
		}
		new_depth += extensions;
		int new_flag = FLAG_NONE;
		/* late move_t reductions */
		int full_depth_search = 0;
		if (depth >= 2 && !checkers && move_number >= (1 + pv_node) && (!move_capture(&m) || cut_node)) {
			int r = late_move_reduction(move_number, depth);
			r -= pv_node;
			r += !move_capture(&m);
			r += cut_node;
			int lmr_depth = CLAMP(new_depth - r, 1, new_depth);
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

		undo_zobrist_key(pos, &m);
		undo_endgame_key(pos, &m);
		undo_move(pos, &m);
		undo_accumulator(pos, &m);
		if (interrupt || si->interrupt)
			return 0;

		assert(eval != VALUE_NONE);
		if (eval > best_eval) {
			best_eval = eval;

			if (eval > alpha) {
				best_move = m;

				if (pv_node)
					store_pv_move(&m, ply, si->pv);
				if (!is_capture(pos, &m) && move_flag(&m) != 2)
					store_history_move(pos, &m, depth, si->history_moves);

				if (pv_node && eval < beta) {
					alpha = eval;
				}
				else {
					if (!is_capture(pos, &m) && move_flag(&m) != 2)
						store_killer_move(&m, ply, si->killers);
					break;
				}
			}
		}
	}
	int bound = (best_eval >= beta) ? BOUND_LOWER : (pv_node && best_move) ? BOUND_EXACT : BOUND_UPPER;
	transposition_store(si->tt, pos, adjust_score_mate_store(best_eval, ply), depth, bound, best_move);
	assert(-VALUE_INFINITE < best_eval && best_eval < VALUE_INFINITE);
#if 0
	if (!pv_node && !checkers) {
		eval = quiescence(pos, ply + 1, razor_alpha - 1, razor_alpha, si);
		if (eval < razor_alpha) {
			/* Razoring happens but it is in fact a fail low. */
			fprintf(f, "%d,%d,%d,%d,%d\n", depth, razor_alpha, static_eval, cut_node, bound == BOUND_UPPER);
		}
	}
#endif
	return best_eval;
}

int32_t aspiration_window(struct position *pos, int depth, int32_t last, struct searchinfo *si) {
	int32_t evaluation = VALUE_NONE;
	int32_t delta = 25 + last * last / 16384;
	int32_t alpha = MAX(last - delta, -VALUE_MATE);
	int32_t beta = MIN(last + delta, VALUE_MATE);

	while (!si->interrupt || interrupt) {
		evaluation = negamax(pos, depth, 0, alpha, beta, 0, FLAG_NONE, si);

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
	return evaluation;
}

int32_t search(struct position *pos, int depth, int verbose, int etime, int movetime, move_t *m, struct transpositiontable *tt, struct history *history, int iterative) {
	assert(option_history == (history != NULL));
	depth = MIN(depth, DEPTH_MAX);

	timepoint_t ts = time_now();
	if (etime && !movetime)
		movetime = etime / 5;

	struct searchinfo si = { 0 };
	si.time_start = ts;
	si.time_stop = movetime ? si.time_start + 1000 * movetime : 0;
	si.tt = tt;
	si.history = history;

	time_init(pos, etime, &si);

	char str[8];
	int32_t eval = VALUE_NONE, best_eval = VALUE_NONE;

	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);
	refresh_endgame_key(pos);

	if (depth == 0) {
		eval = evaluate(pos);
		if (verbose)
			printf("info depth 0 score cp %d\n", eval);
		return eval;
	}

	move_t best_move = 0;
	for (int d = iterative ? 1 : depth; d <= depth; d++) {
		si.root_depth = d;
		if (verbose)
			reset_seldepth(si.history);

		/* Minimum seems to be around d <= 5. */
		if (d <= 5 || !iterative)
			eval = negamax(pos, d, 0, -VALUE_MATE, VALUE_MATE, 0, FLAG_NONE, &si);
		else
			eval = aspiration_window(pos, d, eval, &si);

		/* 16 elo.
		 * Use move_t even from a partial and interrupted search.
		 */
		best_move = si.pv[0][0];
		best_eval = eval;

		if (interrupt || si.interrupt)
			break;

		timepoint_t tp = time_now() - ts;
		if (verbose) {
			printf("info depth %d seldepth %d score ", d, seldepth(si.history));
			if (eval >= VALUE_MATE_IN_MAX_PLY)
				printf("mate %d", (VALUE_MATE - eval + 1) / 2);
			else if (eval <= -VALUE_MATE_IN_MAX_PLY)
				printf("mate %d", (-VALUE_MATE - eval) / 2);
			else
				printf("cp %d", eval);
			printf(" nodes %ld time %ld ", si.nodes, tp / 1000);
			printf("nps %ld ", tp ? 1000000 * si.nodes / tp : 0);
			printf("pv");
			print_pv(pos, si.pv[0], 0);
			printf("\n");
		}
		if (etime && stop_searching(&si))
			break;
	}

	if (verbose && !interrupt)
		printf("bestmove %s\n", move_str_algebraic(str, &best_move));
	if (m && !interrupt)
		*m = best_move;

	return best_eval;
}

void search_init(void) {
	for (int i = 1; i < DEPTH_MAX; i++)
		/* sqrt(C) * log(i) */
		reductions[i] = (int)(21.34 * log(i));
}
