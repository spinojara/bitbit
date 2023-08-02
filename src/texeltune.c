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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"
#include "evaluate.h"
#include "move.h"
#include "tables.h"
#include "position.h"
#include "move.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "bitboard.h"
#include "search.h"
#include "moveorder.h"
#include "pawn.h"
#include "option.h"
#include "nnue.h"

#define BATCH_SIZE (8192)

#define PARAMETER(x, y, z, w) { x, y, z, NULL, NULL, NULL, NULL, w, MACRO_NAME(x) }

const double beta_1 = 0.9;
const double beta_2 = 0.999;
const double epsilon = 1e-8;
const double learning_rate = 5e-2;
int t = 0;

struct position pos;

static inline int flip(int square) {
	return square ^ 0x7;
}

void normal_add(mevalue *ptr, mevalue eval, int index) {
	ptr[index] += eval;
}

void piece_value_add(mevalue *ptr, mevalue eval, int index) {
	UNUSED(ptr);
	for (int turn = 0; turn <= 1; turn++) {
		for (int square = 0; square < 64; square++) {
			psqtable[turn][index + 1][square] += eval;
		}
	}
}

void psqtable_pawn_add(mevalue *ptr, mevalue eval, int index) {
	UNUSED(ptr);
	index += 8;
	int x = index % 8;
	int y = index / 8;
	int square = 8 * (7 - y) + x;
	psqtable[white][pawn][orient(white, square)] += eval;
	psqtable[black][pawn][orient(black, square)] += eval;
}

void psqtable_knight_add(mevalue *ptr, mevalue eval, int index) {
	UNUSED(ptr);
	int x = index % 4;
	int y = index / 4;
	int square = 8 * (7 - y) + x;
	int flipped = flip(square);
	psqtable[white][knight][orient(white, square)] += eval;
	psqtable[white][knight][orient(white, flipped)] += eval;
	psqtable[black][knight][orient(black, square)] += eval;
	psqtable[black][knight][orient(black, flipped)] += eval;
}

void psqtable_bishop_add(mevalue *ptr, mevalue eval, int index) {
	UNUSED(ptr);
	int x = index % 4;
	int y = index / 4;
	int square = 8 * (7 - y) + x;
	int flipped = flip(square);
	psqtable[white][bishop][orient(white, square)] += eval;
	psqtable[white][bishop][orient(white, flipped)] += eval;
	psqtable[black][bishop][orient(black, square)] += eval;
	psqtable[black][bishop][orient(black, flipped)] += eval;
}

void psqtable_rook_add(mevalue *ptr, mevalue eval, int index) {
	UNUSED(ptr);
	int x = index % 4;
	int y = index / 4;
	int square = 8 * (7 - y) + x;
	int flipped = flip(square);
	psqtable[white][rook][orient(white, square)] += eval;
	psqtable[white][rook][orient(white, flipped)] += eval;
	psqtable[black][rook][orient(black, square)] += eval;
	psqtable[black][rook][orient(black, flipped)] += eval;
}

void psqtable_queen_add(mevalue *ptr, mevalue eval, int index) {
	UNUSED(ptr);
	int x = index % 4;
	int y = index / 4;
	int square = 8 * (7 - y) + x;
	int flipped = flip(square);
	psqtable[white][queen][orient(white, square)] += eval;
	psqtable[white][queen][orient(white, flipped)] += eval;
	psqtable[black][queen][orient(black, square)] += eval;
	psqtable[black][queen][orient(black, flipped)] += eval;
}

void psqtable_king_add(mevalue *ptr, mevalue eval, int index) {
	UNUSED(ptr);
	int x = index % 4;
	int y = index / 4;
	int square = 8 * (7 - y) + x;
	int flipped = flip(square);
	psqtable[white][king][orient(white, square)] += eval;
	psqtable[white][king][orient(white, flipped)] += eval;
	psqtable[black][king][orient(black, square)] += eval;
	psqtable[black][king][orient(black, flipped)] += eval;
}

struct parameter {
	mevalue *ptr;
	size_t size;
	int is_mevalue;
	double *value;
	double *grad;
	double *m;
	double *v;
	void (*add)(mevalue *, mevalue, int);
	char *name;
};

struct parameter parameters[] = {
	PARAMETER(&piece_value[0], 5, 1, piece_value_add),
	PARAMETER(&white_psqtable[0][8], 48, 1, psqtable_pawn_add),
	PARAMETER(&white_psqtable[1][0], 32, 1, psqtable_knight_add),
	PARAMETER(&white_psqtable[2][0], 32, 1, psqtable_bishop_add),
	PARAMETER(&white_psqtable[3][0], 32, 1, psqtable_rook_add),
	PARAMETER(&white_psqtable[4][0], 32, 1, psqtable_queen_add),
	PARAMETER(&white_psqtable[5][0], 32, 1, psqtable_king_add),

	PARAMETER(&mobility_bonus[0][0],  9, 1, normal_add),
	PARAMETER(&mobility_bonus[1][0], 14, 1, normal_add),
	PARAMETER(&mobility_bonus[2][0], 15, 1, normal_add),
	PARAMETER(&mobility_bonus[3][0], 28, 1, normal_add),

