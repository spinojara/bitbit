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
	struct pstate pstate;
	pstate_init(pos, &pstate);
	char str[8];
	if (!(ply < DEPTH_MAX) || !(pseudo_legal(pos, &pstate, pv_move) && legal(pos, &pstate, pv_move)))
		return;
	printf(" %s", move_str_algebraic(str, pv_move));
	do_move(pos, pv_move);
	print_pv(pos, pv_move + 1, ply + 1);
	undo_move(pos, pv_move);
	*pv_move = *pv_move & 0xFFFF;
}

static inline void store_killer_move(const move_t *move, int ply, move_t killers[][2]) {
	if (interrupt)
		return;
	assert(0 <= ply && ply < DEPTH_MAX);
	assert(*move);
	if ((*move & 0xFFFF) == killers[ply][0])
		return;
	killers[ply][1] = killers[ply][0];
	killers[ply][0] = *move & 0xFFFF;
}

static inline void store_history_move(const struct position *pos, const move_t *move, int depth, int64_t history_moves[13][64]) {
	if (interrupt)
		return;
	assert(*move);
	assert(0 <= depth && depth < DEPTH_MAX);
	history_moves[pos->mailbox[move_from(move)]][move_to(move)] += (uint64_t)1 << min(depth, 32);
}

static inline void store_pv_move(const move_t *move, int ply, move_t pv[DEPTH_MAX][DEPTH_MAX]) {
	if (interrupt)
		return;
	assert(*move);
	pv[ply][ply] = *move & 0xFFFF;
	memcpy(pv[ply] + ply + 1, pv[ply + 1] + ply + 1, sizeof(**pv) * (DEPTH_MAX - (ply + 1)));
}

/* Random drawn score to avoid threefold blindness. */
static inline int32_t draw(const struct searchinfo *si) {
	return 2 * (si->nodes & 0x3) - 3;
}

static inline int32_t evaluate(const struct position *pos) {
	int32_t evaluation;
	struct endgame *e = endgame_probe(pos);
	if (e && (evaluation = endgame_evaluate(e, pos)) != VALUE_NONE)
		return evaluation;

	int classical = 0;
	/* NNUE. */
	if (option_nnue) {
		int32_t psqt = abs(pos->psqtaccumulation[WHITE] - pos->psqtaccumulation[BLACK]) / 2;
		if (psqt > 350)
			classical = 1;
		else
			evaluation = evaluate_accumulator(pos);
	}
	/* Classical. */
	if (!option_nnue || classical) {
		evaluation = evaluate_classical(pos);
	}

	/* Damp when shuffling pieces. */
	if (option_damp)
		evaluation = evaluation * (200 - pos->halfmove) / 200;
	
	evaluation = clamp(evaluation, -VALUE_MAX, VALUE_MAX);

	return evaluation;
}

