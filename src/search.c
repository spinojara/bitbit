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
#include <inttypes.h>
#include <stdatomic.h>

#include "bitboard.h"
#include "movegen.h"
#include "move.h"
#include "util.h"
#include "transposition.h"
#include "nnue.h"
#include "position.h"
#include "attackgen.h"
#include "timeman.h"
#include "evaluate.h"
#include "history.h"
#include "movepicker.h"
#include "option.h"
#include "endgame.h"

volatile atomic_int ucistop;
volatile atomic_int ucigo;
volatile atomic_int uciponder;

static int reductions[PLY_MAX] = { 0 };

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
	assert(0 <= depth && depth < PLY_MAX);
	int r = reductions[index] * reductions[depth];
#if 1
	return (r >> 10) + 1;
#else
	return ((r + 512) >> 10) + 1;
#endif
}

void print_pv(struct position *pos, move_t *pv_move, int ply) {
	struct pstate pstate;
	pstate_init(pos, &pstate);
	char str[8];
	if (!(ply < PLY_MAX) || !(pseudo_legal(pos, &pstate, pv_move) && legal(pos, &pstate, pv_move)))
		return;
	printf(" %s", move_str_algebraic(str, pv_move));
	do_move(pos, pv_move);
	print_pv(pos, pv_move + 1, ply + 1);
	undo_move(pos, pv_move);
}

void print_info(struct position *pos, struct searchinfo *si, int depth, int32_t eval, int bound) {
	timepoint_t tp = time_since(si->ti);
	printf("info depth %d seldepth %d ", depth, si->sel_depth);
	printf("score ");
	if (eval >= VALUE_MATE_IN_MAX_PLY)
		printf("mate %d", (VALUE_MATE - eval + 1) / 2);
	else if (eval <= -VALUE_MATE_IN_MAX_PLY)
		printf("mate %d", (-VALUE_MATE - eval) / 2);
	else
		printf("cp %d", eval);
	if (bound == BOUND_LOWER)
		printf(" lowerbound");
	else if (bound == BOUND_UPPER)
		printf(" upperbound");
	printf(" nodes %" PRIu64 " time %" PRId64 " ", si->nodes, tp / TPPERMS + 1);
	if (tp > 0)
		printf("nps %" PRIu64 " ", (uint64_t)((double)TPPERSEC * si->nodes / tp));
	if (tp >= TPPERSEC)
		printf("hashfull %d ", hashfull(si->tt));
	printf("pv");
	print_pv(pos, si->pv[0], 0);
	printf("\n");
}

void print_bestmove(struct position *pos, move_t best_move, move_t ponder_move) {
	char str[6];
	printf("bestmove %s", move_str_algebraic(str, &best_move));
	do_move(pos, &best_move);
	struct pstate pstate;
	pstate_init(pos, &pstate);
	if (pseudo_legal(pos, &pstate, &ponder_move) && legal(pos, &pstate, &ponder_move))
		printf(" ponder %s\n", move_str_algebraic(str, &ponder_move));
	else
		putchar('\n');
	undo_move(pos, &best_move);
}

static inline void store_killer_move(const move_t *move, int ply, move_t killers[][2]) {
	assert(0 <= ply && ply < PLY_MAX);
	assert(*move);
	if (move_compare(*move, killers[ply][0]) || move_compare(*move, killers[ply][1]))
		return;
	killers[ply][1] = killers[ply][0];
	killers[ply][0] = *move;
}

/* This option was configured for bitbit 1.2 when
 * bitbit was currently sitting at 3093 Elo ccrl.
 */
static inline int elo_skip(int32_t ply) {
	if (!option_elo)
		return 0;
	const double k = (3073 - option_elo) / 19748.0;
	return guniform() > exp(-k * ply) + eps;
}

/* Suppose that h_0 is the original value of history.
 * After adding a bonus b we get h_1 = h_0 + b.
 * We scale the value back a little so that it doesn't
 * grow too large in either direction. If we scale by
 * a real number alpha we get h_2 = alpha * h_1.
 *
 * This stabilizes whenever h_0 = h_2. This gives
 * h_1 - b = alpha * h_1 which implies h_1 * (1 - alpha) = b.
 * h_1 = b / (1 - alpha) and h_0 = b / (1 - alpha) - b =
 * b * alpha / (1 - alpha) which gives b / h_0 = 1 / alpha - 1.
 *
 * We choose e.g. alpha = 15 / 16.
 */
static inline void add_history(int64_t *history, int64_t bonus) {
	*history += bonus;
	*history -= *history / 16;
}