	PARAMETER(&pawn_shelter[0][0], 7, 1, normal_add),
	PARAMETER(&pawn_shelter[1][0], 7, 1, normal_add),
	PARAMETER(&pawn_shelter[2][0], 7, 1, normal_add),
	PARAMETER(&pawn_shelter[3][0], 7, 1, normal_add),

	PARAMETER(&unblocked_storm[0][0], 7, 1, normal_add),
	PARAMETER(&unblocked_storm[1][0], 7, 1, normal_add),
	PARAMETER(&unblocked_storm[2][0], 7, 1, normal_add),
	PARAMETER(&unblocked_storm[3][0], 7, 1, normal_add),

	PARAMETER(&blocked_storm[0], 7, 1, normal_add),

	PARAMETER(&king_on_open_file, 1, 1, normal_add),
	PARAMETER(&outpost_bonus, 1, 1, normal_add),
	PARAMETER(&outpost_attack, 1, 1, normal_add),
	PARAMETER(&minor_behind_pawn, 1, 1, normal_add),
	PARAMETER(&knight_far_from_king, 1, 1, normal_add),
	PARAMETER(&bishop_far_from_king, 1, 1, normal_add),
	PARAMETER(&bishop_pair, 1, 1, normal_add),
	PARAMETER(&pawn_on_bishop_square, 1, 1, normal_add),
	PARAMETER(&rook_on_open_file, 1, 1, normal_add),
	PARAMETER(&blocked_rook, 1, 1, normal_add),
	PARAMETER(&undeveloped_piece, 1, 1, normal_add),
	PARAMETER(&defended_minor, 1, 1, normal_add),

	PARAMETER(&backward_pawn, 1, 1, normal_add),
	PARAMETER(&supported_pawn, 1, 1, normal_add),
	PARAMETER(&passed_pawn, 1, 1, normal_add),
	PARAMETER(&passed_file, 1, 1, normal_add),
	PARAMETER(&isolated_pawn, 1, 1, normal_add),
	PARAMETER(&doubled_pawn, 1, 1, normal_add),
	PARAMETER(&phalanx_pawn, 1, 1, normal_add),

	PARAMETER(&weak_squares_danger, 1, 0, normal_add),
	PARAMETER(&enemy_no_queen_bonus, 1, 0, normal_add),
	PARAMETER(&knight_king_attack_danger, 1, 0, normal_add),
	PARAMETER(&bishop_king_attack_danger, 1, 0, normal_add),
	PARAMETER(&rook_king_attack_danger, 1, 0, normal_add),
	PARAMETER(&queen_king_attack_danger, 1, 0, normal_add),

	PARAMETER(&tempo_bonus, 1, 0, normal_add),
	PARAMETER(&phase_max_material, 1, 0, normal_add),
	PARAMETER(&phase_min_material, 1, 0, normal_add),
};

void mevalue_print(mevalue eval) {
	printf("S(%3d,%3d), ", mevalue_mg(eval), mevalue_eg(eval));;
}

void print_parameter_name(struct parameter *parameter) {
	size_t len = strlen(parameter->name);
	printf("\n%s", parameter->name);
	for (size_t i = 0; i < 27 - len; i++)
		printf(" ");
}

