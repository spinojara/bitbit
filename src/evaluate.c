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

#include "move_gen.h"
#include "move.h"
#include "util.h"
#include "hash_table.h"
#include "init.h"

int eval_table[13][64];

int piece_value[6] = { 100, 300, 315, 500, 900, 0 };

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
					           piece_value[i - 1];
			}
			else
				eval_table[i][j] = -white_side_eval_table[i - 7][j] -
						   piece_value[i - 7];
			init_status("populating evaluation lookup table");
		}
	}
}

int count_position(struct position *pos) {
	int r = mate(pos);
	int eval = 0;
	for (int i = 0; i < 64; i++) {
		eval += eval_table[pos->mailbox[i]][i];
	}
	return eval;
}

int16_t evaluate_recursive(struct position *pos, uint8_t depth, int alpha, int beta) {
	if (depth <= 0)
		return count_position(pos);

	int16_t evaluation;
	move move_list[256];
	generate_all(pos, move_list);

	if (pos->turn) {
		evaluation = -0x8000;
		for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
			do_move(pos, move_ptr);
			evaluation = MAX(evaluation, evaluate_recursive(pos, depth - 1, alpha, beta));
			undo_move(pos, move_ptr);
			alpha = MAX(evaluation, alpha);
			if (beta < alpha)
				break;
		}
	}
	else {
		evaluation = 0x7FFF;
		for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
			do_move(pos, move_ptr);
			evaluation = MIN(evaluation, evaluate_recursive(pos, depth - 1, alpha, beta));
			undo_move(pos, move_ptr);
			beta = MIN(evaluation, beta);
			if (beta < alpha)
				break;
		}
	}
	return evaluation;
}

int16_t evaluate(struct position *pos, uint8_t depth, move *m, int verbose) {
	if (depth <= 0)
		return count_position(pos);

	int16_t evaluation;
	int16_t evaluation_list[256];
	move move_list[256];
	generate_all(pos, move_list);

	int i;
	int16_t alpha, beta;
	for (int d = 1; d <= depth; d++) {
		alpha = -0x8000;
		beta = 0x7FFF;
		if (pos->turn) {
			evaluation = -0x8000;
			for (i = 0; move_list[i]; i++) {
				do_move(pos, move_list + i);
				evaluation_list[i] = evaluate_recursive(pos, d - 1, alpha, beta);
				evaluation = MAX(evaluation, evaluation_list[i]);
				undo_move(pos, move_list + i);
				alpha = MAX(evaluation, alpha);
				if (beta < alpha) {
					i++;
					break;
				}
			}
			merge_sort(move_list, evaluation_list, 0, i - 1, 0);
		}
		else {
			evaluation = 0x7FFF;
			for (i = 0; move_list[i]; i++) {
				do_move(pos, move_list + i);
				evaluation_list[i] = evaluate_recursive(pos, d - 1, alpha, beta);
				evaluation = MIN(evaluation, evaluation_list[i]);
				undo_move(pos, move_list + i);
				beta = MIN(evaluation, beta);
				if (beta < alpha) {
					i++;
					break;
				}
			}
			merge_sort(move_list, evaluation_list, 0, i - 1, 1);
		}
		if (verbose) {
			printf("[%i/%i] %.2f ", d, depth, (double)evaluation / 100);
			print_move(move_list);
			printf("       \r");
			fflush(stdout);
		}
		if (m)
			*m = *move_list;
	}

	if (verbose)
		printf("\n");
	return evaluation;
}

int16_t evaluate_recursive_hash(struct position *pos, uint8_t depth, int16_t alpha, int16_t beta) {
	if (depth <= 0)
		return count_position(pos);

	if (table_entry(pos)->depth >= depth && table_entry(pos)->zobrist_key == pos->zobrist_key)
		return table_entry(pos)->evaluation;

	int16_t evaluation;
	move move_list[256];
	generate_all(pos, move_list);

	if (pos->turn) {
		evaluation = -0x8000;
		for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
			do_move_zobrist(pos, move_ptr);
			evaluation = MAX(evaluation, evaluate_recursive_hash(pos, depth - 1, alpha, beta));
			undo_move_zobrist(pos, move_ptr);
			alpha = MAX(evaluation, alpha);
			if (beta < alpha)
				break;
		}
	}
	else {
		evaluation = 0x7FFF;
		for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
			do_move_zobrist(pos, move_ptr);
			evaluation = MIN(evaluation, evaluate_recursive_hash(pos, depth - 1, alpha, beta));
			undo_move_zobrist(pos, move_ptr);
			beta = MIN(evaluation, beta);
			if (beta < alpha)
				break;
		}
	}
	store_table_entry(pos, evaluation, depth);
	return evaluation;
}

int16_t evaluate_hash(struct position *pos, uint8_t depth, move *m, int verbose) {
	if (depth <= 0)
		return count_position(pos);

	int16_t evaluation;
	int16_t evaluation_list[256];
	move move_list[256];
	generate_all(pos, move_list);

	int i;
	int16_t alpha, beta;
	for (int d = 1; d <= depth; d++) {
		alpha = -0x8000;
		beta = 0x7FFF;
		if (pos->turn) {
			evaluation = -0x8000;
			for (i = 0; move_list[i]; i++) {
				do_move_zobrist(pos, move_list + i);
				evaluation_list[i] = evaluate_recursive_hash(pos, d - 1, alpha, beta);
				evaluation = MAX(evaluation, evaluation_list[i]);
				undo_move_zobrist(pos, move_list + i);
				alpha = MAX(evaluation, alpha);
				if (beta < alpha) {
					i++;
					break;
				}
			}
			merge_sort(move_list, evaluation_list, 0, i - 1, 0);
		}
		else {
			evaluation = 0x7FFF;
			for (i = 0; move_list[i]; i++) {
				do_move_zobrist(pos, move_list + i);
				evaluation_list[i] = evaluate_recursive_hash(pos, d - 1, alpha, beta);
				evaluation = MIN(evaluation, evaluation_list[i]);
				undo_move_zobrist(pos, move_list + i);
				beta = MIN(evaluation, beta);
				if (beta < alpha) {
					i++;
					break;
				}
			}
			merge_sort(move_list, evaluation_list, 0, i - 1, 1);

		}
		if (verbose) {
			printf("\r[%i/%i] %.2f ", d, depth, (double)evaluation / 100);
			print_move(move_list);
			printf("       \r");
			fflush(stdout);
		}
		store_table_entry(pos, evaluation, d);
		if (m)
			*m = *move_list;
	}

	if (verbose)
		printf("\n");
	return evaluation;
}