static inline void update_history(struct searchinfo *si, const struct position *pos, int depth, int ply, move_t *best_move, int32_t best_eval, int32_t beta, move_t *captures, move_t *quiets, const struct searchstack *ss) {
	int64_t bonus = 16 * depth * depth;

	int our_piece = pos->mailbox[move_from(best_move)];
	int their_piece = move_capture(best_move);

	if (!their_piece && move_flag(best_move) != MOVE_PROMOTION && move_flag(best_move) != MOVE_EN_PASSANT) {
		add_history(&si->quiet_history[our_piece][move_from(best_move)][move_to(best_move)], bonus);
		
		if (best_eval >= beta)
			store_killer_move(best_move, ply, si->killers);

		move_t old_move = (ss - 1)->move;
		if (old_move) {
			int square = move_to(&old_move);
			si->counter_move[pos->mailbox[square]][square] = *best_move;
		}

		for (int i = 0; quiets[i]; i++) {
			int square = move_to(&quiets[i]);
			int piece = pos->mailbox[move_from(&quiets[i])];
			add_history(&si->quiet_history[piece][move_from(&quiets[i])][square], -bonus);
		}
	}
	else if (move_flag(best_move) != MOVE_PROMOTION && move_flag(best_move) != MOVE_EN_PASSANT) {
		add_history(&si->capture_history[our_piece][their_piece][move_to(best_move)], bonus);
	}

	for (int i = 0; captures[i]; i++) {
		int square = move_to(&captures[i]);
		int piece1 = pos->mailbox[move_from(&captures[i])];
		int piece2 = move_capture(&captures[i]);
		add_history(&si->capture_history[piece1][piece2][square], -bonus);
	}
}

static inline void store_pv_move(const move_t *move, int ply, move_t pv[PLY_MAX][PLY_MAX]) {
	assert(*move);
	pv[ply][ply] = *move;
	memcpy(pv[ply] + ply + 1, pv[ply + 1] + ply + 1, sizeof(**pv) * (PLY_MAX - (ply + 1)));
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
		evaluation = evaluation * (200 - (int)pos->halfmove) / 200;
	
	evaluation = clamp(evaluation, -VALUE_MAX, VALUE_MAX);

	return evaluation;
}