void parameters_print(void) {
	for (int i = 0; i < 5; i++) {
		printf("S(%4d,%4d), ", mevalue_mg(piece_value[i]), mevalue_eg(piece_value[i]));
	}
	printf("\n");
	for (int i = 8; i < 56; i++) {
		if (i % 8 == 0)
			printf("\n");
		mevalue_print(white_psqtable[0][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		mevalue_print(white_psqtable[1][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		mevalue_print(white_psqtable[2][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		mevalue_print(white_psqtable[3][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		mevalue_print(white_psqtable[4][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		mevalue_print(white_psqtable[5][i]);
	}
	printf("\n");
	for (size_t j = 0; j < SIZE(parameters); j++) {
		print_parameter_name(&parameters[j]);
		for (size_t i = 0; i < parameters[j].size; i++) {
			if (parameters[j].is_mevalue)
				mevalue_print(parameters[j].ptr[i]);
			else
				printf("%d", parameters[j].ptr[i]);
		}
	}
	printf("\n");
}

void parameters_init(void) {
	for (size_t i = 0; i < SIZE(parameters); i++) {
		size_t bytes = 2 * parameters[i].size * sizeof(double);
		parameters[i].value = malloc(bytes);
		parameters[i].grad = malloc(bytes);
		parameters[i].m = malloc(bytes);
		memset(parameters[i].m, 0, bytes);
		parameters[i].v = malloc(bytes);
		memset(parameters[i].v, 0, bytes);
		
		for (size_t j = 0; j < parameters[i].size; j++) {
			if (parameters[i].is_mevalue) {
				parameters[i].value[2 * j + mg] = mevalue_mg(parameters[i].ptr[j]);
				parameters[i].value[2 * j + eg] = mevalue_eg(parameters[i].ptr[j]);
			}
			else {
				parameters[i].value[2 * j] = parameters[i].ptr[j];
			}
		}
	}
}

double sigmoid(int s) {
	return 1.0 / (1.0 + pow(10, -(double)s / 400));
}

void update_value(mevalue *ptr, double value[2], int is_mevalue) {
	if (is_mevalue)
		*ptr = S((int16_t)round(value[mg]), (int16_t)round(value[eg]));
	else
		*ptr = round(value[0]);
}

void parameter_step(double value[2], double m[2], double v[2], int is_mevalue) {
	for (int i = 0; i <= is_mevalue; i++) {
		double m_hat = m[i] / (1 - pow(beta_1, t));
		double v_hat = v[i] / (1 - pow(beta_2, t));
		value[i] -= learning_rate * m_hat / (sqrt(v_hat) + epsilon);
	}
}

void update_mv(double m[2], double v[2], double grad[2]) {
	for (int i = 0; i < 2; i++) {
		m[i] = beta_1 * m[i] + (1 - beta_1) * grad[i];
		v[i] = beta_2 * v[i] + (1 - beta_2) * grad[i] * grad[i];
	}
}

void step(void) {
	t++;
	for (size_t i = 0; i < SIZE(parameters); i++) {
		for (size_t j = 0; j < parameters[i].size; j++) {
			update_mv(&parameters[i].m[2 * j], &parameters[i].v[2 * j], &parameters[i].grad[2 * j]);
			parameter_step(&parameters[i].value[2 * j], &parameters[i].m[2 * j], &parameters[i].v[2 * j], parameters[i].is_mevalue);
			update_value(&parameters[i].ptr[j], &parameters[i].value[2 * j], parameters[i].is_mevalue);
		}
	}
	tables_init();
}

void zero_grad(void) {
	for (size_t i = 0; i < SIZE(parameters); i++) {
		size_t bytes = 2 * parameters[i].size * sizeof(double);
		memset(parameters[i].grad, 0, bytes);
	}
}

int grad(FILE *f) {
	struct searchinfo si = { 0 };
	int16_t q, q0;
	double s, s0;
	double result;

	size_t actual_size = 0;
	while (actual_size < BATCH_SIZE) {
		move m = 0;
		fread(&m, 2, 1, f);
		if (m)
			do_move(&pos, &m);
		else
			fread(&pos, sizeof(struct partialposition), 1, f);

		int16_t eval = VALUE_NONE;
		fread(&eval, 2, 1, f);
		if (feof(f))
			break;

		int skip = (eval == VALUE_NONE) || gbernoulli(0.9);
		if (skip)
			continue;

		result = sigmoid(eval);
		q0 = quiescence(&pos, 0, -VALUE_MATE, VALUE_MATE, &si);
		s0 = sigmoid(q0);

		for (size_t i = 0; i < SIZE(parameters); i++) {
			for (size_t j = 0; j < parameters[i].size; j++) {
				if (parameters[i].is_mevalue) {
					/* mg */
					parameters[i].add(parameters[i].ptr, S(1, 0), j);
					q = quiescence(&pos, 0, -VALUE_MATE, VALUE_MATE, &si);
					s = sigmoid(q);
					parameters[i].grad[2 * j + mg] += (result - s) * (result - s) - (result - s0) * (result - s0);

					/* eg */
					parameters[i].add(parameters[i].ptr, S(-1, 1), j);
					q = quiescence(&pos, 0, -VALUE_MATE, VALUE_MATE, &si);
					s = sigmoid(q);
					parameters[i].grad[2 * j + eg] += (result - s) * (result - s) - (result - s0) * (result - s0);

					/* reset */
					parameters[i].add(parameters[i].ptr, S(0, -1), j);
				}
				else {
					parameters[i].add(parameters[i].ptr, 1, j);
					q = quiescence(&pos, 0, -VALUE_MATE, VALUE_MATE, &si);
					s = sigmoid(q);
					parameters[i].grad[2 * j] += (result - s) * (result - s) - (result - s0) * (result - s0);

					/* reset */
					parameters[i].add(parameters[i].ptr, -1, j);
				}
			}
		}
		actual_size++;
	}
	if (actual_size == 0)
		return 1;

	for (size_t i = 0; i < SIZE(parameters); i++)
		for (size_t j = 0; j < parameters[i].size; j++)
			for (int k = 0; k <= parameters[i].is_mevalue; k++)
				parameters[i].grad[2 * j + k] /= actual_size;

	return 0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("provide a filename\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		printf("could not open %s\n", argv[1]);
		return 2;
	}

	option_nnue = 0;
	option_transposition = 0;
	option_pawn = 0;
	option_history = 0;

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	tables_init();
	search_init();
	moveorder_init();
	position_init();
	pawn_init();
	srand(time(NULL));
	parameters_init();

	while (1) {
		parameters_print();
		zero_grad();
		if (grad(f)) {
			fseek(f, 0, SEEK_SET);
			continue;
		}
		step();
	}

	fclose(f);
}
