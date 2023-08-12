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
	PARAM_UNBLOCKEDSTORM,
	PARAM_BLOCKEDSTORM,

	PARAM_KINGONOPENFILE,
	PARAM_OUTPOSTBONUS,
	PARAM_OUTPOSTATTACK,
	PARAM_MINORBEHINDPAWN,
	PARAM_KNIGHTFARFROMKING,
	PARAM_BISHOPFARFROMKING,
	PARAM_BISHOPPAIR,
	PARAM_PAWNONBISHOPSQUARE,
	PARAM_ROOKONOPENFILE,
	PARAM_BLOCKEDROOK,
	PARAM_DEFENDEDMINOR,
	PARAM_TEMPOBONUS,

	PARAM_UNDEVELOPEDPIECE,

	PARAM_BACKWARDPAWN,
	PARAM_SUPPORTEDPAWN,
	PARAM_ISOLATEDPAWN,
	PARAM_DOUBLEDPAWN,
	PARAM_CONNECTEDPAWNS,
	PARAM_PASSEDPAWN,
	PARAM_PASSEDFILE,

	PARAM_WEAKSQUARESDANGER,
	PARAM_ENEMYNOQUEENBONUS,
	PARAM_KNIGHTATTACKDANGER,
	PARAM_BISHOPATTACKDANGER,
	PARAM_ROOKATTACKDANGER,
	PARAM_QUEENATTACKDANGER,
	PARAM_KINGDANGER,

	PARAM_PHASEMAXMATERIAL,
	PARAM_PHASEMINMATERIAL,
	PARAM_PHASEKNIGHT,
	PARAM_PHASEBISHOP,
	PARAM_PHASEROOK,
	PARAM_PHASEQUEEN,
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
const double epsilon = 1e-7;
const double learning_rate = 1e-3;
const double weight_decay = 0;
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
	PARAMETER(&white_psqtable[0][8],     48, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&white_psqtable[1][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&white_psqtable[2][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&white_psqtable[3][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&white_psqtable[4][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&white_psqtable[5][0],     32, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),

	PARAMETER(&mobility_bonus[0][0],      9, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&mobility_bonus[1][0],     14, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&mobility_bonus[2][0],     15, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&mobility_bonus[3][0],     28, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),

	PARAMETER(&pawn_shelter[0],          28, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&unblocked_storm[0],       28, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&blocked_storm[0],          7, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),

	PARAMETER(&king_on_open_file,         1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&outpost_bonus,             1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&outpost_attack,            1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&minor_behind_pawn,         1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&knight_far_from_king,      1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&bishop_far_from_king,      1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&bishop_pair,               1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&pawn_on_bishop_square,     1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&rook_on_open_file,         1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&blocked_rook,              1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&defended_minor,            1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&tempo_bonus,               1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_NO),

	PARAMETER(&undeveloped_piece,         1, TYPE_INT,     WEIGHTDECAY_YES, TUNE_NO),

	PARAMETER(&backward_pawn,             1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&supported_pawn,            1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&isolated_pawn,             1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&doubled_pawn,              1, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&connected_pawns[0],        7, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&passed_pawn[0],            7, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),
	PARAMETER(&passed_file[0],            4, TYPE_MEVALUE, WEIGHTDECAY_YES, TUNE_YES),

	PARAMETER(&weak_squares_danger,       1, TYPE_INT,     WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&enemy_no_queen_bonus,      1, TYPE_INT,     WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&knight_attack_danger,      1, TYPE_INT,     WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&bishop_attack_danger,      1, TYPE_INT,     WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&rook_attack_danger,        1, TYPE_INT,     WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&queen_attack_danger,       1, TYPE_INT,     WEIGHTDECAY_YES, TUNE_NO),
	PARAMETER(&king_danger,               1, TYPE_INT,     WEIGHTDECAY_YES, TUNE_NO),

	PARAMETER(&phase_max_material,        1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_NO),
	PARAMETER(&phase_min_material,        1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_NO),
	PARAMETER(&phase_knight,              1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_NO),
	PARAMETER(&phase_bishop,              1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_NO),
	PARAMETER(&phase_rook,                1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_NO),
	PARAMETER(&phase_queen,               1, TYPE_INT,     WEIGHTDECAY_NO,  TUNE_NO),
};

void mevalue_print(mevalue eval) {
	printf("S(%3d,%3d), ", mevalue_mg(eval), mevalue_eg(eval));;
}

void parameters_print() {
	struct parameter *param;
	for (int i = 0; i < 5; i++) {
		printf("S(%4d,%4d), ", mevalue_mg(piece_value[i]), mevalue_eg(piece_value[i]));
	}
	printf("\n");
#if 0
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
	printf("mevalue mobility_bonus[4][28] = {\n\t{\n\t\t");
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
	printf("\n}\n\n");
	param = &parameters[PARAM_UNBLOCKEDSTORM];
	printf("mevalue unblocked_storm[28] = {");
	for (size_t i = 0; i < param->size; i++) {
		if (i % 7 == 0)
			printf("\n\t");
		mevalue_print(param->ptr[i]);
	}
	printf("\n}\n\n");
	param = &parameters[PARAM_BLOCKEDSTORM];
	printf("mevalue blocked_storm[7] = {\n\t");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	printf("\n}\n");
	param = &parameters[PARAM_KINGONOPENFILE];
	printf("\nmevalue king_on_open_file     = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_OUTPOSTBONUS];
	printf("\nmevalue outpost_bonus         = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_OUTPOSTATTACK];
	printf("\nmevalue outpost_attack        = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_MINORBEHINDPAWN];
	printf("\nmevalue minor_behind_pawn     = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_KNIGHTFARFROMKING];
	printf("\nmevalue knight_far_from_king  = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPFARFROMKING];
	printf("\nmevalue bishop_far_from_king  = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BISHOPPAIR];
	printf("\nmevalue bishop_pair           = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_PAWNONBISHOPSQUARE];
	printf("\nmevalue pawn_on_bishop_square = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_ROOKONOPENFILE];
	printf("\nmevalue rook_on_open_file     = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_BLOCKEDROOK];
	printf("\nmevalue blocked_rook          = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_DEFENDEDMINOR];
	printf("\nmevalue defended_minor        = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_TEMPOBONUS];
	printf("\nmevalue tempo_bonus           = ");
	mevalue_print(param->ptr[0]);
	printf("\n\n");
	param = &parameters[PARAM_UNDEVELOPEDPIECE];
	printf("int undeveloped_piece         = %d;\n\n", param->ptr[0]);
	param = &parameters[PARAM_WEAKSQUARESDANGER];
	printf("int weak_squares_danger       = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_ENEMYNOQUEENBONUS];
	printf("int enemy_no_queen_bonus      = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_KNIGHTATTACKDANGER];
	printf("int knight_attack_danger      = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_BISHOPATTACKDANGER];
	printf("int bishop_attack_danger      = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_ROOKATTACKDANGER];
	printf("int rook_attack_danger        = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_QUEENATTACKDANGER];
	printf("int queen_attack_danger       = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_KINGDANGER];
	printf("int king_danger               = %d;\n", param->ptr[0]);
	printf("\n");
	param = &parameters[PARAM_TEMPOBONUS];
	printf("int tempo_bonus               = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEMAXMATERIAL];
	printf("int phase_max_material        = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEMINMATERIAL];
	printf("int phase_min_material        = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEKNIGHT];
	printf("int phase_knight              = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEBISHOP];
	printf("int phase_bishop              = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEROOK];
	printf("int phase_rook                = %d;\n", param->ptr[0]);
	param = &parameters[PARAM_PHASEQUEEN];
	printf("int phase_queen               = %d;\n", param->ptr[0]);
#endif
	printf("\nmevalue backward_pawn  = ");
	param = &parameters[PARAM_BACKWARDPAWN];
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_SUPPORTEDPAWN];
	printf("\nmevalue supported_pawn = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_ISOLATEDPAWN];
	printf("\nmevalue isolated_pawn  = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_DOUBLEDPAWN];
	printf("\nmevalue doubled_pawn   = ");
	mevalue_print(param->ptr[0]);
	param = &parameters[PARAM_CONNECTEDPAWNS];
	printf("\nmevalue connected_pawns = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_PASSEDPAWN];
	printf("\nmevalue passed_pawn    = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	param = &parameters[PARAM_PASSEDFILE];
	printf("}\nmevalue passed_file   = { ");
	for (size_t i = 0; i < param->size; i++)
		mevalue_print(param->ptr[i]);
	printf("};\n\n");
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
		value[i] -= learning_rate * m_hat / (sqrt(v_hat) + epsilon) + (weight_decay_enabled == WEIGHTDECAY_YES) * weight_decay * value[i];
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
	struct evaluationinfo ei = { 0 };
	int32_t eval = evaluate_classical_ei(pos, &ei);
	eval = pos->turn == white ? eval : -eval;

	int32_t p = phase(&ei);
	int strong_side = mevalue_eg(ei.eval) > 0;
	int32_t s = scale(pos, &ei, strong_side);

	double mgs = (double)p / PHASE;
	double egs = (double)(PHASE - p) / PHASE * s / NORMAL_SCALE;

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
			int j = i + 8;
			int x = j % 8;
			int y = j / 8;
			int square = x + 8 * (7 - y);
			double grad = factor * ((pos->mailbox[orient_horizontal(white, square)] == white_pawn) - (pos->mailbox[orient_horizontal(black, square)] == black_pawn));
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			white_psqtable[0][j] += S(100, 0);
			tables_init();
			int16_t new_eval = evaluate_classical(pos);
			new_eval = pos->turn == white ? new_eval : -new_eval;
			white_psqtable[0][j] -= S(100, 0);
			tables_init();
			double gradtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);


			if ((gradmg == 0) ^ (gradtest == 0)) {
				printf("square: %d (%ld)\n", square, i);
				printf("gradmg: %e\n", gradmg);
				printf("gradeg: %e\n", gradeg);
				printf("gradtest: %e\n", gradtest);
				exit(2);
			}
#endif
		}
	}
	if ((param = &parameters[PARAM_PSQTKNIGHT])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			int x = i % 4;
			int y = i / 4;
			int square1 = x + 8 * (7 - y);
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
			int x = i % 4;
			int y = i / 4;
			int square1 = x + 8 * (7 - y);
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
			int x = i % 4;
			int y = i / 4;
			int square1 = x + 8 * (7 - y);
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
			int x = i % 4;
			int y = i / 4;
			int square1 = x + 8 * (7 - y);
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
			int x = i % 4;
			int y = i / 4;
			int square1 = x + 8 * (7 - y);
			int square2 = orient_vertical(1, square1);
			int num = (pos->mailbox[orient_horizontal(white, square1)] == white_king) + (pos->mailbox[orient_horizontal(white, square2)] == white_king) -
				  (pos->mailbox[orient_horizontal(black, square1)] == black_king) - (pos->mailbox[orient_horizontal(black, square2)] == black_king);
			double grad = factor * num;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_MOBILITYKNIGHT])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			uint64_t b = pos->piece[color][knight];
			while (b) {
				int square = ctz(b);
				uint64_t attacks = knight_attacks(square, 0);
				size_t mobility = popcount(attacks & ei.mobility_squares[color]);
				for (size_t i = 0; i < param->size; i++) {
					if (mobility == i) {
						double grad = factor * (2 * color - 1);
						double gradmg = mgs * grad;
						double gradeg = egs * grad;
						param->grad[2 * i + mg] += gradmg;
						param->grad[2 * i + eg] += gradeg;
					}
				}
				
				b = clear_ls1b(b);
			}
		}
	}
	if ((param = &parameters[PARAM_MOBILITYBISHOP])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			uint64_t b = pos->piece[color][bishop];
			while (b) {
				int square = ctz(b);
				uint64_t attacks = bishop_attacks(square, 0, all_pieces(pos));
				size_t mobility = popcount(attacks & ei.mobility_squares[color]);
				for (size_t i = 0; i < param->size; i++) {
					if (mobility == i) {
						double grad = factor * (2 * color - 1);
						double gradmg = mgs * grad;
						double gradeg = egs * grad;
						param->grad[2 * i + mg] += gradmg;
						param->grad[2 * i + eg] += gradeg;
					}
				}
				
				b = clear_ls1b(b);
			}
		}
	}
	if ((param = &parameters[PARAM_MOBILITYROOK])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			uint64_t b = pos->piece[color][rook];
			while (b) {
				int square = ctz(b);
				uint64_t attacks = rook_attacks(square, 0, all_pieces(pos));
				size_t mobility = popcount(attacks & ei.mobility_squares[color]);
				for (size_t i = 0; i < param->size; i++) {
					if (mobility == i) {
						double grad = factor * (2 * color - 1);
						double gradmg = mgs * grad;
						double gradeg = egs * grad;
						param->grad[2 * i + mg] += gradmg;
						param->grad[2 * i + eg] += gradeg;
					}
				}
				
				b = clear_ls1b(b);
			}
		}
	}
	if ((param = &parameters[PARAM_MOBILITYQUEEN])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			uint64_t b = pos->piece[color][queen];
			while (b) {
				int square = ctz(b);
				uint64_t attacks = queen_attacks(square, 0, all_pieces(pos));
				size_t mobility = popcount(attacks & ei.mobility_squares[color]);
				for (size_t i = 0; i < param->size; i++) {
					if (mobility == i) {
						double grad = factor * (2 * color - 1);
						double gradmg = mgs * grad;
						double gradeg = egs * grad;
						param->grad[2 * i + mg] += gradmg;
						param->grad[2 * i + eg] += gradeg;
					}
				}
				
				b = clear_ls1b(b);
			}
		}
	}
	if ((param = &parameters[PARAM_PAWNSHELTER])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdp = ei.pawn_shelter[white][i] - ei.pawn_shelter[black][i];
			double grad = factor * dEdp;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_UNBLOCKEDSTORM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdu = ei.unblocked_storm[white][i] - ei.unblocked_storm[black][i];
			double grad = factor * dEdu;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_BLOCKEDSTORM])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdb = ei.blocked_storm[white][i] - ei.blocked_storm[black][i];
			double grad = factor * dEdb;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	if ((param = &parameters[PARAM_KINGONOPENFILE])->tune == TUNE_YES) {
		double dEdk = ei.king_on_open_file[white] - ei.king_on_open_file[black];
		double grad = factor * dEdk;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_OUTPOSTBONUS])->tune == TUNE_YES) {
		double dEdo = ei.outpost_bonus[white] - ei.outpost_bonus[black];
		double grad = factor * dEdo;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_OUTPOSTATTACK])->tune == TUNE_YES) {
		double dEdo = ei.outpost_attack[white] - ei.outpost_attack[black];
		double grad = factor * dEdo;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_MINORBEHINDPAWN])->tune == TUNE_YES) {
		double dEdm = ei.minor_behind_pawn[white] - ei.minor_behind_pawn[black];
		double grad = factor * dEdm;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_KNIGHTFARFROMKING])->tune == TUNE_YES) {
		double dEdk = ei.knight_far_from_king[white] - ei.knight_far_from_king[black];
		double grad = factor * dEdk;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPFARFROMKING])->tune == TUNE_YES) {
		double dEdb = ei.bishop_far_from_king[white] - ei.bishop_far_from_king[black];
		double grad = factor * dEdb;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BISHOPPAIR])->tune == TUNE_YES) {
		double dEdb = ei.bishop_pair[white] - ei.bishop_pair[black];
		double grad = factor * dEdb;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_PAWNONBISHOPSQUARE])->tune == TUNE_YES) {
		double dEdp = ei.pawn_on_bishop_square[white] - ei.pawn_on_bishop_square[black];
		double grad = factor * dEdp;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_ROOKONOPENFILE])->tune == TUNE_YES) {
		double dEdr = ei.rook_on_open_file[white] - ei.rook_on_open_file[black];
		double grad = factor * dEdr;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BLOCKEDROOK])->tune == TUNE_YES) {
		double dEdb = ei.blocked_rook[white] - ei.blocked_rook[black];
		double grad = factor * dEdb;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_UNDEVELOPEDPIECE])->tune == TUNE_YES) {
		double dEdu = ei.undeveloped_piece[white] - ei.undeveloped_piece[black];
		double grad = factor * mgs * dEdu;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_DEFENDEDMINOR])->tune == TUNE_YES) {
		double dEdd = ei.defended_minor[white] - ei.defended_minor[black];
		double grad = factor * dEdd;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_BACKWARDPAWN])->tune == TUNE_YES) {
		double dEdp = ei.backward_pawn[white] - ei.backward_pawn[black];
		double grad = factor * dEdp;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
#if 0
		if (gradmg || gradeg) {
			print_position(pos, 0);
			char fen[128];
			printf("%s\n", pos_to_fen(fen, pos));
			printf("factor: %e\n", factor);
			printf("gradmg: %e\n", gradmg);
			printf("gradeg: %e\n", gradeg);
			printf("p: %d\n", p);
			printf("s: %d\n", s);
			printf("mgs: %f\n", mgs);
			printf("egs: %f\n", egs);
			printf("eval: %d\n", eval);
			printf("result: %f\n", result);
			printf("sigmoid: %f\n", sigmoid(eval));
			printf("%d\n", ei.backward_pawn[white]);
			printf("%d\n", ei.backward_pawn[black]);
		}
#endif
	}
	if ((param = &parameters[PARAM_SUPPORTEDPAWN])->tune == TUNE_YES) {
		double dEdp = ei.supported_pawn[white] - ei.supported_pawn[black];
		double grad = factor * dEdp;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_PASSEDPAWN])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdp = ei.passed_pawn[white][i] - ei.passed_pawn[black][i];
			double grad = factor * dEdp;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			if (gradmg || gradeg) {
				print_position(pos, 0);
				printf("y: %ld\n", i);
				printf("gradmg: %f\n", gradmg);
				printf("gradeg: %f\n", gradeg);
				printf("result: %f\n", result);
				printf("eval: %d\n", eval);
				printf("sigmoid(eval): %f\n", sigmoid(eval));
			}
