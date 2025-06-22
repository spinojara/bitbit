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

#define _GNU_SOURCE
#include "texelbit.h"

#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

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
#include "io.h"

struct trace trace;

extern score_t king_on_open_file;
extern score_t knight_outpost;
extern score_t knight_outpost_attack;
extern score_t bishop_outpost;
extern score_t bishop_outpost_attack;
extern score_t bishop_long_diagonal;
extern score_t knight_behind_pawn;
extern score_t bishop_behind_pawn;
extern score_t defended_knight;
extern score_t defended_bishop;
extern score_t knight_far_from_king;
extern score_t bishop_far_from_king;
extern score_t knight_pair;
extern score_t bishop_pair;
extern score_t rook_pair;
extern score_t pawn_blocking_bishop;
extern score_t rook_open;
extern score_t rook_semi;
extern score_t rook_closed;
extern score_t rook_blocked;
extern score_t bad_queen;
extern score_t king_attack_pawn;
extern score_t king_defend_pawn;
extern score_t tempo_bonus;

extern score_t pawn_threat;
extern score_t push_threat;
extern score_t minor_threat[7];
extern score_t rook_threat[7];

extern int weak_squares;
extern int enemy_no_queen;
extern int knight_attack;
extern int bishop_attack;
extern int rook_attack;
extern int queen_attack;
extern int discovery;
extern int checks[12];

extern int phase_max;
extern int phase_min;
extern int phase_knight;
extern int phase_bishop;
extern int phase_rook;
extern int phase_queen;

extern score_t supported_pawn;
extern score_t backward_pawn[4];
extern score_t isolated_pawn[4];
extern score_t doubled_pawn[4];
extern score_t connected_pawn[7];
extern score_t passed_pawn[7];
extern score_t passed_blocked[7];
extern score_t passed_file[4];
extern score_t distance_us[7];
extern score_t distance_them[7];

#define BATCH_SIZE (32)

#define PARAMETER(x, y, z, w, q) { .ptr = x, .size = y, .type = z, .weight_decay = w, .tune = q }

enum {
	TYPE_INT,
	TYPE_SCORE,
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

double K = 1.0;
double beta1 = 0.9;
double beta2 = 0.999;
double epsilon = 1e-8;
double alpha = 1e-3;
double weight_decay = 1e-4;
size_t t = 0;

struct parameter {
	score_t *ptr;
	size_t size;
	int type;
	int tune;
	int weight_decay;
	double *value;
	double *grad;
	double *m;
	double *v;
};

/* Parameter list has to be in the same order as the enum. */
struct parameter parameters[] = {
	PARAMETER(&piece_value[0],            5, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&white_psqtable[0][8],     48, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[1][0],     32, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[2][0],     32, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[3][0],     32, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[4][0],     32, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&white_psqtable[5][0],     32, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),

	PARAMETER(&mobility[0][0],            9, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&mobility[1][0],           14, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&mobility[2][0],           15, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&mobility[3][0],           28, TYPE_SCORE, WEIGHTDECAY_YES, TUNE_YES),

	PARAMETER(&pawn_shelter[0],          28, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&blocked_storm[0],         28, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&unblocked_storm[0],       28, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&unblockable_storm[0],     28, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&king_on_open_file,         1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_outpost,            1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_outpost_attack,     1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_outpost,            1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_outpost_attack,     1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_long_diagonal,      1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_behind_pawn,        1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_behind_pawn,        1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&defended_knight,           1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&defended_bishop,           1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_far_from_king,      1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_far_from_king,      1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_pair,               1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_pair,               1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_pair,                 1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&pawn_blocking_bishop,      1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_open,                 1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_semi,                 1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_closed,               1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_blocked,              1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bad_queen,                 1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&king_attack_pawn,          1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&king_defend_pawn,          1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&tempo_bonus,               1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&pawn_threat,               1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&push_threat,               1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&minor_threat[0],           6, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_threat[0],            6, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&weak_squares,              1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&enemy_no_queen,            1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&knight_attack,             1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&bishop_attack,             1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&rook_attack,               1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&queen_attack,              1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&discovery,                 1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&checks[0],                12, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&phase_max,                 1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_min,                 1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_knight,              1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_NO),
	PARAMETER(&phase_bishop,              1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_rook,                1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&phase_queen,               1, TYPE_INT,   WEIGHTDECAY_NO,  TUNE_YES),

