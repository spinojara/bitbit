/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022 Isak Ellme0
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

#include "texeltune.h"

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
#include "endgame.h"

struct trace trace;

#define BATCH_SIZE (32)

#define PARAMETER(x, y, z, w, q) { .ptr = x, .size = y, .type = z, .weight_decay = w, .tune = q }

enum {
	TYPE_INT,
	TYPE_MEVALUE,
};

/* enum has to be in the same order as the parameter list. */
enum {
	PARAM_PIECEVALUE,

	PARAM_PSQTPAWN,
	PARAM_PSQTKNIGHT,
	PARAM_PSQTBISHOP,
	PARAM_PSQTROOK,
	PARAM_PSQTQUEEN,
	PARAM_PSQTKING,

	PARAM_MOBILITYKNIGHT,
	PARAM_MOBILITYBISHOP,
	PARAM_MOBILITYROOK,
	PARAM_MOBILITYQUEEN,

	PARAM_PAWNSHELTER,
	PARAM_BLOCKEDSTORM,
	PARAM_UNBLOCKEDSTORM,
	PARAM_UNBLOCKABLESTORM,

	PARAM_KINGONOPENFILE,
	PARAM_KNIGHTOUTPOST,
	PARAM_KNIGHTOUTPOSTATTACK,
	PARAM_BISHOPOUTPOST,
	PARAM_BISHOPOUTPOSTATTACK,
	PARAM_BISHOPLONGDIAGONAL,
	PARAM_KNIGHTBEHINDPAWN,
	PARAM_BISHOPBEHINDPAWN,
	PARAM_DEFENDEDKNIGHT,
	PARAM_DEFENDEDBISHOP,
	PARAM_KNIGHTFARFROMKING,
	PARAM_BISHOPFARFROMKING,
	PARAM_KNIGHTPAIR,
	PARAM_BISHOPPAIR,
	PARAM_ROOKPAIR,
	PARAM_PAWNBLOCKINGBISHOP,
	PARAM_ROOKOPEN,
	PARAM_ROOKSEMI,
	PARAM_ROOKCLOSED,
	PARAM_ROOKBLOCKED,
	PARAM_BADQUEEN,
	PARAM_KINGATTACKPAWN,
	PARAM_KINGDEFENDPAWN,
	PARAM_TEMPOBONUS,

	PARAM_PAWNTHREAT,
	PARAM_PUSHTHREAT,
	PARAM_MINORTHREAT,
	PARAM_ROOKTHREAT,

	PARAM_WEAKSQUARES,
	PARAM_ENEMYNOQUEEN,
	PARAM_KNIGHTATTACK,
	PARAM_BISHOPATTACK,
	PARAM_ROOKATTACK,
	PARAM_QUEENATTACK,
	PARAM_DISCOVERY,
	PARAM_CHECKS,

	PARAM_PHASEMAX,
	PARAM_PHASEMIN,
	PARAM_PHASEKNIGHT,
	PARAM_PHASEBISHOP,
	PARAM_PHASEROOK,
	PARAM_PHASEQUEEN,

	PARAM_SUPPORTEDPAWN,
	PARAM_BACKWARDPAWN,
	PARAM_ISOLATEDPAWN,
	PARAM_DOUBLEDPAWN,
	PARAM_CONNECTEDPAWN,
	PARAM_PASSEDPAWN,
	PARAM_PASSEDBLOCKED,
	PARAM_PASSEDFILE,
	PARAM_DISTANCEUS,
	PARAM_DISTANCETHEM,
};

enum {
	TUNE_YES,
	TUNE_NO,
};

enum {
	WEIGHTDECAY_YES,
	WEIGHTDECAY_NO,
};

const double K = 1.0;
const double beta_1 = 0.9;
const double beta_2 = 0.999;
const double epsilon = 1e-8;
const double alpha = 1e-3;
const double weight_decay = 1e-4;
size_t t = 0;

struct parameter {
	mevalue *ptr;
	size_t size;
	int type;
	int tune;
	int weight_decay;
	double *value;
	double *grad;
	double *m;
	double *v;
};

