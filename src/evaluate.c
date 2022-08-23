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

#include "move_gen.h"
#include "move.h"
#include "util.h"
#include "transposition_table.h"
#include "init.h"
#include "position.h"
#include "interrupt.h"

unsigned int nodes = 0;

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

int16_t count_position(struct position *pos) {
	int eval, i;
	for (i = 0, eval = 0; i < 64; i++) {
		eval += eval_table[pos->mailbox[i]][i];
	}
	return eval;
}

int16_t quiescence(struct position *pos, int16_t alpha, int16_t beta, clock_t clock_stop) {
	if (interrupt)
		return 0;
	if (nodes % 4096 == 0)
		if (clock_stop && clock() > clock_stop)
			interrupt = 1;
	nodes++;

	int16_t evaluation, t;
	evaluation = mate(pos);

	/* stalemate */
	if (evaluation == 1)
		return 0;
	/* checkmate */
	if (evaluation == 2)
		return pos->turn ? -0x8000 : 0x7FFF;

	move move_list[256];
	int16_t evaluation_list[256];
	generate_quiescence(pos, move_list);

	evaluation = count_position(pos);
	if (!move_list[0])
		return evaluation;

	if (pos->turn) {
		alpha = MAX(evaluation, alpha);
		if (beta < alpha)
			return evaluation;
		for (t = 0; move_list[t]; t++) {
			evaluation_list[t] = eval_table[pos->mailbox[move_to(move_list + t)]][move_to(move_list + t)] +
				eval_table[pos->mailbox[move_from(move_list + t)]][move_from(move_list + t)];
		}
		merge_sort(move_list, evaluation_list, 0, t - 1, 1);
		evaluation = -0x8000;
		for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
			do_move(pos, move_ptr);
			t = quiescence(pos, alpha, beta, clock_stop);
			undo_move(pos, move_ptr);
			if (t < -0x4000)
				t++;
			else if (t > 0x4000)
				t--;
			evaluation = MAX(evaluation, t);
			alpha = MAX(evaluation, alpha);
			if (beta < alpha)
				break;
		}
	}
	else {
		beta = MIN(evaluation, beta);
		if (beta < alpha)
			return evaluation;
		for (t = 0; move_list[t]; t++) {
			evaluation_list[t] = eval_table[pos->mailbox[move_to(move_list + t)]][move_to(move_list + t)] +
				eval_table[pos->mailbox[move_from(move_list + t)]][move_from(move_list + t)];
		}
		merge_sort(move_list, evaluation_list, 0, t - 1, 0);
		evaluation = 0x7FFF;
		for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
			do_move(pos, move_ptr);
			t = quiescence(pos, alpha, beta, clock_stop);
			undo_move(pos, move_ptr);
			if (t < -0x4000)
				t++;
			else if (t > 0x4000)
				t--;
			evaluation = MIN(evaluation, t),
			beta = MIN(evaluation, beta);
			if (beta < alpha)
				break;
		}
	}
	return evaluation;
}

int16_t evaluate_recursive(struct position *pos, uint8_t depth, int16_t alpha, int16_t beta, clock_t clock_stop) {
	if (interrupt)
		return 0;
	if (nodes % 4096 == 0)
		if (clock_stop && clock() > clock_stop)
			interrupt = 1;
	nodes++;

	struct transposition *e = attempt_get(pos);
	if (e && transposition_open(e))
		return 0;
	if (e && transposition_depth(e) >= depth && transposition_type(e) == 0)
		return transposition_evaluation(e);
	if (depth <= 0)
		return quiescence(pos, alpha, beta, clock_stop);

	int16_t evaluation, evaluation_list[256], t;
	move move_list[256];
	generate_all(pos, move_list);

	if (!move_list[0]) {
		uint64_t checkers = generate_checkers(pos);
		if (!checkers)
			return 0;
		return pos->turn ? -0x8000 : 0x7FFF;
	}

	if (pos->halfmove >= 100)
		return 0;

	for (t = 0; move_list[t]; t++) {
		if (pos->mailbox[move_to(move_list + t)])
			evaluation_list[t] = eval_table[pos->mailbox[move_to(move_list + t)]][move_to(move_list + t)] * 10 +
				eval_table[pos->mailbox[move_from(move_list + t)]][move_from(move_list + t)] * 10;
		else
			evaluation_list[t] = eval_table[pos->mailbox[move_from(move_list + t)]][move_from(move_list + t)];
	}
	merge_sort(move_list, evaluation_list, 0, t - 1, pos->turn);
	if (e) {
		reorder_moves(move_list, transposition_move(e));
		transposition_set_open(e);
	}

	/* type all */
	uint8_t type = 2;
	uint16_t m = 0;
	if (pos->turn) {
		evaluation = -0x8000;
		for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
			do_move(pos, move_ptr);
			t = evaluate_recursive(pos, depth - 1, alpha, beta, clock_stop);
			undo_move(pos, move_ptr);
			if (t < -0x4000)
				t++;
			else if (t > 0x4000)
				t--;
			if (evaluation < t) {
				m = *move_ptr & 0xFFF;
				evaluation = t;
				/* type pv */
				type = 0;
			}
			alpha = MAX(evaluation, alpha);
			if (beta < alpha) {
				/* type cut */
				type = 1;
				break;
			}
		}
	}
	else {
		evaluation = 0x7FFF;
		for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
			do_move(pos, move_ptr);
			t = evaluate_recursive(pos, depth - 1, alpha, beta, clock_stop);
			undo_move(pos, move_ptr);
			if (t < -0x4000)
				t++;
			else if (t > 0x4000)
				t--;
			if (evaluation > t) {
				m = *move_ptr & 0xFFF;
				evaluation = t;
				/* type pv */
				type = 0;
			}
			beta = MIN(evaluation, beta);
			if (beta < alpha) {
				/* type cut */
				type = 1;
				break;
			}
		}
	}
	if (e)
		transposition_set_closed(e);
	attempt_store(pos, evaluation, depth, type, m);
	return evaluation;
}

