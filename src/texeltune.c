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
#include <fcntl.h>
#include <unistd.h>
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

#define BATCH_SIZE (8192)

#define PARAMETER(x, y, z) { x, y, z, NULL, NULL, MACRO_NAME(x) }

struct parameter {
	mevalue *ptr;
	size_t size;
	int is_mevalue;
	double *value;
	double *partial;
	char *name;
};

struct parameter parameters[] = {
	PARAMETER(&piece_value[0], 5, 1),
	PARAMETER(&white_psqtable[0][8], 48, 1),
	PARAMETER(&white_psqtable[1][0], 32, 1),
	PARAMETER(&white_psqtable[2][0], 32, 1),
	PARAMETER(&white_psqtable[3][0], 32, 1),
	PARAMETER(&white_psqtable[4][0], 32, 1),
	PARAMETER(&white_psqtable[5][0], 32, 1),

	PARAMETER(&mobility_bonus[0][0],  9, 1),
	PARAMETER(&mobility_bonus[1][0], 14, 1),
	PARAMETER(&mobility_bonus[2][0], 15, 1),
	PARAMETER(&mobility_bonus[3][0], 28, 1),

	PARAMETER(&pawn_shelter[0][0], 7, 1),
	PARAMETER(&pawn_shelter[1][0], 7, 1),
	PARAMETER(&pawn_shelter[2][0], 7, 1),
	PARAMETER(&pawn_shelter[3][0], 7, 1),

	PARAMETER(&unblocked_storm[0][0], 7, 1),
	PARAMETER(&unblocked_storm[1][0], 7, 1),
	PARAMETER(&unblocked_storm[2][0], 7, 1),
	PARAMETER(&unblocked_storm[3][0], 7, 1),

	PARAMETER(&blocked_storm[0], 7, 1),

	PARAMETER(&king_on_open_file, 1, 1),
	PARAMETER(&outpost_bonus, 1, 1),
	PARAMETER(&outpost_attack, 1, 1),
	PARAMETER(&minor_behind_pawn, 1, 1),
	PARAMETER(&knight_far_from_king, 1, 1),
	PARAMETER(&bishop_far_from_king, 1, 1),
	PARAMETER(&bishop_pair, 1, 1),
	PARAMETER(&pawn_on_bishop_square, 1, 1),
	PARAMETER(&rook_on_open_file, 1, 1),
	PARAMETER(&blocked_rook, 1, 1),
	PARAMETER(&undeveloped_piece, 1, 1),
	PARAMETER(&defended_minor, 1, 1),
	PARAMETER(&side_to_move_bonus, 1, 1),

	PARAMETER(&backward_pawn, 1, 1),
	PARAMETER(&supported_pawn, 1, 1),
	PARAMETER(&passed_pawn, 1, 1),
	PARAMETER(&passed_file, 1, 1),
	PARAMETER(&isolated_pawn, 1, 1),
	PARAMETER(&doubled_pawn, 1, 1),
	PARAMETER(&phalanx_pawn, 1, 1),