	PARAMETER(&supported_pawn,            1, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&backward_pawn[0],          4, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&isolated_pawn[0],          4, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&doubled_pawn[0],           4, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&connected_pawn[0],         7, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&passed_pawn[0],            7, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&passed_blocked[0],         7, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&passed_file[0],            4, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&distance_us[0],            7, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
	PARAMETER(&distance_them[0],          7, TYPE_SCORE, WEIGHTDECAY_NO,  TUNE_YES),
};

void score_print(score_t eval) {
	printf("S(%3d,%3d), ", score_mg(eval), score_eg(eval));;
}

void parameters_print(void) {
	struct parameter *param;
	for (int i = 0; i < 5; i++) {
		printf("S(%4d,%4d), ", score_mg(piece_value[i]), score_eg(piece_value[i]));
	}
	printf("\n");
#if 1
	for (int i = 8; i < 56; i++) {
		if (i % 8 == 0)
			printf("\n");
		score_print(white_psqtable[0][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		score_print(white_psqtable[1][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		score_print(white_psqtable[2][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		score_print(white_psqtable[3][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		score_print(white_psqtable[4][i]);
	}
	printf("\n");
	for (int i = 0; i < 32; i++) {
		if (i % 4 == 0)
			printf("\n");
		score_print(white_psqtable[5][i]);
	}
	printf("\n\n");
	param = &parameters[PARAM_MOBILITYKNIGHT];
	printf("score_t mobility[4][28] = {\n\t{\n\t\t");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	printf("\n\t}, {\n\t\t");
	param = &parameters[PARAM_MOBILITYBISHOP];
	for (size_t i = 0; i < param->size; i++) {
		score_print(param->ptr[i]);
		if (i == 8)
			printf("\n\t\t");
	}
	printf("\n\t}, {\n\t\t");
	param = &parameters[PARAM_MOBILITYROOK];
	for (size_t i = 0; i < param->size; i++) {
		score_print(param->ptr[i]);
		if (i == 8)
			printf("\n\t\t");
	}
	printf("\n\t}, {\n\t\t");
	param = &parameters[PARAM_MOBILITYQUEEN];
	for (size_t i = 0; i < param->size; i++) {
		score_print(param->ptr[i]);
		if (i == 8 || i == 17 || i == 26)
			printf("\n\t\t");
	}
	printf("\n\t}\n};\n\n");
	param = &parameters[PARAM_PAWNSHELTER];
	printf("score_t pawn_shelter[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		score_print(param->ptr[i]);
	}
	printf("\n};\n\n");
	param = &parameters[PARAM_UNBLOCKEDSTORM];
	printf("score_t unblocked_storm[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		score_print(param->ptr[i]);
	}
	printf("\n};\n\n");
	param = &parameters[PARAM_UNBLOCKABLESTORM];
	printf("score_t unblockable_storm[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		score_print(param->ptr[i]);
	}
	printf("\n};\n\n");
	param = &parameters[PARAM_BLOCKEDSTORM];
	printf("score_t blocked_storm[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		score_print(param->ptr[i]);
	}
	printf("\n};\n\n");
	param = &parameters[PARAM_KINGONOPENFILE];
	printf("\nscore_t king_on_open_file     = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTOUTPOST];
	printf("\nscore_t knight_outpost        = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTOUTPOSTATTACK];
	printf("\nscore_t knight_outpost_attack = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPOUTPOST];
	printf("\nscore_t bishop_outpost        = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPOUTPOSTATTACK];
	printf("\nscore_t bishop_outpost_attack = ");
	score_print(param->ptr[0]);


	param = &parameters[PARAM_BISHOPLONGDIAGONAL];
	printf("\nscore_t bishop_long_diagonal  = ");
	score_print(param->ptr[0]);

	param = &parameters[PARAM_KNIGHTBEHINDPAWN];
	printf("\nscore_t knight_behind_pawn    = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPBEHINDPAWN];
	printf("\nscore_t bishop_behind_pawn    = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_DEFENDEDKNIGHT];
	printf("\nscore_t defended_knight       = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_DEFENDEDBISHOP];
	printf("\nscore_t defended_bishop       = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTFARFROMKING];
	printf("\nscore_t knight_far_from_king  = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPFARFROMKING];
	printf("\nscore_t bishop_far_from_king  = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTPAIR];
	printf("\nscore_t knight_pair           = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPPAIR];
	printf("\nscore_t bishop_pair           = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKPAIR];
	printf("\nscore_t rook_pair             = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_PAWNBLOCKINGBISHOP];
	printf("\nscore_t pawn_blocking_bishop  = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKOPEN];
	printf("\nscore_t rook_open             = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKSEMI];
	printf("\nscore_t rook_semi             = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKCLOSED];
	printf("\nscore_t rook_closed           = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKBLOCKED];
	printf("\nscore_t rook_blocked          = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_BADQUEEN];
	printf("\nscore_t bad_queen             = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_KINGATTACKPAWN];
	printf("\nscore_t king_attack_pawn      = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_KINGDEFENDPAWN];
	printf("\nscore_t king_defend_pawn      = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_TEMPOBONUS];
	printf("\nscore_t tempo_bonus           = ");
	score_print(param->ptr[0]);

	param = &parameters[PARAM_PAWNTHREAT];
	printf("\n\nscore_t pawn_threat           = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_PUSHTHREAT];
	printf("\nscore_t push_threat           = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_MINORTHREAT];
	printf("\nscore_t minor_threat[7]       = { ");
	for (size_t i = 0; i < param->size; i++) {
		score_print(param->ptr[i]);
	}
	param = &parameters[PARAM_ROOKTHREAT];
	printf("};\nscore_t rook_threat[7]      = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);

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
	printf("\nscore_t supported_pawn     = ");
	score_print(param->ptr[0]);
	param = &parameters[PARAM_BACKWARDPAWN];
	printf("\nscore_t backward_pawn[4]   = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	param = &parameters[PARAM_ISOLATEDPAWN];
	printf("};\nscore_t isolated_pawn[4]   = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	param = &parameters[PARAM_DOUBLEDPAWN];
	printf("};\nscore_t doubled_pawn[4]    = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	param = &parameters[PARAM_CONNECTEDPAWN];
	printf("};\nscore_t connected_pawn[7]  = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	param = &parameters[PARAM_PASSEDPAWN];
	printf("};\nscore_t passed_pawn[7]     = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	param = &parameters[PARAM_PASSEDBLOCKED];
	printf("};\nscore_t passed_blocked[7]  = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	param = &parameters[PARAM_PASSEDFILE];
	printf("};\nscore_t passed_file[4]     = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	param = &parameters[PARAM_DISTANCEUS];
	printf("};\nscore_t distance_us[7]     = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	param = &parameters[PARAM_DISTANCETHEM];
	printf("};\nscore_t distance_them[7]   = { ");
	for (size_t i = 0; i < param->size; i++)
		score_print(param->ptr[i]);
	printf("};\n\n");
}

void arrays_init(void) {
	for (size_t i = 0; i < SIZE(parameters); i++) {
		parameters[i].value = malloc(2 * parameters[i].size * sizeof(*parameters[i].value));
		parameters[i].grad = malloc(2 * parameters[i].size * sizeof(*parameters[i].grad));
		parameters[i].m = calloc(2 * parameters[i].size, sizeof(*parameters[i].m));
		parameters[i].v = calloc(2 * parameters[i].size, sizeof(*parameters[i].v));

		for (size_t j = 0; j < parameters[i].size; j++) {
			if (parameters[i].type == TYPE_SCORE) {
				parameters[i].value[2 * j + mg] = score_mg(parameters[i].ptr[j]);
				parameters[i].value[2 * j + eg] = score_eg(parameters[i].ptr[j]);
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

void update_value(score_t *ptr, double value[2], int type) {
	if (type == TYPE_SCORE)
		*ptr = S((int32_t)round(value[mg]), (int32_t)round(value[eg]));
	else
		*ptr = round(value[0]);
}

void parameter_step(double value[2], double m[2], double v[2], int type, int weight_decay_enabled) {
	for (int i = 0; i <= type ; i++) {
		double m_hat = m[i] / (1 - pow(beta1, t));
		double v_hat = v[i] / (1 - pow(beta2, t));
		value[i] -= alpha * (m_hat / (sqrt(v_hat) + epsilon) + (weight_decay_enabled == WEIGHTDECAY_YES) * weight_decay * value[i]);
	}
}

void update_mv(double m[2], double v[2], double grad[2]) {
	for (int i = 0; i < 2; i++) {
		m[i] = beta1 * m[i] + (1 - beta1) * grad[i];
		v[i] = beta2 * v[i] + (1 - beta2) * grad[i] * grad[i];
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
	for (size_t i = 0; i < SIZE(parameters); i++)
		memset(parameters[i].grad, 0, 2 * parameters[i].size * sizeof(*parameters[i].grad));
}

/* Calculates the gradient of the error function by hand.
 * The error function is E(x)=(result-sigmoid(evaluate(x)))^2
 * which by the chain rule gives
 * E'(x)=2*(sigmoid(evaluate(x))-result)*sigmoid'(evaluate(x))*evaluate'(x).
 */
double grad_calc(struct position *pos, double result) {
	memset(&trace, 0, sizeof(trace));
	int32_t eval = evaluate_classical(pos);
	eval = pos->turn == WHITE ? eval : -eval;

	double mgs = (double)trace.p / PHASE;
	double egs = (double)(PHASE - trace.p) / PHASE * trace.s / NORMAL_SCALE;

	double factor = 2 * (sigmoid(eval) - result) * sigmoid_grad(eval);

	struct parameter *param;
	if ((param = &parameters[PARAM_PIECEVALUE])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int piece = i + 1;
			double grad = factor * ((double)popcount(pos->piece[WHITE][piece]) - popcount(pos->piece[BLACK][piece]));
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
				printf("valmg: %d\n", score_mg(piece_value[i]));
				printf("valeg: %d\n", score_eg(piece_value[i]));
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
			double grad = factor * ((pos->mailbox[orient_horizontal(WHITE, square)] == WHITE_PAWN) - (pos->mailbox[orient_horizontal(BLACK, square)] == BLACK_PAWN));
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
			int num = (pos->mailbox[orient_horizontal(WHITE, square1)] == WHITE_KNIGHT) + (pos->mailbox[orient_horizontal(WHITE, square2)] == WHITE_KNIGHT) -
				  (pos->mailbox[orient_horizontal(BLACK, square1)] == BLACK_KNIGHT) - (pos->mailbox[orient_horizontal(BLACK, square2)] == BLACK_KNIGHT);
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
			int num = (pos->mailbox[orient_horizontal(WHITE, square1)] == WHITE_BISHOP) + (pos->mailbox[orient_horizontal(WHITE, square2)] == WHITE_BISHOP) -
				  (pos->mailbox[orient_horizontal(BLACK, square1)] == BLACK_BISHOP) - (pos->mailbox[orient_horizontal(BLACK, square2)] == BLACK_BISHOP);
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
			int num = (pos->mailbox[orient_horizontal(WHITE, square1)] == WHITE_ROOK) + (pos->mailbox[orient_horizontal(WHITE, square2)] == WHITE_ROOK) -
				  (pos->mailbox[orient_horizontal(BLACK, square1)] == BLACK_ROOK) - (pos->mailbox[orient_horizontal(BLACK, square2)] == BLACK_ROOK);
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
			int num = (pos->mailbox[orient_horizontal(WHITE, square1)] == WHITE_QUEEN) + (pos->mailbox[orient_horizontal(WHITE, square2)] == WHITE_QUEEN) -
				  (pos->mailbox[orient_horizontal(BLACK, square1)] == BLACK_QUEEN) - (pos->mailbox[orient_horizontal(BLACK, square2)] == BLACK_QUEEN);
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
			int num = (pos->mailbox[orient_horizontal(WHITE, square1)] == WHITE_KING) + (pos->mailbox[orient_horizontal(WHITE, square2)] == WHITE_KING) -
				  (pos->mailbox[orient_horizontal(BLACK, square1)] == BLACK_KING) - (pos->mailbox[orient_horizontal(BLACK, square2)] == BLACK_KING);
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
			double dEdm = trace.mobility[WHITE][KNIGHT - 2][i] - trace.mobility[BLACK][KNIGHT - 2][i];
			double grad = factor * dEdm;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_MOBILITYBISHOP])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdm = trace.mobility[WHITE][BISHOP - 2][i] - trace.mobility[BLACK][BISHOP - 2][i];
			double grad = factor * dEdm;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_MOBILITYROOK])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdm = trace.mobility[WHITE][ROOK - 2][i] - trace.mobility[BLACK][ROOK - 2][i];
			double grad = factor * dEdm;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_MOBILITYQUEEN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdm = trace.mobility[WHITE][QUEEN - 2][i] - trace.mobility[BLACK][QUEEN - 2][i];
			double grad = factor * dEdm;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PAWNSHELTER])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdp = trace.pawn_shelter[WHITE][i] - trace.pawn_shelter[BLACK][i];
			double grad = factor * dEdp;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_UNBLOCKEDSTORM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdu = trace.unblocked_storm[WHITE][i] - trace.unblocked_storm[BLACK][i];
			double grad = factor * dEdu;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_UNBLOCKABLESTORM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdb = trace.unblockable_storm[WHITE][i] - trace.unblockable_storm[BLACK][i];
			double grad = factor * dEdb;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_BLOCKEDSTORM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdb = trace.blocked_storm[WHITE][i] - trace.blocked_storm[BLACK][i];
			double grad = factor * dEdb;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_KINGONOPENFILE])->tune == TUNE_YES) {
		double dEdx = trace.king_on_open_file[WHITE] - trace.king_on_open_file[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTOUTPOST])->tune == TUNE_YES) {
		double dEdx = trace.knight_outpost[WHITE] - trace.knight_outpost[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTOUTPOSTATTACK])->tune == TUNE_YES) {
		double dEdx = trace.knight_outpost_attack[WHITE] - trace.knight_outpost_attack[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPOUTPOST])->tune == TUNE_YES) {
		double dEdx = trace.bishop_outpost[WHITE] - trace.bishop_outpost[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPOUTPOSTATTACK])->tune == TUNE_YES) {
		double dEdx = trace.bishop_outpost_attack[WHITE] - trace.bishop_outpost_attack[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPLONGDIAGONAL])->tune == TUNE_YES) {
		double dEdx = trace.bishop_long_diagonal[WHITE] - trace.bishop_long_diagonal[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTBEHINDPAWN])->tune == TUNE_YES) {
		double dEdx = trace.knight_behind_pawn[WHITE] - trace.knight_behind_pawn[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPBEHINDPAWN])->tune == TUNE_YES) {
		double dEdx = trace.bishop_behind_pawn[WHITE] - trace.bishop_behind_pawn[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_DEFENDEDKNIGHT])->tune == TUNE_YES) {
		double dEdx = trace.defended_knight[WHITE] - trace.defended_knight[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_DEFENDEDBISHOP])->tune == TUNE_YES) {
		double dEdx = trace.defended_bishop[WHITE] - trace.defended_bishop[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTFARFROMKING])->tune == TUNE_YES) {
		double dEdx = trace.knight_far_from_king[WHITE] - trace.knight_far_from_king[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPFARFROMKING])->tune == TUNE_YES) {
		double dEdx = trace.bishop_far_from_king[WHITE] - trace.bishop_far_from_king[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTPAIR])->tune == TUNE_YES) {
		double dEdx = trace.knight_pair[WHITE] - trace.knight_pair[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPPAIR])->tune == TUNE_YES) {
		double dEdx = trace.bishop_pair[WHITE] - trace.bishop_pair[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKPAIR])->tune == TUNE_YES) {
		double dEdx = trace.rook_pair[WHITE] - trace.rook_pair[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_PAWNBLOCKINGBISHOP])->tune == TUNE_YES) {
		double dEdx = trace.pawn_blocking_bishop[WHITE] - trace.pawn_blocking_bishop[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKOPEN])->tune == TUNE_YES) {
		double dEdx = trace.rook_open[WHITE] - trace.rook_open[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKSEMI])->tune == TUNE_YES) {
		double dEdx = trace.rook_semi[WHITE] - trace.rook_semi[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKCLOSED])->tune == TUNE_YES) {
		double dEdx = trace.rook_closed[WHITE] - trace.rook_closed[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKBLOCKED])->tune == TUNE_YES) {
		double dEdx = trace.rook_blocked[WHITE] - trace.rook_blocked[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BADQUEEN])->tune == TUNE_YES) {
		double dEdx = trace.bad_queen[WHITE] - trace.bad_queen[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
#if 0
		if (grad) {
			char fen[128];
			print_position(pos, 0);
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
		double dEdx = trace.king_attack_pawn[WHITE] - trace.king_attack_pawn[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KINGDEFENDPAWN])->tune == TUNE_YES) {
		double dEdx = trace.king_defend_pawn[WHITE] - trace.king_defend_pawn[BLACK];
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
		double dEdx = trace.pawn_threat[WHITE] - trace.pawn_threat[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_PUSHTHREAT])->tune == TUNE_YES) {
		double dEdx = trace.push_threat[WHITE] - trace.push_threat[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_MINORTHREAT])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.minor_threat[WHITE][i] - trace.minor_threat[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			if (grad) {
				char fen[128];
				print_position(pos, 0);
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
			double dEdx = trace.rook_threat[WHITE][i] - trace.rook_threat[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}

	/* We want to calculate dE/dw=dE/dr*dr/dk*dk/dw where r is the rectified
	 * king danger r=max(k, 0).
	 */
	if ((param = &parameters[PARAM_WEAKSQUARES])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = max(trace.king_danger[color], 0);
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
			print_bitboard(ei.weak_squares[BLACK]);
			print_bitboard(ei.weak_squares[WHITE]);
			printf("grad: %e\n", total_grad);
			printf("gradtest: %e\n", gradtest);
		}
#endif
	}
	if ((param = &parameters[PARAM_ENEMYNOQUEEN])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = max(trace.king_danger[color], 0);
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
			int r = max(trace.king_danger[color], 0);
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
			int r = max(trace.king_danger[color], 0);
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
			int r = max(trace.king_danger[color], 0);
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
			int r = max(trace.king_danger[color], 0);
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
			int r = max(trace.king_danger[color], 0);
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
				int r = max(trace.king_danger[color], 0);
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
	 * p=(clamp(material, p_m, p_M)-p_m)/(p_M-p_m). It is not differentiable
	 * but we try a difference quotient.
	 */
	if ((param = &parameters[PARAM_PHASEMAX])->tune == TUNE_YES) {
		double dEdp = score_mg(trace.eval) - score_eg(trace.eval);
		int p_M2 = phase_max + 1;
		int p_M1 = phase_max - 1;
		int p_m = phase_min;
		double dpdp_M = ((double)clamp(trace.material, p_m, p_M2) - p_m) / (p_M2 - p_m) -
				((double)clamp(trace.material, p_m, p_M1) - p_m) / (p_M1 - p_m);
		dpdp_M /= p_M2 - p_M1;
		double grad = factor * dEdp * dpdp_M;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEMIN])->tune == TUNE_YES) {
		double dEdp = score_mg(trace.eval) - score_eg(trace.eval);
		int p_m2 = phase_min + 1;
		int p_m1 = phase_min - 1;
		int p_M = phase_max;
		double dpdp_m = ((double)clamp(trace.material, p_m2, p_M) - p_m2) / (p_M - p_m2) -
				((double)clamp(trace.material, p_m1, p_M) - p_m1) / (p_M - p_m1);
		dpdp_m /= p_m2 - p_m1;
		double grad = factor * dEdp * dpdp_m;
		param->grad[0] += grad;
	}
	/* Same idea as before. */
	if ((param = &parameters[PARAM_PHASEKNIGHT])->tune == TUNE_YES) {
		double dEdp = score_mg(trace.eval) - score_eg(trace.eval);
		int num_knights = popcount(pos->piece[WHITE][KNIGHT] | pos->piece[BLACK][KNIGHT]);
		int m2 = trace.material + num_knights * 1;
		int m1 = trace.material - num_knights * 1;
		int p_M = phase_max;
		int p_m = phase_min;
		double dpdm = ((double)clamp(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)clamp(m1, p_m, p_M) - p_m) / (p_M - p_m);
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
		double dEdp = score_mg(trace.eval) - score_eg(trace.eval);
		int num_bishops = popcount(pos->piece[WHITE][BISHOP] | pos->piece[BLACK][BISHOP]);
		int m2 = trace.material + num_bishops * 1;
		int m1 = trace.material - num_bishops * 1;
		int p_M = phase_max;
		int p_m = phase_min;
		double dpdm = ((double)clamp(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)clamp(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEROOK])->tune == TUNE_YES) {
		double dEdp = score_mg(trace.eval) - score_eg(trace.eval);
		int num_rooks = popcount(pos->piece[WHITE][ROOK] | pos->piece[BLACK][ROOK]);
		int m2 = trace.material + num_rooks * 1;
		int m1 = trace.material - num_rooks * 1;
		int p_M = phase_max;
		int p_m = phase_min;
		double dpdm = ((double)clamp(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)clamp(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEQUEEN])->tune == TUNE_YES) {
		double dEdp = score_mg(trace.eval) - score_eg(trace.eval);
		int num_queens = popcount(pos->piece[WHITE][QUEEN] | pos->piece[BLACK][QUEEN]);
		int m2 = trace.material + num_queens * 1;
		int m1 = trace.material - num_queens * 1;
		int p_M = phase_max;
		int p_m = phase_min;
		double dpdm = ((double)clamp(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)clamp(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}

	if ((param = &parameters[PARAM_SUPPORTEDPAWN])->tune == TUNE_YES) {
		double dEdx = trace.supported_pawn[WHITE] - trace.supported_pawn[BLACK];
		double grad = factor * dEdx;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BACKWARDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.backward_pawn[WHITE][i] - trace.backward_pawn[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_ISOLATEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.isolated_pawn[WHITE][i] - trace.isolated_pawn[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_DOUBLEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.doubled_pawn[WHITE][i] - trace.doubled_pawn[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_CONNECTEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.connected_pawn[WHITE][i] - trace.connected_pawn[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PASSEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.passed_pawn[WHITE][i] - trace.passed_pawn[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PASSEDBLOCKED])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.passed_blocked[WHITE][i] - trace.passed_blocked[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_PASSEDFILE])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.passed_file[WHITE][i] - trace.passed_file[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_DISTANCEUS])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.distance_us[WHITE][i] - trace.distance_us[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_DISTANCETHEM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdx = trace.distance_them[WHITE][i] - trace.distance_them[BLACK][i];
			double grad = factor * dEdx;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			if (grad) {
				char fen[128];
				print_position(pos, 0);
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
	char result = RESULT_UNKNOWN;
	while (actual_size < BATCH_SIZE) {
		move_t move = 0;
		read_move(f, &move);
		if (move) {
			do_move(pos, &move);
		}
		else {
			read_position(f, pos);
			read_result(f, &result);
		}

		int32_t eval = 0;
		unsigned char flag;
		read_eval(f, &eval);
		read_flag(f, &flag);
		if (feof(f))
			break;

		int skip = (result == RESULT_UNKNOWN) || (eval == VALUE_NONE) || gbernoulli(0.9);
		if (skip)
			continue;

		double score = result == RESULT_WIN ? 1.0 : result == RESULT_DRAW ? 0.5 : 0.0;
		grad_calc(pos, score);

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

	static struct option opts[] = {
		{ "K",       required_argument, NULL, 'K' },
		{ "beta1",   required_argument, NULL, '1' },
		{ "beta2",   required_argument, NULL, '2' },
		{ "epsilon", required_argument, NULL, 'e' },
		{ "alpha",   required_argument, NULL, 'a' },
		{ "decay",   required_argument, NULL, 'd' },
		{ NULL,      0,                 NULL,  0  },
	};
	char *endptr;
	int c, option_index = 0;
	int error = 0;
	while ((c = getopt_long(argc, argv, "K:1:2:e:a:d:", opts, &option_index)) != -1) {
		double *ptr = NULL;
		switch (c) {
		case 'K':
			ptr = &K;
			break;
		case '1':
			ptr = &beta1;
			break;
		case '2':
			ptr = &beta2;
			break;
		case 'e':
			ptr = &epsilon;
			break;
		case 'a':
			ptr = &alpha;
			break;
		case 'd':
			ptr = &weight_decay;
			break;
		default:
			error = 1;
		}
		if (!error) {
			errno = 0;
			*ptr = strtod(optarg, &endptr);
			if (*endptr != '\0' || errno || *ptr < 1.0e-11)
				return 4;
		}
	}
	if (error)
		return 1;
	if (optind >= argc) {
		fprintf(stderr, "usage: %s infile\n", argv[0]);
		return 3;
	}
	char *path = argv[optind];
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "failed to open file \"%s\"\n", path);
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
