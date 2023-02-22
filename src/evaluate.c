/* bitbit, a bitboard based chess engine written in c.  * Copyright (C) 2022 Isak Ellmer
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

#include <string.h>

#include "init.h"
#include "bitboard.h"
#include "util.h"
#include "attack_gen.h"
#include "move_gen.h"
#include "pawn.h"

int eval_table[2][13][64];

int piece_value[2][13] = { {     0,   100,   310,   325,   500,   900,     0,  -100,  -310,  -325,  -500,  -900,     0 },
                           {     0,   150,   340,   375,   580,  1050,     0,  -150,  -340,  -375,  -580, -1050,     0 } };
enum { mg, eg };

/* <https://www.chessprogramming.org/King_Safety> */
int safety_table[100] = {
	  0,   0,   1,   2,   3,   5,   7,   9,  12,  15,
	 18,  22,  26,  30,  35,  39,  44,  50,  56,  62,
	 68,  75,  82,  85,  89,  97, 105, 113, 122, 131,
	140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
	260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
	377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
	494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

/* <https://www.chessprogramming.org/Simplified_Evaluation_Function> */
int white_side_eval_table[2][6][64] = { { /* early game */
	{
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   5,  10,   0,   0,   0,
		  0,   0,   0,  15,  20,   0,   0,   0,
		  0,   0,   0,   5,  15, -18,   0,   0,
		  5,  -5, -10,   7,  -2, -13,  -7,   2,
		  1,   7,   5,  -3,  -4,  12,   9,   3,
		  0,   0,   0,   0,   0,   0,   0,   0
	}, {
		-82, -34, -31, -27, -27, -31, -34, -82,
		-34, -21, -19,  -9,  -9, -19, -21, -34,
		-29,  -4,   2,   4,   4,   2,  -4, -29,
		-25,   3,  12,  14,  14,  12,   3, -25,
		-23,   4,  16,  17,  17,  16,   4, -23,
		-11,   6,  20,  18,  18,  22,   6, -11,
		-40, -20,   1,  11,  11,   1, -20, -40,
		-76, -34, -25, -28, -28, -25, -34, -76
	}, {
		-23, -17, -15, -14, -14, -15, -17, -23,
		-18,   3,   9,   2,   2,   9,   3, -18,
		-14,   0,   3,   9,   9,   3,   0, -14,
		-13,   5,   5,  15,  15,   5,   5, -13,
		-10,  12,  11,  14,  14,  11,  12, -10,
		 -9,   9,  10,  10,  10,  10,   9,  -9,
		-13,   5,   3,   0,   0,   3,   5, -13,
		-22,  -5, -10, -11, -11, -10,  -5, -22
	}, {
		-10, -11, -10,  -8,  -8, -10, -11, -10,
		-21,  -4,  -3,  -5,  10,  10,  10, -21,
		-13,  -5,  -3,  -3,  -3,  -3,  -5, -13,
		-14,  -6,  -1,  -2,  -2,  -1,  -6, -14,
		-11,  -8,   0,   0,   0,   0,  -8, -11,
		-13, -10,   1,   3,   3,   1, -10, -13,
		 -3,   2,   4,   8,   8,   4,   2,  -3,
		 -9,  -7,   9,  13,  13,  10,  -7,  -9
	}, {
		-14,  -6,  -7,  -5,  -5,  -7,  -6, -14,
		-11,  -1,   1,   0,   0,   1,  -1, -11,
		-10,  -2,   3,   3,   3,   5,  -2, -10,
		 -5,   3,   2,   4,   4,   5,   3,  -5,
		  1,   6,   4,   5,   5,   5,   6,   1,
		-10,   5,   5,   6,   6,   5,   5, -10,
		-12,  -1,   6,   8,   8,   6,  -1, -12,
		-13,  -5,  -4,  -1,  -1,  -4,  -5, -13
	}, {
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,
		  5,   7,   0,   0,   0,   0,   6,   7,
		  8,  11,   9,  -5,   0,  -9,  11,   9
	} }, /* end game */ { {
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,
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
		-50, -40, -30, -20, -20, -30, -40, -50,
		-30, -20, -10,   0,   0, -10, -20, -30,
		-30, -10,  20,  30,  30,  20, -10, -30,
		-30, -10,  30,  40,  40,  30, -10, -30,
		-30, -10,  30,  40,  40,  30, -10, -30,
		-30, -10,  20,  30,  30,  20, -10, -30,
		-30, -30,   0,   0,   0,   0, -30, -30,
		-50, -30, -30, -30, -30, -30, -30, -50
	} }
};

int king_safety(struct position *pos, int color) {
	int king_square = ctz(pos->piece[color][king]);
	int eval = 0;
	/* half open files near king are bad */
	if (file_left(king_square) && !(file_left(king_square) & pos->piece[color][pawn]))
		eval -= 25;
	if (!(file(king_square) & pos->piece[color][pawn]))
		eval -= 35;
	if (file_right(king_square) && !(file_right(king_square) & pos->piece[color][pawn]))
		eval -= 25;

	/* safety table */
	int attack_units;
	uint64_t squares;
	attack_units = 0;
	squares = king_squares(king_square, color);
	attack_units += 5 * popcount(squares & pos->piece[1 - color][queen]);
	attack_units += 3 * popcount(squares & pos->piece[1 - color][rook]);
	attack_units += 2 * popcount(squares & (pos->piece[1 - color][bishop] | pos->piece[1 - color][knight]));
	eval -= safety_table[MIN(attack_units, 99)];

	/* own pawns close to king_square */
	eval += 7 * popcount(king_attacks(king_square, 0) & pos->piece[color][pawn]);

	/* uncastled king penalty */
	if (popcount(pos->piece[color][rook]) == 2) {
		uint64_t b = pos->piece[color][rook];
		int rook1 = ctz(b) % 8;
		b = clear_ls1b(b);
		int rook2 = ctz(b) % 8;
		if (rook1 > rook2) {
			b = rook1;
			rook1 = rook2;
			rook2 = b;
		}
		if (rook1 <= king_square % 8 && king_square % 8 <= rook2)
			eval -= 15;
	}

	return eval;
}

int16_t evaluate_knights(struct position *pos) {
	UNUSED(pos);
	int eval = 0;
	return eval;
}

int16_t evaluate_bishops(struct position *pos, int color) {
	int eval = 0;
	/* bishop pair */
	if (pos->piece[color][bishop] && clear_ls1b(pos->piece[color][bishop]))
		eval += 30;
	return eval;
}

int16_t evaluate_rooks(struct position *pos, int color) {
	int eval = 0, square;
	uint64_t b;
	b = pos->piece[color][rook];
	while (b) {
		square = ctz(b);
		eval += 20 - 10 * popcount((pos->piece[white][pawn] | pos->piece[black][pawn]) & file(square % 8));
		b = clear_ls1b(b);
	}

	/* connected rooks (assuming max 2 rooks) */
	if (popcount(pos->piece[color][rook]) == 2) {
		square = ctz(pos->piece[color][rook]);
		if (pos->piece[color][rook] & rook_attacks(square, 0, pos->piece[white][all] | pos->piece[black][all]))
			eval += 30;
	}

	return eval;
}

int16_t evaluate_queens(struct position *pos, int color) {
	int eval = 0;
	/* moving queen before minor pieces */
	if (color && pos->piece[white][queen] != bitboard(d1))
		eval -= 15 * popcount((pos->piece[white][knight] | pos->piece[white][bishop]) & RANK_1);
	else if(!color && pos->piece[black][queen] != bitboard(d8))
		eval -= 15 * popcount((pos->piece[black][knight] | pos->piece[black][bishop]) & RANK_8);
	
	return eval;
}

double game_phase(struct position *pos) {
	const int piece_phase[] = { 0, -1, 5, 5, 10, 20 };
	double phase = 0;
	for (int color = black; color <= white; color++)
		for (int piece = pawn; piece < king; piece++)
			phase += (double)popcount(pos->piece[color][piece]) * piece_phase[piece];
	phase = (phase - 30) / 70;
	phase = CLAMP(phase, 0, 1);
	return phase;
}

void print_evaluation(struct position *pos) {
	int i, j;
	int material[2][2] = { 0 };
	int positional[2][2] = { 0 };
	int mob[2][2] = { 0 };
	int safety[2][2] = { 0 };
	int bishops[2][2] = { 0 };
	int rooks[2][2] = { 0 };
	int pawns[2][2] = { 0 };
	const double phase_mg = game_phase(pos);
	const double phase_eg = 1 - phase_mg;
	int color;
	int sign;
	for (i = 0; i < 64; i++) {
		color = pos->mailbox[i] <= white_king;
		sign = 2 * color - 1;
		material[mg][color] += sign * piece_value[mg][pos->mailbox[i]];
		material[eg][color] += sign * piece_value[eg][pos->mailbox[i]];
	}
	for (i = 0; i < 64; i++) {
		color = pos->mailbox[i] <= white_king;
		sign = 2 * color - 1;
		positional[mg][color] += sign * eval_table[mg][pos->mailbox[i]][i];
		positional[eg][color] += sign * eval_table[eg][pos->mailbox[i]][i];
	}
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			positional[i][j] -= material[i][j];

	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			mob[i][j] = mobility(pos, j);

	for (j = 0; j < 2; j++)
		safety[mg][j] = king_safety(pos, j);

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			pawns[i][j] = evaluate_pawns(pos, j);
			bishops[eg][j] = evaluate_bishops(pos, j);
			rooks[i][j] = evaluate_rooks(pos, j);
		}
	}

	printf("+-------------+-------------+-------------+-------------+\n"
	       "| Term        |    White    |    Black    |    Total    |\n"
	       "|             |   MG    EG  |   MG    EG  |   MG    EG  |\n"
	       "|-------------|-------------|-------------|-------------|\n"
	       "| Material    | ");
	printdigits(material[mg][white]);
	printf(" ");
	printdigits(material[eg][white]);
	printf(" | ");
	printdigits(material[mg][black]);
	printf(" ");
	printdigits(material[eg][black]);
	printf(" | ");
	printdigits(material[mg][white] - material[mg][black]);
	printf(" ");
	printdigits(material[eg][white] - material[eg][black]);
	printf(" |\n");
	printf("| Positional  | %+.2f %+.2f | %+.2f %+.2f | %+.2f %+.2f |\n"
	       "| Mobility    | %+.2f %+.2f | %+.2f %+.2f | %+.2f %+.2f |\n"
	       "| King Safety | %+.2f %+.2f | %+.2f %+.2f | %+.2f %+.2f |\n"
	       "| Pawns       | %+.2f %+.2f | %+.2f %+.2f | %+.2f %+.2f |\n"
	       "| Bishops     | %+.2f %+.2f | %+.2f %+.2f | %+.2f %+.2f |\n"
	       "| Rooks       | %+.2f %+.2f | %+.2f %+.2f | %+.2f %+.2f |\n"
	       "| Turn        | %+.2f %+.2f | %+.2f %+.2f | %+.2f %+.2f |\n"
	       "+-------------+-------------+-------------+-------------+\n",
	       (double)positional[mg][white] / 100, (double)positional[eg][white] / 100,
	       (double)positional[mg][black] / 100, (double)positional[eg][black] / 100,
	       (double)(positional[mg][white] - positional[mg][black]) / 100,
	       (double)(positional[eg][white] - positional[eg][black]) / 100,
	       (double)mob[mg][white] / 100, (double)mob[eg][white] / 100,
	       (double)mob[mg][black] / 100, (double)mob[eg][black] / 100,
	       (double)(mob[mg][white] - mob[mg][black]) / 100,
	       (double)(mob[eg][white] - mob[eg][black]) / 100,
	       (double)safety[mg][white] / 100, (double)safety[eg][white] / 100,
	       (double)safety[mg][black] / 100, (double)safety[eg][black] / 100,
	       (double)(safety[mg][white] - safety[mg][black]) / 100,
	       (double)(safety[eg][white] - safety[eg][black]) / 100,
	       (double)pawns[mg][white] / 100, (double)pawns[eg][white] / 100,
	       (double)pawns[mg][black] / 100, (double)pawns[eg][black] / 100,
	       (double)(pawns[mg][white] - pawns[mg][black]) / 100,
	       (double)(pawns[eg][white] - pawns[eg][black]) / 100,
	       (double)bishops[mg][white] / 100, (double)bishops[eg][white] / 100,
	       (double)bishops[mg][black] / 100, (double)bishops[eg][black] / 100,
	       (double)(bishops[mg][white] - bishops[mg][black]) / 100,
	       (double)(bishops[eg][white] - bishops[eg][black]) / 100,
	       (double)rooks[mg][white] / 100, (double)rooks[eg][white] / 100,
	       (double)rooks[mg][black] / 100, (double)rooks[eg][black] / 100,
	       (double)(rooks[mg][white] - rooks[mg][black]) / 100,
	       (double)(rooks[eg][white] - rooks[eg][black]) / 100,
	       (double)4 * pos->turn / 100, (double)0,
	       (double)4 * (1 - pos->turn) / 100, (double)0,
	       (double)4 * (2 * pos->turn - 1) / 100, (double)0);
	printf("Phase: %.2f\n", phase_eg);
	printf("Evaluation: %.2f\n", (double)(pos->turn ? evaluate_static(pos, NULL) : -evaluate_static(pos, NULL)) / 100);
}