int32_t quiescence(struct position *pos, int ply, int32_t alpha, int32_t beta, struct searchinfo *si, const struct pstate *pstateptr, struct searchstack *ss) {
	if (interrupt || si->interrupt)
		return 0;
	if (ply >= DEPTH_MAX)
		return evaluate(pos);
	if ((si->nodes & (0x1000 - 1)) == 0)
		check_time(si);

	const int pv_node = (beta != alpha + 1);

	if (si->history)
		si->history->zobrist_key[si->history->ply + ply] = pos->zobrist_key;

	struct pstate pstate;
	/* Skip generation of pstate if it was already generated during
	 * the previous negamax.
	 */
	if (pstateptr)
		pstate = *pstateptr;
	else
		pstate_init(pos, &pstate);

	int32_t eval = evaluate(pos), best_eval = -VALUE_INFINITE;

	if (ply >= DEPTH_MAX)
		return pstate.checkers ? 0 : eval;

	if (pos->halfmove >= 100 || (option_history && is_repetition(pos, si->history, ply, 1 + pv_node)))
		return draw(si);

	struct transposition *e = transposition_probe(si->tt, pos);
	int tthit = e != NULL;
	int32_t tteval = tthit ? adjust_score_mate_get(e->eval, ply) : VALUE_NONE;
	int ttbound = tthit ? e->bound : 0;
	move_t ttmove_unsafe = tthit ? e->move : 0;
	if (!pv_node && tthit && ttbound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
		return tteval;

	if (!pstate.checkers) {
		if (tthit && ttbound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
			best_eval = tteval;
		else
			best_eval = eval;

		if (best_eval >= beta)
			return beta;
		if (best_eval > alpha)
			alpha = best_eval;
	}

	move_t ttmove = pseudo_legal(pos, &pstate, &ttmove_unsafe) ? ttmove_unsafe : 0;
	struct movepicker mp;
	movepicker_init(&mp, 1, pos, &pstate, ttmove, 0, 0, si);
	move_t best_move = 0;
	move_t move;
	int move_index = -1;
	while ((move = next_move(&mp))) {
		if (!legal(pos, &pstate, &move))
			continue;

		move_index++;

		do_zobrist_key(pos, &move);
		do_endgame_key(pos, &move);
		do_move(pos, &move);
		ss->move = move;
		si->nodes++;
		do_accumulator(pos, &move);
		eval = -quiescence(pos, ply + 1, -beta, -alpha, si, NULL, ss + 1);
		undo_zobrist_key(pos, &move);
		undo_endgame_key(pos, &move);
		undo_move(pos, &move);
		undo_accumulator(pos, &move);
		if (eval > best_eval) {
			best_move = move;
			best_eval = eval;
			if (eval > alpha) {
				alpha = eval;
				if (eval >= beta)
					break;
			}
		}
	}
	if (move_index == -1 && pstate.checkers)
		best_eval = -VALUE_MATE + ply;
	assert(-VALUE_INFINITE < best_eval && best_eval < VALUE_INFINITE);
	int bound = (best_eval >= beta) ? BOUND_LOWER : (pv_node && best_move) ? BOUND_EXACT : BOUND_UPPER;
	transposition_store(si->tt, pos, adjust_score_mate_store(best_eval, ply), 0, bound, best_move);
	return best_eval;
}

int32_t negamax(struct position *pos, int depth, int ply, int32_t alpha, int32_t beta, int cut_node, struct searchinfo *si, struct searchstack *ss) {
	if (interrupt || si->interrupt)
		return 0;
	if (ply >= DEPTH_MAX)
		return evaluate(pos);
	if ((si->nodes & (0x1000 - 1)) == 0)
		check_time(si);

	if (si->history)
		si->history->zobrist_key[si->history->ply + ply] = pos->zobrist_key;

	int32_t eval = VALUE_NONE, best_eval = -VALUE_INFINITE;
	depth = max(0, depth);

	assert(0 <= depth && depth < DEPTH_MAX);

	const int root_node = (ply == 0);
	const int pv_node = (beta != alpha + 1);

	assert(!(pv_node && cut_node));

	if (!root_node) {
		/* Draws. */
		if (pos->halfmove >= 100 || (option_history && is_repetition(pos, si->history, ply, 1 + pv_node)))
			return draw(si);

		/* Mate distance pruning. */
		alpha = max(alpha, -VALUE_MATE + ply);
		beta = min(beta, VALUE_MATE - ply - 1);
		if (alpha >= beta)
			return alpha;
	}

	move_t excluded_move = ss->excluded_move;

	struct transposition *e = excluded_move ? NULL : transposition_probe(si->tt, pos);
	int tthit = e != NULL;
	int32_t tteval = tthit ? adjust_score_mate_get(e->eval, ply) : VALUE_NONE;
	int ttbound = tthit ? e->bound : 0;
	int ttdepth = tthit ? e->depth : 0;
	move_t ttmove_unsafe = root_node ? si->pv[0][0] : tthit ? e->move : 0;
	if (!pv_node && tthit && ttdepth >= depth && ttbound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
		return tteval;

	struct pstate pstate;
	pstate_init(pos, &pstate);
	
	if (depth == 0 && !pstate.checkers)
		return quiescence(pos, ply, alpha, beta, si, &pstate, ss);

	move_t ttmove = pseudo_legal(pos, &pstate, &ttmove_unsafe) ? ttmove_unsafe : 0;
	int ttcapture = ttmove ? is_capture(pos, &ttmove) : 0;

	if (pstate.checkers)
		goto skip_pruning;

#if 0
	int32_t static_eval = evaluate(pos);
	if (tteval != VALUE_NONE && ttbound & (tteval >= static_eval ? BOUND_LOWER : BOUND_UPPER))
		static_eval = tteval;
#endif

#if 0
	/* Razoring. */
	if (!pv_node && depth <= 8 && static_eval + 100 + 150 * depth * depth < alpha) {
		char fen[128];
		printf("Trying to razor in interval %d, %d\n", alpha, beta);
		printf("%s\n", pos_to_fen(fen, pos));
		eval = quiescence(pos, ply, alpha - 1, alpha, si, &pstate, ss);
		if (eval < alpha)
			return eval;
		printf("Didn't razor\n");
	}
#endif
#if 0
	/* Futility pruning. */
	if (!pv_node && depth <= 6 && static_eval - 200 > beta)
		return static_eval;
#endif
#if 0
	/* Null move pruning. */
	if (!pv_node && (ss - 1)->move && depth >= 3 && has_sliding_piece(pos)) {
		int reduction = 3;
		int new_depth = clamp(depth - reduction, 1, depth);
		int ep = pos->en_passant;
		do_null_zobrist_key(pos, 0);
		do_null_move(pos, 0);
		ss->move = 0;
		if (option_history)
			si->history->zobrist_key[si->history->ply + ply] = pos->zobrist_key;
		eval = -negamax(pos, new_depth, ply + 1, -beta, -beta + 1, !cut_node, si, ss + 1);
		do_null_zobrist_key(pos, ep);
		do_null_move(pos, ep);
		if (eval >= beta)
			return beta;
	}
#endif
#if 0
	/* Internal iterative deepening. */
	if ((pv_node || cut_node) && !excluded_move && depth >= 4 && !ttmove) {
		int reduction = 3;
		int new_depth = depth - reduction;
		negamax(pos, new_depth, ply, -VALUE_MATE, beta, cut_node, si, ss);
		e = transposition_probe(si->tt, pos);
		if (e) {
			/* Not this tthit, but last tthit. */
			if (!tthit) {
				tteval = adjust_score_mate_get(e->eval, ply);
				ttbound = e->bound;
				ttdepth = e->depth;
				/* Can once more update static_eval but maybe
				 * we won't use it anyway.
				 */
			}
			tthit = 1;
			ttmove = e->move;
		}
	}
#endif
#if 0
	if (pv_node && depth >= 3 && !ttmove)
        	depth -= 2;

	if (cut_node && depth >= 8 && !ttmove)
        	depth--;
#endif

skip_pruning:;
	move_t best_move = 0;

	struct movepicker mp;
	movepicker_init(&mp, 0, pos, &pstate, ttmove, si->killers[ply][0], si->killers[ply][1], si);

	move_t move;
	int move_index = -1;
	while ((move = next_move(&mp))) {
		if (!legal(pos, &pstate, &move) || move == excluded_move)
			continue;

		move_index++;

#if 0
		if (!root_node && !pstate.checkers && move_index > depth * depth / 2)
			mp.quiescence = 1;
#endif

		/* Extensions. */
		int extensions = 0;
		if (ply < 2 * si->root_depth) {
			if (pstate.checkers) {
				extensions = 1;
			}
#if 0
			else if (!root_node && depth >= 5 && ttmove == move && !excluded_move && e->bound & BOUND_LOWER && e->depth >= depth - 3) {
				int reduction = 3;
				int new_depth = depth - reduction;

				int32_t singular_beta = tteval - 4 * depth;

				ss->excluded_move = move;
				eval = negamax(pos, new_depth, ply, singular_beta - 1, singular_beta, cut_node, si, ss);
				ss->excluded_move = 0;

				//char fen[128];
				/* If eval < singular_beta we have the following inequalities,
				 * eval < singular_beta < tteval < exact_tteval since tteval is
				 * a lower bound. In particular all moves except ttmove fail
				 * low on [singular_beta - 1, singular_beta] and ttmove is the
				 * single best move by some margin.
				 */
				if (eval < singular_beta) {
#if 0
					printf("%s\n", pos_to_fen(fen, pos));
					print_move(&ttmove);
					printf("\n%d\n", tteval);
					printf("%d\n", singular_beta);
					printf("%d\n", eval);
#endif
					extensions = 1;
				}
#if 0
				/* If singular_beta >= beta and the search did not fail low,
				 * it failed high. ttmove fails high for the search [alpha, beta]
				 * and some other move fails high for [singular_beta - 1, singular_beta].
				 * In particular since singular_beta >= beta, the move fails high also for
				 * [alpha, beta] and there are at least 2 moves which make the search
				 * [alpha, beta] fail high. We assume that at least one of the two moves
				 * fail high also for the higher depth search and we have a beta cutoff.
				 */
#if 1
				else if (singular_beta >= beta) {
					return singular_beta;
				}
#endif
#if 1
				/* We get the following inequalities,
				 * eval < singular_beta < beta < tteval < exact_tteval.
				 */
				else if (tteval >= beta) {
					extensions = -2;
				}
				else if (tteval <= alpha && tteval <= eval) {
					extensions = -1;
				}
#endif
#endif
			}
#endif
		}

		do_zobrist_key(pos, &move);
		do_endgame_key(pos, &move);
		do_move(pos, &move);
		ss->move = move;
		si->nodes++;
		do_accumulator(pos, &move);

		int new_depth = depth - 1;

		new_depth += extensions;

		/* Late move reductions. */
		int full_depth_search = 0;
		if (depth >= 2 && !pstate.checkers && move_index >= (1 + pv_node) && (!move_capture(&move) || cut_node)) {
			int r = late_move_reduction(move_index, depth);

			if (pv_node)
				r -= 1;
			if (!move_capture(&move) && ttcapture)
				r += 1;
			if (cut_node && move != si->killers[ply][0] && move != si->killers[ply][1])
				r += 1;

			int lmr_depth = clamp(new_depth - r, 1, new_depth);
			/* Since this is a child of either a pv, all, or cut node and it is not the first
			 * child it is an expected cut node. Instead of searching in [-beta, -alpha], we
			 * expect there to be a cut and it should suffice to search in [-alpha - 1, -alpha].
			 */
			eval = -negamax(pos, lmr_depth, ply + 1, -alpha - 1, -alpha, 1, si, ss + 1);

			/* If eval > alpha, then negamax < -alpha but we expected negamax >= -alpha. We
			 * must therefore research this node.
			 */
			if (eval > alpha)
				full_depth_search = 1;
		}
		else {
			/* If this is not a pv node, then alpha = beta + 1, or -beta = -alpha - 1.
			 * Our full depth search will thus search in the full interval [-beta, -alpha].
			 * If this is a pv node and we are not at the first child, then the child is
			 * an expected cut node. We should thus do a full depth search.
			 */
			full_depth_search = !pv_node || move_index;
		}

		/* Do a full depth search of our node. This is no longer neccessarily an expected cut node
		 * since it is possibly the first child of a cut node.
		 */
		if (full_depth_search)
			eval = -negamax(pos, new_depth, ply + 1, -alpha - 1, -alpha, !cut_node || move_index, si, ss + 1);

		/* We should only do this search for new possible pv nodes. There are two cases.
		 * For the first case we are in a pv node and it is our first child, this is a pv
		 * node.
		 * For the second case we are in a pv node and it is not our first child. Our
		 * previous full depth search was expected to fail high but it did not. In fact
		 * eval > alpha again implies negamax < -alpha, but we expected negamax >= -alpha.
		 */
		if (pv_node && (!move_index || (eval > alpha && (root_node || eval < beta))))
			eval = -negamax(pos, new_depth, ply + 1, -beta, -alpha, 0, si, ss + 1);

		undo_zobrist_key(pos, &move);
		undo_endgame_key(pos, &move);
		undo_move(pos, &move);
		undo_accumulator(pos, &move);
		if (interrupt || si->interrupt)
			return 0;

		assert(eval != VALUE_NONE);
		if (eval > best_eval) {
			best_eval = eval;

			if (eval > alpha) {
				best_move = move;

				if (pv_node)
					store_pv_move(&move, ply, si->pv);
				if (!is_capture(pos, &move) && move_flag(&move) != 2)
					store_history_move(pos, &move, depth, si->history_moves);

				if (pv_node && eval < beta) {
					alpha = eval;
				}
				else {
					if (!is_capture(pos, &move) && move_flag(&move) != 2)
						store_killer_move(&move, ply, si->killers);
					break;
				}
			}
		}
	}
	if (move_index == -1)
		best_eval = excluded_move ? alpha :
			pstate.checkers ? -VALUE_MATE + ply : 0;
	int bound = (best_eval >= beta) ? BOUND_LOWER : (pv_node && best_move) ? BOUND_EXACT : BOUND_UPPER;
	if (!excluded_move)
		transposition_store(si->tt, pos, adjust_score_mate_store(best_eval, ply), depth, bound, best_move);
	assert(-VALUE_INFINITE < best_eval && best_eval < VALUE_INFINITE);

	return best_eval;
}

int32_t aspiration_window(struct position *pos, int depth, int32_t last, struct searchinfo *si, struct searchstack *ss) {
	int32_t evaluation = VALUE_NONE;
	int32_t delta = 25 + last * last / 16384;
	int32_t alpha = max(last - delta, -VALUE_MATE);
	int32_t beta = min(last + delta, VALUE_MATE);

	while (!si->interrupt || interrupt) {
		evaluation = negamax(pos, depth, 0, alpha, beta, 0, si, ss);

		if (evaluation <= alpha) {
			alpha = max(alpha - delta, -VALUE_MATE);
			beta = (alpha + 3 * beta) / 4;
		}
		else if (evaluation >= beta) {
			beta = min(beta + delta, VALUE_MATE);
		}
		else {
			return evaluation;
		}

		delta += delta / 3;
	}
	return evaluation;
}

int32_t search(struct position *pos, int depth, int verbose, int etime, int movetime, move_t *move, struct transpositiontable *tt, struct history *history, int iterative) {
	assert(option_history == (history != NULL));
	depth = min(depth, DEPTH_MAX);

	timepoint_t ts = time_now();
	if (etime && !movetime)
		movetime = etime / 5;

	struct searchinfo si = { 0 };
	si.time_start = ts;
	si.time_stop = movetime ? si.time_start + 1000 * movetime : 0;
	si.tt = tt;
	si.history = history;

	struct searchstack ss[DEPTH_MAX + 1] = { 0 };

	time_init(pos, etime, &si);

	char str[8];
	int32_t eval = VALUE_NONE, best_eval = VALUE_NONE;

	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);
	refresh_endgame_key(pos);
	refresh_zobrist_key(pos);

	if (depth == 0) {
		eval = evaluate(pos);
		if (verbose)
			printf("info depth 0 score cp %d\n", eval);
		return eval;
	}

	move_t best_move = 0;
	for (int d = iterative ? 1 : depth; d <= depth; d++) {
		si.root_depth = d;
		if (verbose && history)
			reset_seldepth(si.history);

		/* Minimum seems to be around d <= 5. */
		if (d <= 5 || !iterative)
			eval = negamax(pos, d, 0, -VALUE_MATE, VALUE_MATE, 0, &si, ss + 1);
		else
			eval = aspiration_window(pos, d, eval, &si, ss + 1);

		/* 16 elo.
		 * Use move even from a partial and interrupted search.
		 */
		best_move = si.pv[0][0];
		best_eval = eval;

		if (interrupt || si.interrupt)
			break;

		timepoint_t tp = time_now() - ts;
		if (verbose) {
			printf("info depth %d ", d);
			if (history)
				printf("seldepth %d ", seldepth(si.history));
			printf("score ");
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
	if (move && !interrupt)
		*move = best_move;

	return best_eval;
}

void search_init(void) {
	for (int i = 1; i < DEPTH_MAX; i++)
		/* sqrt(C) * log(i) */
		reductions[i] = (int)(21.34 * log(i));
}