/* parameter list has to be in the same order as the enum. */
struct parameter parameters[] = {
	PARAMETER(&piece_value[0],            5, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&white_psqtable[0][8],     48, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[1][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[2][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[3][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[4][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[5][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),

	PARAMETER(&mobility[0][0],            9, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&mobility[1][0],           14, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&mobility[2][0],           15, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&mobility[3][0],           28, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),

	PARAMETER(&pawn_shelter[0],          28, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&blocked_storm[0],         28, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&unblocked_storm[0],       28, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&unblockable_storm[0],     28, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&king_on_open_file,         1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_outpost,            1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_outpost_attack,     1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_outpost,            1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_outpost_attack,     1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_long_diagonal,      1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_behind_pawn,        1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_behind_pawn,        1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&defended_knight,           1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&defended_bishop,           1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_far_from_king,      1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_far_from_king,      1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_pair,               1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_pair,               1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_pair,                 1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&pawn_blocking_bishop,      1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_open,                 1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_semi,                 1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_closed,               1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_blocked,              1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bad_queen,                 1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&king_attack_pawn,          1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&king_defend_pawn,          1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&tempo_bonus,               1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&pawn_threat,               1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&push_threat,               1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&minor_threat[0],           6, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_threat[0],            6, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&weak_squares,              1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&enemy_no_queen,            1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_attack,             1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_attack,             1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_attack,               1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&queen_attack,              1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&discovery,                 1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&checks[0],                12, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&phase_max,                 1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_min,                 1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_knight,              1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_bishop,              1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_rook,                1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_queen,               1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&supported_pawn,            1, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&backward_pawn[0],          4, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&isolated_pawn[0],          4, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&doubled_pawn[0],           4, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&connected_pawn[0],         7, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&passed_pawn[0],            7, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&passed_blocked[0],         7, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&passed_file[0],            4, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&distance_us[0],            7, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&distance_them[0],          7, TYPE_MEVALUE, WEIGHTDECAY_NO,  TUNE_YES),
};

void mevalue_print(mevalue eval) {
	printf("S(%3d,%3d), ", mevalue_mg(eval), mevalue_eg(eval));;
}

void parameters_print(void) {
	struct parameter *param;
	for (int i = 0; i < 5; i++) {
		printf("S(%4d,%4d), ", mevalue_mg(piece_value[i]), mevalue_eg(piece_value[i]));
	}
	printf("\n");
#if 1
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
	printf("\n\n");
	param = &parameters[PARAM_MOBILITYKNIGHT];
	printf("mevalue mobility[4][28] = {\n\t{\n\t\t");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	printf("\n\t}, {\n\t\t");
	param = &parameters[PARAM_MOBILITYBISHOP];
	for (size_t i = 0; i < param->size; i++) {
		mevalue_print(param->ptr[i]);
		if (i == 8)
			printf("\n\t\t");
	}
	printf("\n\t}, {\n\t\t");
	param = &parameters[PARAM_MOBILITYROOK];
	for (size_t i = 0; i < param->size; i++) {
		mevalue_print(param->ptr[i]);
		if (i == 8)
			printf("\n\t\t");
	}
	printf("\n\t}, {\n\t\t");
	param = &parameters[PARAM_MOBILITYQUEEN];
	for (size_t i = 0; i < param->size; i++) {
		mevalue_print(param->ptr[i]);
		if (i == 8 || i == 17 || i == 26)
			printf("\n\t\t");
	}
	printf("\n\t}\n};\n\n");
	param = &parameters[PARAM_PAWNSHELTER];
	printf("mevalue pawn_shelter[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		mevalue_print(param->ptr[i]);
	}
	printf("\n};\n\n");
	param = &parameters[PARAM_UNBLOCKEDSTORM];
	printf("mevalue unblocked_storm[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		mevalue_print(param->ptr[i]);
	}
	printf("\n};\n\n");
	param = &parameters[PARAM_UNBLOCKABLESTORM];
	printf("mevalue unblockable_storm[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		mevalue_print(param->ptr[i]);
	}
	printf("\n};\n\n");
	param = &parameters[PARAM_BLOCKEDSTORM];
	printf("mevalue blocked_storm[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		mevalue_print(param->ptr[i]);
	}
	printf("\n};\n\n");
	param = &parameters[PARAM_KINGONOPENFILE];
	printf("\nmevalue king_on_open_file     = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTOUTPOST];
	printf("\nmevalue knight_outpost        = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTOUTPOSTATTACK];
	printf("\nmevalue knight_outpost_attack = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPOUTPOST];
	printf("\nmevalue bishop_outpost        = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPOUTPOSTATTACK];
	printf("\nmevalue bishop_outpost_attack = ");
	mevalue_print(param->ptr[0]);


	param = &parameters[PARAM_BISHOPLONGDIAGONAL];
	printf("\nmevalue bishop_long_diagonal  = ");
	mevalue_print(param->ptr[0]);

	param = &parameters[PARAM_KNIGHTBEHINDPAWN];
	printf("\nmevalue knight_behind_pawn    = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPBEHINDPAWN];
	printf("\nmevalue bishop_behind_pawn    = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_DEFENDEDKNIGHT];
	printf("\nmevalue defended_knight       = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_DEFENDEDBISHOP];
	printf("\nmevalue defended_bishop       = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTFARFROMKING];
	printf("\nmevalue knight_far_from_king  = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPFARFROMKING];
	printf("\nmevalue bishop_far_from_king  = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTPAIR];
	printf("\nmevalue knight_pair           = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPPAIR];
	printf("\nmevalue bishop_pair           = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKPAIR];
	printf("\nmevalue rook_pair             = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_PAWNBLOCKINGBISHOP];
	printf("\nmevalue pawn_blocking_bishop  = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKOPEN];
	printf("\nmevalue rook_open             = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKSEMI];
	printf("\nmevalue rook_semi             = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKCLOSED];
	printf("\nmevalue rook_closed           = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKBLOCKED];
	printf("\nmevalue rook_blocked          = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BADQUEEN];
	printf("\nmevalue bad_queen             = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_KINGATTACKPAWN];
	printf("\nmevalue king_attack_pawn      = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_KINGDEFENDPAWN];
	printf("\nmevalue king_defend_pawn      = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_TEMPOBONUS];
	printf("\nmevalue tempo_bonus           = ");
	mevalue_print(param->ptr[0]);

	param = &parameters[PARAM_PAWNTHREAT];
	printf("\n\nmevalue pawn_threat           = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_PUSHTHREAT];
	printf("\nmevalue push_threat           = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_MINORTHREAT];
	printf("\nmevalue minor_threat[7]       = { ");
	for (size_t i = 0; i < param->size; i++) {
		mevalue_print(param->ptr[i]);
	}
	param = &parameters[PARAM_ROOKTHREAT];
	printf("};\nmevalue rook_threat[7]      = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);

	printf("};\n\n");
	param = &parameters[PARAM_WEAKSQUARES];
	printf("int weak_squares              = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_ENEMYNOQUEEN];
	printf("int enemy_no_queen            = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_KNIGHTATTACK];
	printf("int knight_attack             = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_BISHOPATTACK];
	printf("int bishop_attack             = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_ROOKATTACK];
	printf("int rook_attack               = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_QUEENATTACK];
	printf("int queen_attack              = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_DISCOVERY];
	printf("int discovery                 = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_CHECKS];
	printf("int checks[12]                = { ");
	for (size_t i = 0; i < param->size; i++)
		printf("%d, ", param->ptr[i]);
	printf("};\n\n");
	param = &parameters[PARAM_PHASEMAX];
	printf("int phase_max                 = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEMIN];
	printf("int phase_min                 = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEKNIGHT];
	printf("int phase_knight              = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEBISHOP];
	printf("int phase_bishop              = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEROOK];
	printf("int phase_rook                = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEQUEEN];
	printf("int phase_queen               = %d;\n", param->ptr[0]);
#endif

	param = &parameters[PARAM_SUPPORTEDPAWN];
	printf("\nmevalue supported_pawn     = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BACKWARDPAWN];
	printf("\nmevalue backward_pawn[4]   = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_ISOLATEDPAWN];
	printf("};\nmevalue isolated_pawn[4]   = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_DOUBLEDPAWN];
	printf("};\nmevalue doubled_pawn[4]    = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_CONNECTEDPAWN];
	printf("};\nmevalue connected_pawn[7]  = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_PASSEDPAWN];
	printf("};\nmevalue passed_pawn[7]     = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_PASSEDBLOCKED];
	printf("};\nmevalue passed_blocked[7]  = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_PASSEDFILE];
	printf("};\nmevalue passed_file[4]     = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_DISTANCEUS];
	printf("};\nmevalue distance_us[7]     = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_DISTANCETHEM];
	printf("};\nmevalue distance_them[7]   = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	printf("};\n\n");
}

void arrays_init(void) {
	for (size_t i = 0; i < SIZE(parameters); i++) {
		size_t bytes = 2 * parameters[i].size * sizeof(double);
		parameters[i].value = malloc(bytes);
		parameters[i].grad = malloc(bytes);
		parameters[i].m = malloc(bytes);
		memset(parameters[i].m, 0, bytes);
		parameters[i].v = malloc(bytes);
		memset(parameters[i].v, 0, bytes);
		
		for (size_t j = 0; j < parameters[i].size; j++) {
			if (parameters[i].type == TYPE_MEVALUE) {
				parameters[i].value[2 * j + mg] = mevalue_mg(parameters[i].ptr[j]);
				parameters[i].value[2 * j + eg] = mevalue_eg(parameters[i].ptr[j]);
			}
			else {
				parameters[i].value[2 * j] = parameters[i].ptr[j];
			}
		}
	}
}

double sigmoid(int q) {
	const double f = -K * q / 400;
	return 1.0 / (1.0 + pow(10, f));
}

double sigmoid_grad(int q) {
	const double s = sigmoid(q);
	return K * log(10) / 400 * s * (1 - s);
}

void update_value(mevalue *ptr, double value[2], int type) {
	if (type == TYPE_MEVALUE)
		*ptr = S((int32_t)round(value[mg]), (int32_t)round(value[eg]));
	else
		*ptr = round(value[0]);
}

void parameter_step(double value[2], double m[2], double v[2], int type, int weight_decay_enabled) {
	for (int i = 0; i <= type ; i++) {
		double m_hat = m[i] / (1 - pow(beta_1, t));
		double v_hat = v[i] / (1 - pow(beta_2, t));
		value[i] -= alpha * (m_hat / (sqrt(v_hat) + epsilon) + (weight_decay_enabled == WEIGHTDECAY_YES) * weight_decay * value[i]);
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
		if (parameters[i].tune == TUNE_NO)
			continue;
		for (size_t j = 0; j < parameters[i].size; j++) {
			update_mv(&parameters[i].m[2 * j], &parameters[i].v[2 * j], &parameters[i].grad[2 * j]);
			parameter_step(&parameters[i].value[2 * j], &parameters[i].m[2 * j], &parameters[i].v[2 * j], parameters[i].type, parameters[i].weight_decay);
			update_value(&parameters[i].ptr[j], &parameters[i].value[2 * j], parameters[i].type);
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

/* Calculates the gradient of the error function by hand.
 * The error function is E(x)=(result-sigmoid(evaluate(x)))^2
 * which by the chain rule gives
 * E'(x)=2*(sigmoid(evaluate(x))-result)*sigmoid'(evaluate(x))*evaluate'(x).
 */
double grad_calc(struct position *pos, double result) {
	memset(&trace, 0, sizeof(trace));
	int32_t eval = evaluate_classical(pos);
	eval = pos->turn == white ? eval : -eval;

	double mgs = (double)trace.p / PHASE;
	double egs = (double)(PHASE - trace.p) / PHASE * trace.s / NORMAL_SCALE;

	double factor = 2 * (sigmoid(eval) - result) * sigmoid_grad(eval);

	struct parameter *param;
	/* Piece value get way too low
	 * mg - 80 327 361 390 722
	 * eg - 109 261 258 441 774
	 */
	if ((param = &parameters[PARAM_PIECEVALUE])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int piece = i + 1;
			double grad = factor * ((double)popcount(pos->piece[white][piece]) - popcount(pos->piece[black][piece]));
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			if (piece != bishop)
				continue;
			piece_value[i] += S(1, 0);
			tables_init();
			int32_t new_eval = evaluate_classical(pos);
			new_eval = pos->turn == white ? new_eval : -new_eval;
			piece_value[i] -= S(1, 0);
			tables_init();
			double gradmgtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);
			piece_value[i] += S(0, 1);
			tables_init();
			new_eval = evaluate_classical(pos);
			new_eval = pos->turn == white ? new_eval : -new_eval;
			piece_value[i] -= S(0, 1);
			tables_init();
			double gradegtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);

			if (gradmg || gradeg || gradmgtest || gradegtest) {
				print_position(pos, 0);
				char fen[128];
				printf("%s\n", pos_to_fen(fen, pos));
				printf("factor: %e\n", factor);
				printf("gradmg: %e\n", gradmg);
				printf("gradeg: %e\n", gradeg);
				printf("gradmgtest: %e\n", gradmgtest);
				printf("gradegtest: %e\n", gradegtest);
				printf("p: %d\n", p);
				printf("s: %d\n", s);
				printf("mgs: %f\n", mgs);
				printf("egs: %f\n", egs);
				printf("eval: %d\n", eval);
				printf("result: %f\n", result);
				printf("sigmoid: %f\n", sigmoid(eval));
				printf("valmg: %d\n", mevalue_mg(piece_value[i]));
				printf("valeg: %d\n", mevalue_eg(piece_value[i]));
			}
#endif
		}
	}
	if ((param = &parameters[PARAM_PSQTPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int square = i + 8;
			int f = file_of(square);
			int r = rank_of(square);
			square = make_square(f, 7 - r);
			double grad = factor * ((pos->mailbox[orient_horizontal(white, square)] == white_pawn) - (pos->mailbox[orient_horizontal(black, square)] == black_pawn));
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			white_psqtable[0][i + 8] += S(100, 0);
			tables_init();
			int16_t new_eval = evaluate_classical(pos);
			new_eval = pos->turn == white ? new_eval : -new_eval;
			white_psqtable[0][i + 8] -= S(100, 0);
			tables_init();
			double gradtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);


			if (gradmg != 0 || gradeg != 0) {
				char tmp[3];
				print_position(pos, 0);
				printf("turn: %s\n", pos->turn ? "white" : "black");
				printf("eval: %d\n", (2 * pos->turn - 1) * evaluate_classical(pos));
				printf("square: %s (%d, %ld)\n", algebraic(tmp, square), square, i);
				printf("num pawns: %d\n", (pos->mailbox[orient_horizontal(white, square)] == white_pawn) - (pos->mailbox[orient_horizontal(black, square)] == black_pawn));
				printf("gradmg: %e\n", gradmg);
				printf("gradeg: %e\n", gradeg);
				printf("gradtest: %e\n", gradtest);
				printf("result: %f\n", result);
			}
#endif
		}
	}
	if ((param = &parameters[PARAM_PSQTKNIGHT])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int f = i % 4;
			int r = i / 4;
			int square1 = make_square(f, 7 - r);
			int square2 = orient_vertical(1, square1);
			int num = (pos->mailbox[orient_horizontal(white, square1)] == white_knight) + (pos->mailbox[orient_horizontal(white, square2)] == white_knight) -
				  (pos->mailbox[orient_horizontal(black, square1)] == black_knight) - (pos->mailbox[orient_horizontal(black, square2)] == black_knight);
			double grad = factor * num;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			white_psqtable[1][i] += S(100, 0);
			tables_init();
			int16_t new_eval = evaluate_classical(pos);
			new_eval = pos->turn == white ? new_eval : -new_eval;
			white_psqtable[1][i] -= S(100, 0);
			tables_init();
			double gradtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);

			if ((gradmg == 0) ^ (gradtest == 0)) {
				printf("square: %d, %d, %ld\n", square1, square2, i);
				char str[3];
				printf("w1 %s\n", algebraic(str, square1));
				printf("w2 %s\n", algebraic(str, square2));
				print_position(pos, 0);
				printf("num: %d\n", num);
				printf("gradmg: %e\n", gradmg);
				printf("gradeg: %e\n", gradeg);
				printf("gradtest: %e\n", gradtest);
				exit(3);
			}
#endif
		}
	}
	if ((param = &parameters[PARAM_PSQTBISHOP])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int f = i % 4;
			int r = i / 4;
			int square1 = make_square(f, 7 - r);
			int square2 = orient_vertical(1, square1);
			int num = (pos->mailbox[orient_horizontal(white, square1)] == white_bishop) + (pos->mailbox[orient_horizontal(white, square2)] == white_bishop) -
				  (pos->mailbox[orient_horizontal(black, square1)] == black_bishop) - (pos->mailbox[orient_horizontal(black, square2)] == black_bishop);
			double grad = factor * num;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PSQTROOK])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int f = i % 4;
			int r = i / 4;
			int square1 = make_square(f, 7 - r);
			int square2 = orient_vertical(1, square1);
			int num = (pos->mailbox[orient_horizontal(white, square1)] == white_rook) + (pos->mailbox[orient_horizontal(white, square2)] == white_rook) -
				  (pos->mailbox[orient_horizontal(black, square1)] == black_rook) - (pos->mailbox[orient_horizontal(black, square2)] == black_rook);
			double grad = factor * num;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PSQTQUEEN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int f = i % 4;
			int r = i / 4;
			int square1 = make_square(f, 7 - r);
			int square2 = orient_vertical(1, square1);
			int num = (pos->mailbox[orient_horizontal(white, square1)] == white_queen) + (pos->mailbox[orient_horizontal(white, square2)] == white_queen) -
				  (pos->mailbox[orient_horizontal(black, square1)] == black_queen) - (pos->mailbox[orient_horizontal(black, square2)] == black_queen);
			double grad = factor * num;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PSQTKING])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int f = i % 4;
			int r = i / 4;
			int square1 = make_square(f, 7 - r);
			int square2 = orient_vertical(1, square1);
			int num = (pos->mailbox[orient_horizontal(white, square1)] == white_king) + (pos->mailbox[orient_horizontal(white, square2)] == white_king) -
				  (pos->mailbox[orient_horizontal(black, square1)] == black_king) - (pos->mailbox[orient_horizontal(black, square2)] == black_king);
			double grad = factor * num;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			if (i == 1 && grad) {
				print_position(pos, 0);
				printf("gradmg: %e\n", gradmg);
				printf("gradeg: %e\n", gradeg);
				printf("phase: %f\n", (double)p / 256);
				printf("result: %f\n", result);
				printf("eval: %d\n", eval);
				printf("sigmoid(eval): %f\n", sigmoid(eval));
			}
#endif
		}
	}
	if ((param = &parameters[PARAM_MOBILITYKNIGHT])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdm = trace.mobility[white][knight - 2][i] - trace.mobility[black][knight - 2][i];
			double grad = factor * dEdm;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_MOBILITYBISHOP])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdm = trace.mobility[white][bishop - 2][i] - trace.mobility[black][bishop - 2][i];
			double grad = factor * dEdm;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_MOBILITYROOK])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdm = trace.mobility[white][rook - 2][i] - trace.mobility[black][rook - 2][i];
			double grad = factor * dEdm;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_MOBILITYQUEEN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdm = trace.mobility[white][queen - 2][i] - trace.mobility[black][queen - 2][i];
			double grad = factor * dEdm;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PAWNSHELTER])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdp = trace.pawn_shelter[white][i] - trace.pawn_shelter[black][i];
			double grad = factor * dEdp;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_UNBLOCKEDSTORM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdu = trace.unblocked_storm[white][i] - trace.unblocked_storm[black][i];
			double grad = factor * dEdu;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_UNBLOCKABLESTORM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdb = trace.unblockable_storm[white][i] - trace.unblockable_storm[black][i];
			double grad = factor * dEdb;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_BLOCKEDSTORM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdb = trace.blocked_storm[white][i] - trace.blocked_storm[black][i];
			double grad = factor * dEdb;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_KINGONOPENFILE])->tune == TUNE_YES) {
		double dEdx = trace.king_on_open_file[white] - trace.king_on_open_file[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTOUTPOST])->tune == TUNE_YES) {
		double dEdx = trace.knight_outpost[white] - trace.knight_outpost[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTOUTPOSTATTACK])->tune == TUNE_YES) {
		double dEdx = trace.knight_outpost_attack[white] - trace.knight_outpost_attack[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPOUTPOST])->tune == TUNE_YES) {
		double dEdx = trace.bishop_outpost[white] - trace.bishop_outpost[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPOUTPOSTATTACK])->tune == TUNE_YES) {
		double dEdx = trace.bishop_outpost_attack[white] - trace.bishop_outpost_attack[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPLONGDIAGONAL])->tune == TUNE_YES) {
		double dEdx = trace.bishop_long_diagonal[white] - trace.bishop_long_diagonal[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTBEHINDPAWN])->tune == TUNE_YES) {
		double dEdx = trace.knight_behind_pawn[white] - trace.knight_behind_pawn[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPBEHINDPAWN])->tune == TUNE_YES) {
		double dEdx = trace.bishop_behind_pawn[white] - trace.bishop_behind_pawn[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_DEFENDEDKNIGHT])->tune == TUNE_YES) {
		double dEdx = trace.defended_knight[white] - trace.defended_knight[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_DEFENDEDBISHOP])->tune == TUNE_YES) {
		double dEdx = trace.defended_bishop[white] - trace.defended_bishop[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTFARFROMKING])->tune == TUNE_YES) {
		double dEdx = trace.knight_far_from_king[white] - trace.knight_far_from_king[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPFARFROMKING])->tune == TUNE_YES) {
		double dEdx = trace.bishop_far_from_king[white] - trace.bishop_far_from_king[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTPAIR])->tune == TUNE_YES) {
		double dEdx = trace.knight_pair[white] - trace.knight_pair[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPPAIR])->tune == TUNE_YES) {
		double dEdx = trace.bishop_pair[white] - trace.bishop_pair[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKPAIR])->tune == TUNE_YES) {
		double dEdx = trace.rook_pair[white] - trace.rook_pair[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_PAWNBLOCKINGBISHOP])->tune == TUNE_YES) {
		double dEdx = trace.pawn_blocking_bishop[white] - trace.pawn_blocking_bishop[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKOPEN])->tune == TUNE_YES) {
		double dEdx = trace.rook_open[white] - trace.rook_open[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKSEMI])->tune == TUNE_YES) {
		double dEdx = trace.rook_semi[white] - trace.rook_semi[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKCLOSED])->tune == TUNE_YES) {
		double dEdx = trace.rook_closed[white] - trace.rook_closed[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKBLOCKED])->tune == TUNE_YES) {
		double dEdx = trace.rook_blocked[white] - trace.rook_blocked[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BADQUEEN])->tune == TUNE_YES) {
		double dEdx = trace.bad_queen[white] - trace.bad_queen[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
#if 0
		if (grad) {
			char fen[128];
			print_position(pos, 0);
			printf("%s\n", pos_to_fen(fen, pos));
			printf("gradmg: %e\n", gradmg);
			printf("gradeg: %e\n", gradeg);
			printf("phase: %f\n", (double)trace.p / 256);
			printf("result: %f\n", result);
			printf("eval: %d\n", eval);
			printf("sigmoid(eval): %f\n", sigmoid(eval));
			printf("num: %f\n", dEdx);
			int color = dEdx > 0;
			printf("%d\n", color);
			int square = ctz(pos->piece[color][queen]);
			uint64_t blockers = generate_blockers(pos, pos->piece[other_color(color)][bishop] | pos->piece[other_color(color)][rook], square);
			print_bitboard(blockers);
		}
#endif
	}
	if ((param = &parameters[PARAM_KINGATTACKPAWN])->tune == TUNE_YES) {
		double dEdx = trace.king_attack_pawn[white] - trace.king_attack_pawn[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KINGDEFENDPAWN])->tune == TUNE_YES) {
		double dEdx = trace.king_defend_pawn[white] - trace.king_defend_pawn[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_TEMPOBONUS])->tune == TUNE_YES) {
		double dEdx = 2 * pos->turn - 1;
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}

	if ((param = &parameters[PARAM_PAWNTHREAT])->tune == TUNE_YES) {
		double dEdx = trace.pawn_threat[white] - trace.pawn_threat[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_PUSHTHREAT])->tune == TUNE_YES) {
		double dEdx = trace.push_threat[white] - trace.push_threat[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_MINORTHREAT])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.minor_threat[white][i] - trace.minor_threat[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			if (grad) {
				char fen[128];
				print_position(pos, 0);
				printf("%s\n", pos_to_fen(fen, pos));
				printf("index: %ld\n", i);
				printf("grad: %e\n", grad);
				printf("result: %f\n", result);
				printf("sigmoid(eval): %f\n", sigmoid(eval));
			}
#endif
		}
	}
	if ((param = &parameters[PARAM_ROOKTHREAT])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.rook_threat[white][i] - trace.rook_threat[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}

	/* We want to calculate dE/dw=dE/dr*dr/dk*dk/dw where r is the rectified
	 * king danger r=MAX(k, 0).
	 */
	if ((param = &parameters[PARAM_WEAKSQUARES])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(trace.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = trace.king_danger[color] >= 0;
			double dkdw = trace.weak_squares[color];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
#if 0
		weak_squares_danger += 1;
		int16_t new_eval = evaluate_classical(pos);
		new_eval = pos->turn == white ? new_eval : -new_eval;
		weak_squares_danger -= 1;
		double gradtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);

		if (total_grad) {
			print_position(pos, 0);
			print_bitboard(ei.weak_squares[black]);
			print_bitboard(ei.weak_squares[white]);
			printf("grad: %e\n", total_grad);
			printf("gradtest: %e\n", gradtest);
		}
#endif
	}
	if ((param = &parameters[PARAM_ENEMYNOQUEEN])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(trace.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = trace.king_danger[color] >= 0;
			double dkdw = trace.enemy_no_queen[color];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_KNIGHTATTACK])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(trace.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = trace.king_danger[color] >= 0;
			double dkdw = trace.knight_attack[color];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_BISHOPATTACK])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(trace.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = trace.king_danger[color] >= 0;
			double dkdw = trace.bishop_attack[color];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_ROOKATTACK])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(trace.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = trace.king_danger[color] >= 0;
			double dkdw = trace.rook_attack[color];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_QUEENATTACK])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(trace.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = trace.king_danger[color] >= 0;
			double dkdw = trace.queen_attack[color];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_DISCOVERY])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(trace.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = trace.king_danger[color] >= 0;
			double dkdw = trace.discovery[color];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_CHECKS])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			for (int color = 0; color <= 1; color++) {
				int r = MAX(trace.king_danger[color], 0);
				double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
				double drdk = trace.king_danger[color] >= 0;
				double dkdw = trace.checks[color][i];
				double dEdw = dEdr * drdk * dkdw;
				double grad = factor * dEdw * (2 * color - 1);
				param->grad[2 * i] += grad;
			}
		}
	}

	/* We want to calculate dE/dp_M=dE/dp*dp/dp_M where p is the phase.
	 * E depends on p as E=p*mg+(1-p)*eg, so dE/dp=mg-eg. We now have
	 * p=(CLAMP(material, p_m, p_M)-p_m)/(p_M-p_m). It is not differentiable
	 * but we try a difference quotient.
	 */
	if ((param = &parameters[PARAM_PHASEMAX])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(trace.eval) - mevalue_eg(trace.eval);
		int p_M2 = phase_max + 1;
		int p_M1 = phase_max - 1;
		int p_m = phase_min;
		double dpdp_M = ((double)CLAMP(trace.material, p_m, p_M2) - p_m) / (p_M2 - p_m) -
				((double)CLAMP(trace.material, p_m, p_M1) - p_m) / (p_M1 - p_m);
		dpdp_M /= p_M2 - p_M1;
		double grad = factor * dEdp * dpdp_M;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEMIN])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(trace.eval) - mevalue_eg(trace.eval);
		int p_m2 = phase_min + 1;
		int p_m1 = phase_min - 1;
		int p_M = phase_max;
		double dpdp_m = ((double)CLAMP(trace.material, p_m2, p_M) - p_m2) / (p_M - p_m2) -
				((double)CLAMP(trace.material, p_m1, p_M) - p_m1) / (p_M - p_m1);
		dpdp_m /= p_m2 - p_m1;
		double grad = factor * dEdp * dpdp_m;
		param->grad[0] += grad;
	}
	/* Same idea as before. */
	if ((param = &parameters[PARAM_PHASEKNIGHT])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(trace.eval) - mevalue_eg(trace.eval);
		int num_knights = popcount(pos->piece[white][knight] | pos->piece[black][knight]);
		int m2 = trace.material + num_knights * 1;
		int m1 = trace.material - num_knights * 1;
		int p_M = phase_max;
		int p_m = phase_min;
		double dpdm = ((double)CLAMP(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)CLAMP(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
#if 0
		phase_knight += 1;
		int16_t new_eval = evaluate_classical(pos);
		new_eval = pos->turn == white ? new_eval : -new_eval;
		phase_knight -= 1;
		double gradtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);

		if ((grad == 0) & (gradtest != 0)) {
			printf("num_knights: %d\n", num_knights);
			printf("material: %d\n", ei.material);
			printf("dpdm: %f\n", dpdm);
			/* double factor = 2 * (sigmoid(eval) - result) * sigmoid_grad(eval); */
			printf("factor: %f\n", factor);
			print_position(pos, 0);
			char fen[128];
			printf("%s\n", pos_to_fen(fen, pos));
			printf("eval: %d\n", eval);
			printf("sigmoid(eval): %f\n", sigmoid(eval));
			printf("result: %f\n", result);
			printf("dEdp: %f\n", dEdp);
			printf("grad: %e\n", grad);
			printf("gradtest: %e\n", gradtest);
			exit(3);
		}
#endif
	}
	if ((param = &parameters[PARAM_PHASEBISHOP])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(trace.eval) - mevalue_eg(trace.eval);
		int num_bishops = popcount(pos->piece[white][bishop] | pos->piece[black][bishop]);
		int m2 = trace.material + num_bishops * 1;
		int m1 = trace.material - num_bishops * 1;
		int p_M = phase_max;
		int p_m = phase_min;
		double dpdm = ((double)CLAMP(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)CLAMP(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEROOK])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(trace.eval) - mevalue_eg(trace.eval);
		int num_rooks = popcount(pos->piece[white][rook] | pos->piece[black][rook]);
		int m2 = trace.material + num_rooks * 1;
		int m1 = trace.material - num_rooks * 1;
		int p_M = phase_max;
		int p_m = phase_min;
		double dpdm = ((double)CLAMP(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)CLAMP(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEQUEEN])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(trace.eval) - mevalue_eg(trace.eval);
		int num_queens = popcount(pos->piece[white][queen] | pos->piece[black][queen]);
		int m2 = trace.material + num_queens * 1;
		int m1 = trace.material - num_queens * 1;
		int p_M = phase_max;
		int p_m = phase_min;
		double dpdm = ((double)CLAMP(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)CLAMP(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}

	if ((param = &parameters[PARAM_SUPPORTEDPAWN])->tune == TUNE_YES) {
		double dEdx = trace.supported_pawn[white] - trace.supported_pawn[black];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BACKWARDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.backward_pawn[white][i] - trace.backward_pawn[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_ISOLATEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.isolated_pawn[white][i] - trace.isolated_pawn[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_DOUBLEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.doubled_pawn[white][i] - trace.doubled_pawn[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_CONNECTEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.connected_pawn[white][i] - trace.connected_pawn[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PASSEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.passed_pawn[white][i] - trace.passed_pawn[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PASSEDBLOCKED])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.passed_blocked[white][i] - trace.passed_blocked[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PASSEDFILE])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.passed_file[white][i] - trace.passed_file[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_DISTANCEUS])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.distance_us[white][i] - trace.distance_us[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_DISTANCETHEM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.distance_them[white][i] - trace.distance_them[black][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			if (grad) {
				char fen[128];
				print_position(pos, 0);
				printf("%s\n", pos_to_fen(fen, pos));
				printf("grad: %e\n", grad);
				printf("result: %f\n", result);
				printf("sigmoid: %f\n", sigmoid(eval));
			}
#endif
		}
	}

	return (sigmoid(eval) - result) * (sigmoid(eval) - result);
}

size_t grad(FILE *f, struct position *pos) {

	size_t actual_size = 0;
	while (actual_size < BATCH_SIZE) {
		move m = 0;
		fread(&m, 2, 1, f);
		if (m)
			do_move(pos, &m);
		else
			fread(pos, sizeof(struct partialposition), 1, f);

		int16_t eval;
		fread(&eval, 2, 1, f);
		if (feof(f))
			break;

		int skip = (eval == VALUE_NONE) || gbernoulli(0.9);
		if (skip)
			continue;

		eval = pos->turn == white ? eval : -eval;
		double result = sigmoid(eval);

		grad_calc(pos, result);

		actual_size++;
	}
	if (actual_size == 0)
		return 0;

	for (size_t i = 0; i < SIZE(parameters); i++)
		for (size_t j = 0; j < parameters[i].size; j++)
			for (int k = 0; k <= parameters[i].type; k++)
				parameters[i].grad[2 * j + k] /= actual_size;
	return actual_size;
}

int main(int argc, char **argv) {
	struct position pos;

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
	option_history = 0;

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	search_init();
	moveorder_init();
	position_init();
	arrays_init();
	tables_init();

	while (1) {
		zero_grad();
		if (!grad(f, &pos)) {
			parameters_print();
			fseek(f, 0, SEEK_SET);
			continue;
		}
		step();
	}

	fclose(f);
}