int16_t evaluate_static(struct position *pos, uint64_t *nodes) {
	if (nodes)
		++*nodes;
	int eval = 0, i;

	const double phase_mg = game_phase(pos);
	const double phase_eg = 1 - phase_mg;

	for (i = 0; i < 64; i++)
		eval += phase_mg * eval_table[mg][pos->mailbox[i]][i] + phase_eg * eval_table[eg][pos->mailbox[i]][i];

	eval += mobility(pos, white) - mobility(pos, black);
	eval += phase_mg * (king_safety(pos, white) - king_safety(pos, black));

	eval += phase_eg * (evaluate_bishops(pos, white) - evaluate_bishops(pos, black));
	eval += evaluate_rooks(pos, white) - evaluate_rooks(pos, black);
	eval += phase_mg * 4 * (2 * pos->turn - 1);
	eval += evaluate_pawns(pos, white) - evaluate_pawns(pos, black);
	eval += phase_mg * (evaluate_queens(pos, white) - evaluate_queens(pos, black));

	return pos->turn ? eval : -eval;
}

void evaluate_init(void) {
	memset(eval_table, 0, sizeof(eval_table));
	for (int i = 0; i < 13; i++) {
		for (int j  = 0; j < 64; j++) {
			for (int k = 0; k < 2; k++) {
				if (i == 0)
					eval_table[k][i][j] = 0;
				else if (i < 7)
					eval_table[k][i][j] = white_side_eval_table[k][i - 1][(7 - j / 8) * 8 + (j % 8)] +
						           piece_value[k][i];
				else
					eval_table[k][i][j] = -white_side_eval_table[k][i - 7][j] +
							   piece_value[k][i];
				init_status("populating evaluation lookup table");
			}
		}
	}
}
