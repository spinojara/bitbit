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

uint64_t nodes = 0;

static int reductions[256];

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
int late_move_reduction(int index, int depth) {
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
	if (!killer_moves)
		return;
	killer_moves[ply][1] = killer_moves[ply][0];
	killer_moves[ply][0] = *m & 0xFFFF;
}

static inline void store_history_move(struct position *pos, move *m, uint8_t depth, uint64_t history_moves[13][64]) {
	if (!history_moves)
		return;
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
		if ((*ptr & 0xFFFF) == pv_moves[0][ply])
			return 1;
	return 0;
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

int16_t quiescence(struct position *pos, int16_t alpha, int16_t beta, clock_t clock_stop, uint64_t history_moves[13][64]) {
	if (interrupt)
		return 0;
	if (nodes % 4096 == 0)
		if (clock_stop && clock() > clock_stop)
			interrupt = 1;

	/* 0 value will never be used, only so the compiler does not warn */
	int16_t evaluation = 0;
	uint64_t checkers = generate_checkers(pos, pos->turn);
	
	evaluation = evaluate(pos);
	nodes++;
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
	move *ptr = order_moves(pos, move_list, evaluation_list, 0, 0, NULL, 0, NULL, NULL, history_moves);
	for (; *ptr; next_move(move_list, evaluation_list, &ptr)) {
		if (is_capture(pos, ptr) && !see_geq(pos, ptr, -100) && !checkers)
			continue;
		do_move(pos, ptr);
#if defined(NNUE)
		do_accumulator(pos, ptr);
#endif
		evaluation = -quiescence(pos, -beta, -alpha, clock_stop, history_moves);
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

int16_t negamax(struct position *pos, uint8_t depth, uint8_t ply, int16_t alpha, int16_t beta, int null_move, clock_t clock_stop, int *pv_flag, move pv_moves[][256], move killer_moves[][2], uint64_t history_moves[13][64], struct history *history) {
	int16_t evaluation;

	if (interrupt)
		return 0;
	if (nodes % 4096 == 0)
		if (clock_stop && clock() > clock_stop)
			interrupt = 1;

	if (ply <= 2 && history && is_threefold(pos, history))
		return 0;

#if defined(TRANSPOSITION)
	void *e = attempt_get(pos);
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
#else
	void *e = NULL;
#endif
	
	if (depth <= 0) {
		evaluation = mate(pos);
		/* stalemate */
		if (evaluation == 1)
			return 0;
		/* checkmate */
		if (evaluation == 2)
			return -0x7F00;
		return quiescence(pos, alpha, beta, clock_stop, history_moves);
	}

	uint64_t checkers = generate_checkers(pos, pos->turn);
	/* null move pruning */
	if (!null_move && !(*pv_flag) && !checkers && depth >= 3 && has_big_piece(pos)) {
		int t = pos->en_passant;
		do_null_move(pos, 0);
#if defined(TRANSPOSITION)
		do_null_zobrist_key(pos, 0);
#endif
		evaluation = -negamax(pos, depth - 3, ply + 1, -beta, -beta + 1, 1, clock_stop, pv_flag, NULL, NULL, history_moves, NULL);
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
		return checkers ? -0x7F00 : 0;

	if (*pv_flag && !contains_pv_move(move_list, ply, pv_moves))
		*pv_flag = 0;

	if (pos->halfmove >= 100)
		return 0;

	uint64_t evaluation_list[MOVES_MAX];
	
#if defined(TRANSPOSITION)
	if (e)
		transposition_set_open(e);
#endif

	uint16_t m = 0;
#if !defined(TRANSPOSITION)
	UNUSED(m);
#endif
	int flag = 0;
	if (ply > 15 && 0) {
		flag = 1;
		char fen[128];
		print_position(pos, 0);
		printf("%s\n", pos_to_fen(fen, pos));
	}
	move *ptr = order_moves(pos, move_list, evaluation_list, depth, ply, e, *pv_flag, pv_moves, killer_moves, history_moves);
	for (; *ptr; next_move(move_list, evaluation_list, &ptr)) {
		if (flag) {
			char str[8];
			printf("%s, %lu\n", move_str_pgn(str, pos, ptr), evaluation_list[ptr - move_list]);
			continue;
		}
#if defined(TRANSPOSITION)
		do_zobrist_key(pos, ptr);
#endif
		do_move(pos, ptr);
#if defined(NNUE)
		do_accumulator(pos, ptr);
#endif
		/* late move reduction */
		if (!(*pv_flag) && depth >= 2 && !checkers && ptr - move_list >= 1 && move_flag(ptr) != 2 && is_capture(pos, ptr)) {
			uint8_t r = late_move_reduction(ptr - move_list, depth);
			evaluation = -negamax(pos, MAX(depth - 1 - r, 0), ply + 1, -alpha - 1, -alpha, 0, clock_stop, pv_flag, pv_moves, killer_moves, history_moves, history);
		}
		else {
			evaluation = alpha + 1;
		}
		if (evaluation > alpha) {
			/* -beta - 1 to search for mate in <n> */
			evaluation = -negamax(pos, depth - 1, ply + 1, -beta - 1, -alpha, 0, clock_stop, pv_flag, pv_moves, killer_moves, history_moves, history);
		}
		
		evaluation -= (evaluation > 0x4000);
#if defined(TRANSPOSITION)
		undo_zobrist_key(pos, ptr);
#endif
		undo_move(pos, ptr);
#if defined(NNUE)
		undo_accumulator(pos, ptr);
#endif
		if (evaluation >= beta) {
			/* quiet */
			if (!is_capture(pos, ptr) && move_flag(ptr) != 2)
				store_killer_move(ptr, ply, killer_moves);
#if defined(TRANSPOSITION)
			if (e)
				transposition_set_closed(e);
			/* type cut */
			attempt_store(pos, beta, depth, 1, *ptr);
#endif
			return beta;
		}
		if (evaluation > alpha) {
			/* quiet */
			if (!is_capture(pos, ptr) && move_flag(ptr) != 2)
				store_history_move(pos, ptr, depth, history_moves);
			alpha = evaluation;
			store_pv_move(ptr, ply, pv_moves);
#if defined(TRANSPOSITION)
			m = *ptr;
#endif
		}
	}
	if (flag)
		exit(1);
#if defined(TRANSPOSITION)
	if (e)
		transposition_set_closed(e);
	/* type pv or all */
	attempt_store(pos, alpha, depth, m ? 0 : 2, m & 0xFFFF);
#endif
	return alpha;
}

int16_t search(struct position *pos, uint8_t depth, int verbose, int etime, int movetime, move *m, struct history *history, int iterative) {
	clock_t clock_start = clock();
	if (etime && !movetime)
		movetime = etime / 5;
	clock_t clock_stop = movetime ? clock() + (CLOCKS_PER_SEC * movetime) / 1000 : 0;

	char str[8];
	int16_t evaluation = 0, saved_evaluation[256] = { 0 };

	move move_list[MOVES_MAX];
	uint64_t evaluation_list[MOVES_MAX];
	generate_all(pos, move_list);

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
				printf("info depth 0 score cp 0 string stalemate\n");
		}
		return evaluation;
	}

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

	move pv_moves[256][256];
	move killer_moves[256][2];
	uint64_t history_moves[13][64];
	memset(pv_moves, 0, sizeof(pv_moves));
	memset(killer_moves, 0, sizeof(killer_moves));
	memset(history_moves, 0, sizeof(history_moves));

	int16_t alpha, beta;
	int pv_flag;
	move bestmove = 0;
	uint8_t d;
	int delta = 35;
	int last = 0;
	int aspiration_window = 0;
	for (d = iterative ? 1 : depth; d <= depth; d++) {
		if (!aspiration_window)
			nodes = 0;
		pv_flag = 1;
		alpha = -0x7F00;
		beta = 0x7F00;

		if (aspiration_window) {
			alpha = last - delta;
			beta = last + delta;
		}

		move *ptr = order_moves(pos, move_list, evaluation_list, d, 0, NULL, pv_flag, pv_moves, killer_moves, history_moves);

		for (; *ptr; next_move(move_list, evaluation_list, &ptr)) {
#if defined(TRANSPOSITION)
			do_zobrist_key(pos, ptr);
#endif
			do_move(pos, ptr);
#if defined(NNUE)
			do_accumulator(pos, ptr);
#endif
			evaluation = -negamax(pos, d - 1, 1, -beta, -alpha, 0, clock_stop, &pv_flag, pv_moves, killer_moves, history_moves, history);
			evaluation -= (evaluation > 0x4000);
#if defined(TRANSPOSITION)
			undo_zobrist_key(pos, ptr);
#endif
			undo_move(pos, ptr);
#if defined(NNUE)
			undo_accumulator(pos, ptr);
#endif
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
		else if (d >= 5)
			aspiration_window = 1;

		bestmove = pv_moves[0][0];

		clock_t clocks = clock() - clock_start;
		if (verbose) {
			printf("info depth %d score ", d);
			if (evaluation < -0x4000)
				printf("mate %d", -0x7F00 - evaluation);
			else if (evaluation > 0x4000)
				printf("mate %d", 0x7F00 - evaluation);
			else
				printf("cp %d", evaluation);
			printf(" nodes %ld time %ld ", nodes, 1000 * clocks / CLOCKS_PER_SEC);
			printf("nps %ld ", clocks ? nodes * CLOCKS_PER_SEC / clocks : 0);
			printf("pv");
			print_pv(pos, pv_moves[0], 0);
			printf("\n");
		}
		if (etime && 3 * 1000 * (clock() - clock_start) > 2 * time_man(etime, saved_evaluation, d) * CLOCKS_PER_SEC)
			break;

		/* stop searching if mate is found */
		if (etime && evaluation < -0x4000 && 2 * (0x7F00 + evaluation) <= d)
			break;
		if (etime && evaluation > 0x4000 && 2 * (0x7F00 - evaluation) - 1 <= d)
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
		reductions[i] = (int)(21.35 * log(i));
	}
}