int32_t quiescence(struct position *pos, int ply, int32_t alpha, int32_t beta, struct searchinfo *si, const struct pstate *pstateptr, struct searchstack *ss) {
	if (si->interrupt)
		return 0;
	if (ply >= PLY_MAX)
		return evaluate(pos);
	if ((check_time(si) || atomic_load_explicit(&ucistop, memory_order_relaxed)) && si->done_depth)
		si->interrupt = 1;

	if (si->sel_depth < ply)
		si->sel_depth = ply;

	const int pv_node = (beta != alpha + 1);

	history_store(pos, si->history, ply);

	struct pstate pstate;
	/* Skip generation of pstate if it was already generated during
	 * the previous negamax.
	 */
	if (pstateptr)
		pstate = *pstateptr;
	else
		pstate_init(pos, &pstate);

	int32_t eval = evaluate(pos), best_eval = -VALUE_INFINITE;

	if (ply >= PLY_MAX)
		return pstate.checkers ? 0 : eval;

	if (alpha < 0 && upcoming_repetition(pos, si->history, ply)) {
		alpha = draw(si);
		if (alpha >= beta)
			return alpha;
	}

	if (pos->halfmove >= 100 || repetition(pos, si->history, ply, 1 + pv_node))
		return draw(si);

	struct transposition *e = transposition_probe(si->tt, pos);
	int tthit = e != NULL;
	int32_t tteval = tthit ? adjust_score_mate_get(e->eval, ply, pos->halfmove) : VALUE_NONE;
	int ttbound = tthit ? e->bound : 0;
	move_t ttmove_unsafe = tthit ? e->move : 0;
	if (!pv_node && tthit && normal_eval(tteval) && ttbound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
		return tteval;

	if (!pstate.checkers) {
		if (tthit && normal_eval(tteval) && ttbound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
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
	movepicker_init(&mp, 1, pos, &pstate, ttmove, 0, 0, 0, si);
	move_t best_move = 0;
	move_t move;
	int move_index = -1;
	while ((move = next_move(&mp))) {
		if (!legal(pos, &pstate, &move) || elo_skip(ply))
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

		if (si->interrupt)
			return 0;

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
	if (si->interrupt)
		return 0;
	if (ply >= PLY_MAX)
		return evaluate(pos);
	if ((check_time(si) || atomic_load_explicit(&ucistop, memory_order_relaxed)) && si->done_depth)
		si->interrupt = 1;

	if (si->sel_depth < ply)
		si->sel_depth = ply;

	struct pstate pstate;
	pstate_init(pos, &pstate);
	
	if (depth <= 0 && !pstate.checkers)
		return quiescence(pos, ply, alpha, beta, si, &pstate, ss);

	history_store(pos, si->history, ply);

	int32_t eval = VALUE_NONE, best_eval = -VALUE_INFINITE;
	depth = max(0, min(depth, PLY_MAX - 1));

	assert(0 <= depth && depth < PLY_MAX);

	const int root_node = (ply == 0);
	const int pv_node = (beta != alpha + 1) || root_node;

	assert(!(pv_node && cut_node));

	if (!root_node) {
		/* Draws. */
		if (alpha < 0 && upcoming_repetition(pos, si->history, ply)) {
			alpha = draw(si);
			if (alpha >= beta)
				return alpha;
		}
		if (pos->halfmove >= 100 || repetition(pos, si->history, ply, 1 + pv_node))
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
	int32_t tteval = tthit ? adjust_score_mate_get(e->eval, ply, pos->halfmove) : VALUE_NONE;
	int ttbound = tthit ? e->bound : 0;
	int ttdepth = tthit ? e->depth : 0;
	move_t ttmove_unsafe = root_node ? si->pv[0][0] : tthit ? e->move : 0;

	if (!pv_node && tthit && normal_eval(tteval) && ttdepth >= depth && ttbound & (tteval >= beta ? BOUND_LOWER : BOUND_UPPER))
		return tteval;

	move_t ttmove = pseudo_legal(pos, &pstate, &ttmove_unsafe) ? ttmove_unsafe : 0;
	int ttcapture = ttmove ? is_capture(pos, &ttmove) : 0;

	if (pstate.checkers)
		goto skip_pruning;

#if 1
	int32_t estimated_eval = ss->static_eval = evaluate(pos);
	if (tteval != VALUE_NONE && ttbound & (tteval >= estimated_eval ? BOUND_LOWER : BOUND_UPPER))
		estimated_eval = tteval;
#endif

#if 1
	/* Razoring (37+-5 Elo). */
	if (!pv_node && depth <= 8 && estimated_eval + 100 + 150 * depth * depth < alpha) {
		eval = quiescence(pos, ply, alpha - 1, alpha, si, &pstate, ss);
		if (eval < alpha)
			return eval;
	}
#endif
#if 1
	/* Futility pruning (60+-8 Elo). */
#if 1
	if (!pv_node && depth <= 6 && estimated_eval - 200 * depth > beta)
		return estimated_eval;
#else
	if (!pv_node && depth <= 12 && estimated_eval - (100 - 40 * (cut_node && !tthit)) * depth + 70 > beta)
		return beta;
#endif
#endif
#if 1
	/* Null move pruning (43+-6 Elo). */
	if (!pv_node && (ss - 1)->move && estimated_eval >= beta && depth >= 3 && has_sliding_piece(pos)) {
		int reduction = 4;
		int new_depth = clamp(depth - reduction, 1, depth);
		int ep = pos->en_passant;
		do_null_zobrist_key(pos, 0);
		do_null_move(pos, 0);
		ss->move = 0;
		history_store(pos, si->history, ply);
		eval = -negamax(pos, new_depth, ply + 1, -beta, -beta + 1, !cut_node, si, ss + 1);
		do_null_zobrist_key(pos, ep);
		do_null_move(pos, ep);
		if (eval >= beta)
			return beta;
	}
#endif
#if 0
	/* Internal iterative deepening. */
	if (depth >= 4 && (pv_node || cut_node) && !(ttbound & BOUND_UPPER) && !excluded_move && !ttmove) {
		int reduction = 3;
		int new_depth = depth - reduction;
		negamax(pos, new_depth, ply, alpha, beta, cut_node, si, ss);
		e = transposition_probe(si->tt, pos);
		if (e) {
			/* Not this tthit, but last tthit. */
			if (!tthit) {
				tteval = adjust_score_mate_get(e->eval, ply);
				ttbound = e->bound;
				ttdepth = e->depth;
				/* Can once more update estimated_eval but maybe
				 * we won't use it anyway.
				 */
			}
			tthit = 1;
			ttmove = e->move;
			ttmove = pseudo_legal(pos, &pstate, &ttmove) ? ttmove : 0;
		}
	}
#endif
#if 0
	if (pv_node && !ttmove)
		depth = max(depth - 3 + (tthit && ttdepth >= depth), 0);
#endif

#if 0
	if (!root_node && depth <= 0)
		return quiescence(pos, ply, alpha, beta, si, &pstate, ss);
#endif

#if 0
	if (cut_node && depth >= 8 && !ttmove)
		depth -= 2;
#endif

skip_pruning:;
	move_t best_move = 0;

	move_t counter_move = 0;
	move_t old_move = (ss - 1)->move;
	if (old_move) {
		int square = move_to(&old_move);
		counter_move = si->counter_move[pos->mailbox[square]][square];
	}
	struct movepicker mp;
	movepicker_init(&mp, 0, pos, &pstate, ttmove, si->killers[ply][0], si->killers[ply][1], counter_move, si);

	move_t move, quiets[MOVES_MAX], captures[MOVES_MAX];
	int n_quiets = 0, n_captures = 0;
	int move_index = -1;
	while ((move = next_move(&mp))) {

		if (!legal(pos, &pstate, &move) || move == excluded_move || elo_skip(ply))
			continue;

		move_index++;

		int r = late_move_reduction(move_index, depth);

#if 1
		if (!root_node) {
			/* Late move pruning (85+-5 Elo). */
			if (move_index >= 4 + depth * depth)
				mp.prune = 1;
		}
#endif

		/* Extensions. */
		int extensions = 0;
#if 1
		if (ply < 2 * si->root_depth) {
#if 1
			if (!root_node && depth >= 5 && move_compare(ttmove, move) && !excluded_move && ttbound & BOUND_LOWER && ttdepth >= depth - 3) {
				int reduction = 3;
				int new_depth = depth - reduction;

				int32_t singular_beta = tteval - 2 * depth;

				ss->excluded_move = move;
				eval = negamax(pos, new_depth, ply, singular_beta - 1, singular_beta, cut_node, si, ss);
				ss->excluded_move = 0;

				/* Singular extension (29+-5 Elo).
				 * If eval < singular_beta we have the following inequalities,
				 * eval < singular_beta < tteval <= exact_eval since tteval is
				 * a lower bound. In particular all moves except ttmove fail
				 * low on [singular_beta - 1, singular_beta] and ttmove is the
				 * single best move by some margin.
				 */
				if (eval < singular_beta)
					extensions = 1;
#if 1
				/* Multi cut (6+-4 Elo).
				 * Now eval >= singular_beta. If also singular_beta >= beta
				 * we get the inequalities
				 * eval >= singular_beta >= beta and we have another move
				 * that fails high. We assume at least one move fails high
				 * on a regular search and we thus return beta.
				 */
#if 1
				else if (singular_beta >= beta && !pstate.checkers)
					return singular_beta;
#endif
				/* We get the following inequalities,
				 * singular_beta < beta <= tteval < exact_eval.
				 */
#if 0
				else if (tteval >= beta && !pstate.checkers) {
					extensions = -1;
				}
#endif
#if 0
				else if (tteval <= alpha && tteval <= eval && !pstate.checkers) {
					extensions = -1;
				}
#endif
#endif
			}
#endif
		}
#endif

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
#if 1
		if (new_depth >= 2 && !pstate.checkers && move_index >= (1 + pv_node) && (!move_capture(&move) || cut_node)) {
#else
		if (depth >= 2 && !pstate.checkers && move_index >= (1 + pv_node) && (!move_capture(&move) || cut_node)) {
#endif


#if 1
			if (pv_node)
				r -= 1;
			if (!move_capture(&move) && ttcapture)
				r += 1;
			if (cut_node && !move_compare(move, si->killers[ply][0]) && !move_compare(move, si->killers[ply][1]))
				r += 1;
#endif

#if 1
			int lmr_depth = clamp(new_depth - r, 1, new_depth);
#else
			int lmr_depth = new_depth;
#endif
			/* Since this is a child of either a pv, all, or cut node and it is not the first
			 * child it is an expected cut node. Instead of searching in [-beta, -alpha], we
			 * expect there to be a cut and it should suffice to search in [-alpha - 1, -alpha].
			 */
			eval = -negamax(pos, lmr_depth, ply + 1, -alpha - 1, -alpha, 1, si, ss + 1);

			/* If eval > alpha, then negamax < -alpha but we expected negamax >= -alpha. We
			 * must therefore research this node.
			 */
			if (eval > alpha && new_depth > lmr_depth)
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

		if (si->interrupt)
			return 0;

		assert(eval != VALUE_NONE);
		if (eval > best_eval) {
			best_eval = eval;

			if (eval > alpha) {
				best_move = move;

				if (pv_node)
					store_pv_move(&move, ply, si->pv);

				if (pv_node && eval < beta)
					alpha = eval;
				else
					break;
			}
		}
		/* If this move was not good we reduce its stats.
		 * Note that in non-pv nodes we will only ever
		 * have one best move and the problem of overwriting
		 * best moves will not be a problem.
		 */
		if (!move_compare(move, best_move)) {
			if (move_capture(&move))
				captures[n_captures++] = move;
			else
				quiets[n_quiets++] = move;
		}
	}
	if (move_index == -1) {
		best_eval = excluded_move ? alpha :
			pstate.checkers ? -VALUE_MATE + ply : 0;
	}
	else if (best_move) {
		captures[n_captures] = quiets[n_quiets] = 0;
		update_history(si, pos, depth, ply, &best_move, best_eval, beta, captures, quiets, ss);
	}
	int bound = (best_eval >= beta) ? BOUND_LOWER : (pv_node && best_move) ? BOUND_EXACT : BOUND_UPPER;
	if (!excluded_move)
		transposition_store(si->tt, pos, adjust_score_mate_store(best_eval, ply), depth, bound, best_move);
	assert(-VALUE_INFINITE < best_eval && best_eval < VALUE_INFINITE);

	return best_eval;
}

int32_t aspiration_window(struct position *pos, int depth, int verbose, int32_t last, struct searchinfo *si, struct searchstack *ss) {
	int32_t eval = VALUE_NONE;
	int32_t delta = 25 + last * last / 16384;
	int32_t alpha = max(last - delta, -VALUE_MATE);
	int32_t beta = min(last + delta, VALUE_MATE);

	while (1) {
		eval = negamax(pos, depth, 0, alpha, beta, 0, si, ss);
		if (si->interrupt)
			break;

		int bound = 0;
		if (eval <= alpha) {
			eval = alpha;
			bound = BOUND_UPPER;
			alpha = max(alpha - delta, -VALUE_MATE);
			beta = (alpha + 3 * beta) / 4;
		}
		else if (eval >= beta) {
			eval = beta;
			bound = BOUND_LOWER;
			beta = min(beta + delta, VALUE_MATE);
		}
		else {
			return eval;
		}

		if (verbose)
			print_info(pos, si, depth, eval, bound);

		delta += delta / 3;
	}
	return eval;
}

int32_t search(struct position *pos, int depth, int verbose, struct timeinfo *ti, move_t move[2], struct transpositiontable *tt, struct history *history, int iterative) {
	assert(option_history == (history != NULL));
	if (depth <= 0)
		depth = PLY_MAX;
	depth = min(depth, PLY_MAX / 2);

	struct searchinfo si = { 0 };
	si.ti = ti;
	si.tt = tt;
	si.history = history;

	struct searchstack ss[PLY_MAX + 1] = { 0 };

	time_init(pos, si.ti);

	int32_t eval = VALUE_NONE;

	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);
	refresh_endgame_key(pos);
	refresh_zobrist_key(pos);

	move_t best_move = 0, ponder_move = 0;
	for (int d = iterative ? 1 : depth; d <= depth; d++) {
		si.root_depth = d;
		si.sel_depth = 1;

		/* Minimum seems to be around d <= 5. */
		if (d <= 5 || !iterative)
			eval = negamax(pos, d, 0, -VALUE_MATE, VALUE_MATE, 0, &si, ss + 1);
		else
			eval = aspiration_window(pos, d, verbose, eval, &si, ss + 1);

		/* 16 elo.
		 * Use move even from a partial and interrupted search.
		 */
		best_move = si.pv[0][0];
		ponder_move = si.pv[0][1];

		if (si.interrupt)
			break;

		si.done_depth = d;

		if (verbose)
			print_info(pos, &si, d, eval, 0);

		if (stop_searching(si.ti, best_move))
			break;
	}

	/* We are not allowed to exit the search before either a ponderhit
	 * or stop command. Both of these commands will set uciponder to 0.
	 */
	while (atomic_load_explicit(&uciponder, memory_order_relaxed));

	if (move) {
		move[0] = best_move;
		move[1] = ponder_move;
	}

	return eval;
}

void search_init(void) {
	for (int i = 1; i < PLY_MAX; i++)
		/* sqrt(C) * log(i) */
		reductions[i] = (int)(21.34 * log(i));
}