	PARAMETER(&weak_squares_danger, 1, 0),
	PARAMETER(&enemy_no_queen_bonus, 1, 0),
	PARAMETER(&knight_king_attack_danger, 1, 0),
	PARAMETER(&bishop_king_attack_danger, 1, 0),
	PARAMETER(&rook_king_attack_danger, 1, 0),
	PARAMETER(&queen_king_attack_danger, 1, 0),
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

void parameters_print() {
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
	for (size_t j = 7; j < SIZE(parameters); j++) {
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

void parameters_init() {
	for (size_t i = 0; i < SIZE(parameters); i++) {
		parameters[i].value = malloc(2 * parameters[i].size * sizeof(double));
		parameters[i].partial = malloc(2 * parameters[i].size * sizeof(double));
		
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

double K = 1;

size_t total = 0;

const size_t size = sizeof(struct compressedposition) + sizeof(float);

size_t file_total(int fd) {
	lseek(fd, 0, SEEK_SET);
	char tmp[size];
	while (read(fd, tmp, size))
		total++;
	return total;
}

double sigmoid(int s) {
	return 1 / (1 + pow(10, -K * s / 400));
}

double E(int fd) {
	lseek(fd, 0, SEEK_SET);
	struct position pos[1];
	struct searchinfo si[1];
	float result;
	double ret = 0;
	tables_init();

	while (read(fd, pos, sizeof(struct compressedposition))) {
		if (read(fd, &result, sizeof(float)) == -1)
			printf("READ ERROR\n");
		int16_t q = (2 * pos->turn - 1) * quiescence(pos, -VALUE_MATE, VALUE_MATE, si);
		double s = sigmoid(q);
		ret += (result - s) * (result - s);
	}
	return ret / total;
}

void parameter_step(double value[2], double partial[2], int is_mevalue) {
	if (is_mevalue) {
		value[mg] -= 25000 * partial[mg];
		value[eg] -= 25000 * partial[eg];
	}
	else {
		value[0]  -= 25000 * partial[0];
	}
}

void update_value(mevalue *ptr, double value[2], int is_mevalue) {
	if (is_mevalue)
		*ptr = S((int16_t)value[mg], (int16_t)value[eg]);
	else
		*ptr = (int)value[0];
}

void step() {
	for (size_t i = 0; i < SIZE(parameters); i++) {
		for (size_t j = 0; j < parameters[i].size; j++) {
			parameter_step(&parameters[i].value[2 * j], &parameters[i].partial[2 * j], parameters[i].is_mevalue);
			update_value(&parameters[i].ptr[j], &parameters[i].value[2 * j], parameters[i].is_mevalue);
		}
	}
}

double sum_positions(struct position pos[BATCH_SIZE], float result[BATCH_SIZE], struct searchinfo *si) {
	tables_init();
	double ret = 0;
	for (size_t i = 0; i < BATCH_SIZE; i++) {
		int16_t q = (2 * pos[i].turn - 1) * quiescence(&pos[i], -VALUE_MATE, VALUE_MATE, si);
		double s = sigmoid(q);
		ret += (result[i] - s) * (result[i] - s);
	}
	return ret / BATCH_SIZE;
}

double Es(int fd) {
	struct position pos[1];
	struct searchinfo si[1] = { 0 };
	float result;
	double ret = 0;
	tables_init();

	for (size_t i = 0; i < BATCH_SIZE; i++) {
		size_t offset = (rand() % total) * size;
		lseek(fd, offset, SEEK_SET);
		if (read(fd, pos, sizeof(struct compressedposition)) == -1)
			printf("READ ERROR\n");
		if (read(fd, &result, sizeof(float)) == -1)
			printf("READ ERROR\n");
		int16_t q = (2 * pos->turn - 1) * quiescence(pos, -VALUE_MATE, VALUE_MATE, si);
		double s = sigmoid(q);
		ret += (result - s) * (result - s);
	}
	return ret / BATCH_SIZE;
}

void dEdxs(int fd) {
	struct searchinfo si = { 0 };
	struct position *pos = malloc(BATCH_SIZE * sizeof(*pos));
	float *result = malloc(BATCH_SIZE * sizeof(*result));

	for (size_t i = 0; i < BATCH_SIZE; i++) {
		size_t offset = (rand() % total) * size;
		lseek(fd, offset, SEEK_SET);
		if (read(fd, &pos[i], sizeof(struct compressedposition)) == -1)
			printf("READ ERROR\n");
		if (read(fd, &result[i], sizeof(float)) == -1)
			printf("READ ERROR\n");
	}
	for (size_t j = 0; j < SIZE(parameters); j++) {
		for (size_t k = 0; k < parameters[j].size; k++) {
			if (parameters[j].is_mevalue) {
				parameters[j].ptr[k] += S(1, 0);
				parameters[j].partial[2 * k + mg] = sum_positions(pos, result, &si);
				parameters[j].ptr[k] -= S(2, 0);
				parameters[j].partial[2 * k + mg] -= sum_positions(pos, result, &si);
				parameters[j].ptr[k] += S(1, 0);
				parameters[j].partial[2 * k + mg] /= 2;

				parameters[j].ptr[k] += S(0, 1);
				parameters[j].partial[2 * k + eg] = sum_positions(pos, result, &si);
				parameters[j].ptr[k] -= S(0, 2);
				parameters[j].partial[2 * k + eg] -= sum_positions(pos, result, &si);
				parameters[j].ptr[k] += S(0, 1);
				parameters[j].partial[2 * k + eg] /= 2;
			}
			else {
				parameters[j].ptr[k] += 1;
				parameters[j].partial[2 * k] = sum_positions(pos, result, &si);
				parameters[j].ptr[k] -= 2;
				parameters[j].partial[2 * k] -= sum_positions(pos, result, &si);
				parameters[j].ptr[k] += 1;
				parameters[j].partial[2 * k] /= 2;
			}
		}
	}
	free(pos);
	free(result);
}

void learn(int fd) {
	while (1) {
		dEdxs(fd);
		step();
		parameters_print();
	}
}

int main() {
	util_init();
	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	search_init();
	moveorder_init();
	position_init();
	pawn_init();
	srand(time(NULL));

	int fd = open("texel.bin", O_RDONLY);

	total = file_total(fd);
	printf("%ld\n", total);

	parameters_init();

	learn(fd);

	close(fd);
}
