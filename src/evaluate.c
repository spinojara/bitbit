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

int piece_value[13] = { 0, 100, 300, 315, 500, 900, 0, -100, -300, -315, -500, -900, 0 };

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
	} }, /* end game */ { {
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

int king_safety(struct position *pos) {
	int king_white;
	int king_black;
	int eval = 0;
	/* half open files near king are bad */
	king_white = ctz(pos->piece[white][king]);
	if (file_left(king_white) && !(file_left(king_white) & pos->piece[white][pawn]))
		eval -= 30;
	if (!(file(king_white) & pos->piece[white][pawn]))
		eval -= 30;
	if (file_right(king_white) && !(file_right(king_white) & pos->piece[white][pawn]))
		eval -= 30;
	king_black = ctz(pos->piece[black][king]);
	if (file_left(king_black) && !(file_left(king_black) & pos->piece[black][pawn]))
		eval += 30;
	if (!(file(king_black) & pos->piece[black][pawn]))
		eval += 30;
	if (file_right(king_black) && !(file_right(king_black) & pos->piece[black][pawn]))
		eval += 30;

	/* safety table */
	int attack_units;
	uint64_t squares;
	attack_units = 0;
	squares = king_squares(ctz(pos->piece[white][king]), 1);
	attack_units += 5 * popcount(squares & pos->piece[black][queen]);
	attack_units += 3 * popcount(squares & pos->piece[black][rook]);
	attack_units += 2 * popcount(squares & (pos->piece[black][bishop] | pos->piece[black][knight]));
	eval -= safety_table[MIN(attack_units, 99)];
	attack_units = 0;
	squares = king_squares(ctz(pos->piece[black][king]), 0);
	attack_units += 5 * popcount(squares & pos->piece[white][queen]);
	attack_units += 3 * popcount(squares & pos->piece[white][rook]);
	attack_units += 2 * popcount(squares & (pos->piece[white][bishop] | pos->piece[white][knight]));
	eval += safety_table[MIN(attack_units, 99)];

	/* own pawns close to king */
	eval += 15 * popcount(king_attacks(king_white, 0) & pos->piece[white][pawn]);
	eval -= 15 * popcount(king_attacks(king_black, 0) & pos->piece[black][pawn]);

	/* uncastled king */
	if (popcount(pos->piece[white][rook]) == 2) {
		int rook1_x = ctz(pos->piece[white][rook]) % 8;
		squares = clear_ls1b(pos->piece[white][rook]);
		int rook2_x = ctz(squares) % 8;
		if (rook2_x < rook1_x) {
			squares = rook2_x;
			rook2_x = rook1_x;
			rook1_x = squares;
		}
		if (rook1_x <= king_white % 8 && king_white % 8 <= rook2_x)
			eval -= 40;
	}
	if (popcount(pos->piece[black][rook]) == 2) {
		int rook1_x = ctz(pos->piece[black][rook]) % 8;
		squares = clear_ls1b(pos->piece[black][rook]);
		int rook2_x = ctz(squares) % 8;
		if (rook2_x < rook1_x) {
			squares = rook2_x;
			rook2_x = rook1_x;
			rook1_x = squares;
		}
		if (rook1_x <= king_black % 8 && king_black % 8 <= rook2_x)
			eval += 40;
	}

	return eval;
}

int16_t evaluate_knights(struct position *pos) {
	int eval = 0;
	/* blocked c pawns */
	if (pos->mailbox[c3] == white_knight && pos->mailbox[c2] == white_pawn &&
			pos->mailbox[d4] == white_pawn && pos->mailbox[e4] != white_pawn)
		eval -= 50;
	if (pos->mailbox[c6] == white_knight && pos->mailbox[c7] == white_pawn &&
			pos->mailbox[d5] == white_pawn && pos->mailbox[e5] != white_pawn)
		eval += 50;
	return eval;
}

int16_t evaluate_bishops(struct position *pos) {
	int eval = 0;
	if (pos->piece[white][bishop] && clear_ls1b(pos->piece[white][bishop]))
		eval += 30;
	if (pos->piece[black][bishop] && clear_ls1b(pos->piece[black][bishop]))
		eval -= 30;
	return eval;
}

int16_t evaluate_rooks(struct position *pos) {
	int eval = 0, square;
	uint64_t b;
	/* penalize rooks on non open files */
	for (int i = 0; i < 7; i++) {
		b = file(i) & (pos->piece[white][pawn] | pos->piece[black][pawn]);
		if (file(i) & pos->piece[white][rook])
			eval -= 10 * popcount(b);
		if (file(i) & pos->piece[black][rook])
			eval += 10 * popcount(b);
	}

	/* connected rooks (assuming max 2 rooks) */
	if (pos->piece[white][rook]) {
		square = ctz(pos->piece[white][rook]);
		if (pos->piece[white][rook] & rook_attacks(square, 0, pos->piece[white][all] | pos->piece[black][all]))
			eval += 30;
	}
	if (pos->piece[black][rook]) {
		square = ctz(pos->piece[black][rook]);
		if (pos->piece[black][rook] & rook_attacks(square, 0, pos->piece[white][all] | pos->piece[black][all]))
			eval -= 30;
	}
	return eval;
}

int16_t evaluate_queen(struct position *pos) {
	int eval = 0;
	/* moving queen before minor pieces */
	if (pos->piece[white][queen] != bitboard(d1))
		eval -= 15 * popcount((pos->piece[white][knight] | pos->piece[white][bishop]) & RANK_1);
	if (pos->piece[black][queen] != bitboard(d8))
		eval += 15 * popcount((pos->piece[white][knight] | pos->piece[white][bishop]) & RANK_1);
	
	return eval;
}

int16_t evaluate_static(struct position *pos, uint64_t *nodes) {
	++*nodes;
	int eval = 0, i;
	int piece_value_total[] = { 0, 0, 3, 3, 5, 9 };
	double early_game = 0;

	for (i = knight; i < king; i++) {
		early_game += piece_value_total[i] * popcount(pos->piece[white][i]);
		early_game += piece_value_total[i] * popcount(pos->piece[black][i]);
	}
 	early_game = CLAMP((early_game - 10) / 35, 0, 1);

	for (i = 0; i < 64; i++)
		eval += early_game * eval_table[0][pos->mailbox[i]][i] + (1 - early_game) * eval_table[1][pos->mailbox[i]][i];

	/* encourage trading when ahead, discourage when behind */
	eval = nearint((0.99 + 0.011 / (early_game + 0.1)) * eval);

	eval += (2 * pos->turn - 1) * 15;

	eval += evaluate_bishops(pos);
	eval += evaluate_knights(pos);
	eval += evaluate_pawns(pos);
	eval += evaluate_rooks(pos);
	eval += evaluate_queen(pos);

	eval += early_game * king_safety(pos);

	eval += 8 * (mobility(pos, white) - mobility(pos, black));
	
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
						           piece_value[i];
				else
					eval_table[k][i][j] = -white_side_eval_table[k][i - 7][j] +
							   piece_value[i];
				init_status("populating evaluation lookup table");
			}
		}
	}
}