int16_t evaluate(struct position *pos, uint8_t depth, move *m, int verbose, int max_duration, struct history *history) {
	int16_t evaluation, last_evaluation = 0;
	int16_t evaluation_list[256];
	move move_list[256];
	generate_all(pos, move_list);

	if (!move_list[0]) {
		uint64_t checkers = generate_checkers(pos);
		if (!checkers)
			evaluation = 0;
		else 
			evaluation = pos->turn ? -0x8000 : 0x7FFF;
		if (m)
			*m = 0;
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
		if (verbose)
			printf("\33[2K[0/0] %+.2f\n", (double)evaluation / 100);
		if (m)
			*m = 0;
		return evaluation;
	}

	clock_t clock_stop;
	if (max_duration >= 0)
		clock_stop = clock() + CLOCKS_PER_SEC * max_duration;
	else
		clock_stop = 0;

	char str[8];
	int i;
	int16_t alpha, beta;
	for (int d = 1; d <= depth; d++) {
		alpha = -0x8000;
		beta = 0x7FFF;
		if (pos->turn) {
			evaluation = -0x8000;
			for (i = 0; move_list[i]; i++) {
				do_move(pos, move_list + i);
				evaluation_list[i] = evaluate_recursive(pos, d - 1, alpha, beta, clock_stop);
				if (is_threefold(pos, history))
					evaluation_list[i] = 0;
				undo_move(pos, move_list + i);

				if (evaluation_list[i] < -0x4000)
					evaluation_list[i]++;
				else if (evaluation_list[i] > 0x4000)
					evaluation_list[i]--;

				evaluation = MAX(evaluation, evaluation_list[i]);
				alpha = MAX(evaluation, alpha);
				if (beta < alpha) {
					i++;
					break;
				}
			}
			if (!interrupt) {
				last_evaluation = evaluation;
				merge_sort(move_list, evaluation_list, 0, i - 1, 0);
			}
		}
		else {
			evaluation = 0x7FFF;
			for (i = 0; move_list[i]; i++) {
				do_move(pos, move_list + i);
				evaluation_list[i] = evaluate_recursive(pos, d - 1, alpha, beta, clock_stop);
				if (is_threefold(pos, history))
					evaluation_list[i] = 0;
				undo_move(pos, move_list + i);

				if (evaluation_list[i] < -0x4000)
					evaluation_list[i]++;
				else if (evaluation_list[i] > 0x4000)
					evaluation_list[i]--;

				evaluation = MIN(evaluation, evaluation_list[i]);
				beta = MIN(evaluation, beta);
				if (beta < alpha) {
					i++;
					break;
				}
			}
			if (!interrupt) {
				last_evaluation = evaluation;
				merge_sort(move_list, evaluation_list, 0, i - 1, 1);
			}
		}
		if (interrupt)
			d--;
		if (verbose) {
			if (last_evaluation < -0x4000)
				printf("\r\33[2K[%i/%i] -m%i ", d, depth, ((0x8000 + last_evaluation) + 1) / 2);
			else if (last_evaluation > 0x4000)
				printf("\r\33[2K[%i/%i] +m%i ", d, depth, ((0x7FFF - last_evaluation) + 1) / 2);
			else
				printf("\r\33[2K[%i/%i] %+.2f ", d, depth, (double)last_evaluation / 100);
			printf("%s\r", move_str_pgn(str, pos, move_list));
			fflush(stdout);
		}
		if (!interrupt && m)
			*m = *move_list;
		if (interrupt)
			break;
		if (last_evaluation < -0x4000 && (0x8000 + last_evaluation) <= d)
			break;
		if (last_evaluation > 0x4000 && (0x7FFF - last_evaluation) <= d)
			break;
	}

	if (verbose)
		printf("\n");
	return last_evaluation;
}