#endif
		}
	}
	if ((param = &parameters[PARAM_PASSEDFILE])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdp = ei.passed_file[white][i] - ei.passed_file[black][i];
			double grad = factor * dEdp;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
#if 0
			if (gradtest[0] + gradtest[1] != 0) {
				print_position(pos, 0);
				printf("gradmgb: %f\n", gradtest[0]);
				printf("gradegb: %f\n", gradtest[1]);
				printf("gradmgw: %f\n", gradtest[2]);
				printf("gradegw: %f\n", gradtest[3]);
				printf("%d\n", ei.passed_file[0]);
				printf("%d\n", ei.passed_file[1]);
				printf("result: %f\n", result);
			}
#endif
		}
	}
	if ((param = &parameters[PARAM_ISOLATEDPAWN])->tune == TUNE_YES) {
		double dEdp = ei.isolated_pawn[white] - ei.isolated_pawn[black];
		double grad = factor * dEdp;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_DOUBLEDPAWN])->tune == TUNE_YES) {
		double dEdp = ei.doubled_pawn[white] - ei.doubled_pawn[black];
		double grad = factor * dEdp;
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
	}
	if ((param = &parameters[PARAM_CONNECTEDPAWNS])->tune == TUNE_YES) {
		for (size_t i = 0; i < param->size; i++) {
			double dEdp = ei.connected_pawns[white][i] - ei.connected_pawns[black][i];
			double grad = factor * dEdp;
			double gradmg = mgs * grad;
			double gradeg = egs * grad;
			param->grad[2 * i + mg] += gradmg;
			param->grad[2 * i + eg] += gradeg;
		}
	}
	/* We want to calculate dE/dw=dE/dr*dr/dk*dk/dw where r is the rectified
	 * king danger r=MAX(k, 0).
	 */
	if ((param = &parameters[PARAM_WEAKSQUARESDANGER])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(ei.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = ei.king_danger[color] >= 0;
			double dkdw = popcount(ei.weak_squares[color] & ei.king_ring[color]);
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
	if ((param = &parameters[PARAM_ENEMYNOQUEENBONUS])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(ei.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = ei.king_danger[color] >= 0;
			double dkdw = -!pos->piece[1 - color][queen];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
#if 0
		enemy_no_queen_bonus += 1;
		int16_t new_eval = evaluate_classical(pos);
		new_eval = pos->turn == white ? new_eval : -new_eval;
		enemy_no_queen_bonus -= 1;
		double gradtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);
		total_total_grad += total_grad;
		if (total_grad /*|| (total_grad == 0) ^ (gradtest == 0)*/) {
			printf("grad: %e\n", total_grad);
			printf("gradtest: %e\n", gradtest);
			printf("total: %e\n", total_total_grad);
		}
#endif
	}
	if ((param = &parameters[PARAM_KNIGHTATTACKDANGER])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(ei.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = ei.king_danger[color] >= 0;
			double dkdw = ei.king_attack_units[1 - color][knight];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
#if 0
		if (gradtest[0] + gradtest[1] != 0) {
			print_position(pos, 0);
			printf("gradb: %f\n", gradtest[0]);
			printf("gradw: %f\n", gradtest[1]);
			print_bitboard(ei.king_ring[0]);
			print_bitboard(ei.king_ring[1]);
			printf("%d\n", ei.king_attack_units[0][knight]);
			printf("%d\n", ei.king_attack_units[1][knight]);
			printf("result: %f\n", result);
		}
#endif
	}
	if ((param = &parameters[PARAM_BISHOPATTACKDANGER])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(ei.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = ei.king_danger[color] >= 0;
			double dkdw = ei.king_attack_units[1 - color][bishop];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_ROOKATTACKDANGER])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(ei.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = ei.king_danger[color] >= 0;
			double dkdw = ei.king_attack_units[1 - color][rook];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_QUEENATTACKDANGER])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(ei.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = ei.king_danger[color] >= 0;
			double dkdw = ei.king_attack_units[1 - color][queen];
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
	}
	if ((param = &parameters[PARAM_KINGDANGER])->tune == TUNE_YES) {
		for (int color = 0; color <= 1; color++) {
			int r = MAX(ei.king_danger[color], 0);
			double dEdr = -mgs * 2 * r / 2048 - egs * 1 / 8;
			double drdk = ei.king_danger[color] >= 0;
			double dkdw = 1;
			double dEdw = dEdr * drdk * dkdw;
			double grad = factor * dEdw * (2 * color - 1);
			param->grad[0] += grad;
		}
#if 0
		if (gradtest[0] + gradtest[1] != 0) {
			print_position(pos, 0);
			printf("gradb: %f\n", gradtest[0]);
			printf("gradw: %f\n", gradtest[1]);
			//print_bitboard(ei.king_ring[0]);
			//print_bitboard(ei.king_ring[1]);
			printf("%d\n", ei.king_danger[black]);
			printf("%d\n", ei.king_danger[white]);
			printf("%d\n", eval);
			printf("sigmoid: %f\n", sigmoid(eval));
			printf("result: %f\n", result);
		}
#endif
	}
	if ((param = &parameters[PARAM_TEMPOBONUS])->tune == TUNE_YES) {
		double grad = factor * (2 * pos->turn - 1);
		double gradmg = mgs * grad;
		double gradeg = egs * grad;
		param->grad[mg] += gradmg;
		param->grad[eg] += gradeg;
#if 0
		tempo_bonus += 1;
		int16_t new_eval = evaluate_classical(pos);
		new_eval = pos->turn == white ? new_eval : -new_eval;
		tempo_bonus -= 1;
		double gradtest = (sigmoid(new_eval) - result) * (sigmoid(new_eval) - result) - (sigmoid(eval) - result) * (sigmoid(eval) - result);

		if (1 || (grad == 0) ^ (gradtest == 0)) {
			printf("grad: %e\n", grad);
			printf("gradtest: %e\n", gradtest);
		}
#endif
	}
	/* We want to calculate dE/dp_M=dE/dp*dp/dp_M where p is the phase.
	 * E depends on p as E=p*mg+(1-p)*eg, so dE/dp=mg-eg. We now have
	 * p=(CLAMP(material, p_m, p_M)-p_m)/(p_M-p_m). It is not differentiable
	 * but we try a difference quotient.
	 */
	if ((param = &parameters[PARAM_PHASEMAXMATERIAL])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(ei.eval) - mevalue_eg(ei.eval);
		int p_M2 = phase_max_material + 1;
		int p_M1 = phase_max_material - 1;
		int p_m = phase_min_material;
		double dpdp_M = ((double)CLAMP(ei.material, p_m, p_M2) - p_m) / (p_M2 - p_m) -
				((double)CLAMP(ei.material, p_m, p_M1) - p_m) / (p_M1 - p_m);
		dpdp_M /= p_M2 - p_M1;
		double grad = factor * dEdp * dpdp_M;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEMINMATERIAL])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(ei.eval) - mevalue_eg(ei.eval);
		int p_m2 = phase_min_material + 1;
		int p_m1 = phase_min_material - 1;
		int p_M = phase_max_material;
		double dpdp_m = ((double)CLAMP(ei.material, p_m2, p_M) - p_m2) / (p_M - p_m2) -
				((double)CLAMP(ei.material, p_m1, p_M) - p_m1) / (p_M - p_m1);
		dpdp_m /= p_m2 - p_m1;
		double grad = factor * dEdp * dpdp_m;
		param->grad[0] += grad;
	}
	/* Same idea as before. */
	if ((param = &parameters[PARAM_PHASEKNIGHT])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(ei.eval) - mevalue_eg(ei.eval);
		int num_knights = popcount(pos->piece[white][knight] | pos->piece[black][knight]);
		int m2 = ei.material + num_knights * 1;
		int m1 = ei.material - num_knights * 1;
		int p_M = phase_max_material;
		int p_m = phase_min_material;
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
		double dEdp = mevalue_mg(ei.eval) - mevalue_eg(ei.eval);
		int num_bishops = popcount(pos->piece[white][bishop] | pos->piece[black][bishop]);
		int m2 = ei.material + num_bishops * 1;
		int m1 = ei.material - num_bishops * 1;
		int p_M = phase_max_material;
		int p_m = phase_min_material;
		double dpdm = ((double)CLAMP(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)CLAMP(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEROOK])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(ei.eval) - mevalue_eg(ei.eval);
		int num_rooks = popcount(pos->piece[white][rook] | pos->piece[black][rook]);
		int m2 = ei.material + num_rooks * 1;
		int m1 = ei.material - num_rooks * 1;
		int p_M = phase_max_material;
		int p_m = phase_min_material;
		double dpdm = ((double)CLAMP(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)CLAMP(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}
	if ((param = &parameters[PARAM_PHASEQUEEN])->tune == TUNE_YES) {
		double dEdp = mevalue_mg(ei.eval) - mevalue_eg(ei.eval);
		int num_queens = popcount(pos->piece[white][queen] | pos->piece[black][queen]);
		int m2 = ei.material + num_queens * 1;
		int m1 = ei.material - num_queens * 1;
		int p_M = phase_max_material;
		int p_m = phase_min_material;
		double dpdm = ((double)CLAMP(m2, p_m, p_M) - p_m) / (p_M - p_m) -
			      ((double)CLAMP(m1, p_m, p_M) - p_m) / (p_M - p_m);
		dpdm /= 1 - (-1);
		double grad = factor * dEdp * dpdm;
		param->grad[0] += grad;
	}

	return (sigmoid(eval) - result) * (sigmoid(eval) - result);
}

double error = 0;
size_t total = 0;

size_t prints = 1;

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

		error += grad_calc(pos, result);

		actual_size++;
		total++;
	}
	if (actual_size == 0) {
		parameters_print();
		printf("%f\n", error / total);
		error = 0;
		total = 0;
		return 0;
	}

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
	tables_init();
	search_init();
	moveorder_init();
	position_init();
	parameters_init();

	while (1) {
		zero_grad();
		if (!grad(f, &pos)) {
			fseek(f, 0, SEEK_SET);
			continue;
		}
		step();
	}

	fclose(f);
}
