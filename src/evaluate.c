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

unsigned int nodes = 0;

uint32_t mvv_lva_lookup[13 * 13];

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

uint32_t mvv_lva_calc(int attacker, int victim) {
	int a = (attacker - 1) % 6;
	int v = (victim - 1) % 6;
	int lookup_t[6 * 6] = {
		10, 18, 19, 20, 24,  0,
		 6, 11, 15, 17, 23,  0,
		 5,  9, 12, 16, 22,  0,
		 4,  7,  8, 13, 21,  0,
		 0,  1,  2,  3, 14,  0,
		 0,  0,  0, 25, 26,  0,
	};
	return lookup_t[v + 6 * a] + ((uint64_t)1 << 31);
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

int is_threefold(struct position *pos, struct history *history) {
	int count;
	struct history *t;
	for (t = history, count = 0; t; t = t->previous)
		if (pos_are_equal(pos, t->pos))
			count++;
	return count >= 2;
}

static inline uint32_t mvv_lva(int attacker, int victim) {
	return mvv_lva_lookup[victim + 13 * attacker];
}

static inline void store_killer_move(move *m, uint8_t depth, move killer_moves[][2]) {
	killer_moves[depth][1] = killer_moves[depth][0];
	killer_moves[depth][0] = *m & 0xFFFF;
}

static inline void store_history_move(struct position *pos, move *m, uint8_t depth, uint64_t history_moves[13][64]) {
	history_moves[pos->mailbox[move_from(m)]][move_to(m)] += (uint64_t)1 << depth;
}

uint32_t evaluate_move(struct position *pos, move *m, uint8_t depth, struct transposition *e, move killer_moves[][2], uint64_t history_moves[13][64]) {
	/* transposition table */
	if (e && (*m & 0xFFF) == transposition_move(e))
		return (((uint64_t)1 << 32) - 1);

	/* attack */
	if (pos->mailbox[move_to(m)])
		return mvv_lva(pos->mailbox[move_from(m)], pos->mailbox[move_to(m)]);

	/* promotions */
	if (move_flag(m) == 2)
		return ((uint64_t)1 << 31) - 4 + move_promote(m);

	/* killer */
	if (killer_moves) {
		if (killer_moves[depth][0] == *m)
			return ((uint64_t)1 << 31) - 5;
		if (killer_moves[depth][1] == *m)
			return ((uint64_t)1 << 31) - 6;
	}

	/* history */
	if (history_moves)
		return history_moves[pos->mailbox[move_from(m)]][move_to(m)];
	return 0;
}

/* 1. tt move
 * 2. mvv lva
 * 3. promotions
 * 4. killer
 * 5. history
 */
void evaluate_moves(struct position *pos, move *move_list, uint8_t depth, struct transposition *e, move killer_moves[][2], uint64_t history_moves[13][64]) {
	uint32_t evaluation_list[256];
	int i;
	for (i = 0; move_list[i]; i++)
		evaluation_list[i] = evaluate_move(pos, move_list + i, depth, e, killer_moves, history_moves);

	for (int j = 0; j < i; j++) {
		for (int k = j + 1; k < i; k++) {
			if (evaluation_list[j] < evaluation_list[k]) {
				uint32_t t = evaluation_list[j];
				evaluation_list[j] = evaluation_list[k];
				evaluation_list[k] = t;

				move m = move_list[j];
				move_list[j] = move_list[k];
				move_list[k] = m;
			}
		}
	}
}

int pawn_structure(struct position *pos) {
	int eval = 0, i;
	uint64_t t;

	/* doubled pawns */
	for (i = 0; i < 8; i++) {
		if ((t = pos->white_pieces[pawn] & file(i)))
			if (popcount(t) > 1)
				eval -= 70;
		if ((t = pos->black_pieces[pawn] & file(i)))
			if (popcount(t) > 1)
				eval += 70;
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

	evaluate_moves(pos, move_list, 0, NULL, NULL, NULL);
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

int16_t evaluate_recursive(struct position *pos, uint8_t depth, int16_t alpha, int16_t beta, clock_t clock_stop, move killer_moves[][2], uint64_t history_moves[13][64]) {
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

	move move_list[256];
	generate_all(pos, move_list);

	if (!move_list[0]) {
		uint64_t checkers = generate_checkers(pos);
		if (!checkers)
			return 0;
		return -0x7F00;
	}

	evaluate_moves(pos, move_list, depth, e, killer_moves, history_moves);

	if (pos->halfmove >= 100)
		return 0;
	
	if (e)
		transposition_set_open(e);

	uint16_t m = 0;
	for (move *ptr = move_list; *ptr; ptr++) {
		do_move(pos, ptr);
		/* -beta - 1 to open the window and search for mate in n */
		evaluation = -evaluate_recursive(pos, depth - 1, -beta - 1, -alpha, clock_stop, killer_moves, history_moves);
		evaluation -= (evaluation > 0x4000);
		undo_move(pos, ptr);
		if (evaluation >= beta) {
			/* quiet */
			if (!pos->mailbox[move_to(ptr)])
				store_killer_move(ptr, depth, killer_moves);
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
			m = *ptr;
		}
	}
	if (e)
		transposition_set_closed(e);
	/* type pv or all */
	attempt_store(pos, alpha, depth, m ? 0 : 2, m);
	return alpha;
}

int16_t evaluate(struct position *pos, uint8_t depth, move *m, int verbose, int max_duration, struct history *history) {
	int16_t evaluation = 0, last_evaluation;
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
				printf("\33[2K[%i/%i] %cm\n", depth, depth,
						pos->turn ? '-' : '+');
			else
				printf("\33[2K[%i/%i] s\n", depth, depth);
		}
		return evaluation;
	}

	if (depth == 0) {
		evaluation = count_position(pos);
		if (!pos->turn)
			evaluation = -evaluation;
		if (verbose)
			printf("\33[2K[0/0] %+.2f\n", (double)evaluation / 100);
		return evaluation;
	}

	clock_t clock_stop;
	if (max_duration >= 0)
		clock_stop = clock() + CLOCKS_PER_SEC * max_duration;
	else
		clock_stop = 0;

	move killer_moves[depth][2];
	uint64_t history_moves[13][64];
	memset(killer_moves, 0, sizeof(killer_moves));
	memset(history_moves, 0, sizeof(history_moves));

	move last_move = 0, best_move;
	char str[8];
	int i;
	int16_t alpha, beta;
	for (int d = 1; d <= depth; d++) {
		nodes = 0;
		alpha = -0x7F00;
		beta = 0x7F00;
		for (i = 0; move_list[i]; i++) {
			do_move(pos, move_list + i);
			last_evaluation = -evaluate_recursive(pos, d - 1, -beta, -alpha, clock_stop, killer_moves, history_moves);
			last_evaluation -= (last_evaluation > 0x4000);
			if (is_threefold(pos, history))
				last_evaluation = 0;
			undo_move(pos, move_list + i);
			if (last_evaluation > alpha)
				alpha = last_evaluation, last_move = move_list[i];
		}
		if (!interrupt)
			evaluation = pos->turn ? alpha : -alpha, best_move = last_move;
		else
			d--;
		if (verbose) {
			if (evaluation < -0x4000)
				printf("\r\33[2K[%i/%i] -m%i ", d, depth, 0x7F00 + evaluation);
			else if (evaluation > 0x4000)
				printf("\r\33[2K[%i/%i] +m%i ", d, depth, 0x7F00 - evaluation);
			else
				printf("\r\33[2K[%i/%i] %+.2f ", d, depth, (double)evaluation / 100);
			printf("%s %i\r", move_str_pgn(str, pos, &best_move), nodes);
			fflush(stdout);
		}
		if (!interrupt && m)
			*m = best_move;
		if (interrupt)
			break;
		/* stop searching if mate is found */
		if (evaluation < -0x4000 && 2 * (0x7F00 + evaluation) + pos->turn - 1 <= d)
			break;
		if (evaluation > 0x4000 && 2 * (0x7F00 - evaluation) - pos->turn <= d)
			break;
	}

	if (verbose)
		printf("\n");
	return evaluation;
}
