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

mevalue psqtable[2][7][64];

mevalue piece_value[6] = { S(pawn_mg, pawn_eg), S(knight_mg, knight_eg), S(bishop_mg, bishop_eg), S(rook_mg, rook_eg), S(queen_mg, queen_eg), S(0, 0) };

enum { mg, eg };

/* from white's perspective, files a to d on a regular board */
const mevalue white_psqtable[6][64] = {
	{ /* pawn */
		S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0),
		S(  7,   0), S(  9,   0), S(  6,   0), S( 12,   0), S( 11,   0), S(  9,   0), S( 10,   0), S(  9,   0),
		S(  9,   0), S( 11,   0), S( 14,   0), S( 11,   0), S( 13,   0), S( 11,   0), S(  8,   0), S(  7,   0),
		S(  1,   5), S(  6,   5), S(  5,  10), S( 13,  25), S( 23,  25), S(  4,  10), S(  3,   5), S(  2,   5),
		S(  0,   0), S(  0,   0), S(  0,   0), S( 19,  20), S( 28,  20), S(-18,   0), S(  0,   0), S(  0,   0),
		S(  5,   5), S( -5,  -5), S(-10, -10), S(  9,   0), S( -2,   0), S(-13, -10), S( -7,  -5), S(  2,   5),
		S(  1,   5), S(  7,  10), S(  5,  10), S( -3, -20), S( -4, -20), S( 12,  10), S(  9,  10), S(  3,   5),
		S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0),
	}, { /* knight */
		S(-82, -50), S(-34, -40), S(-31, -30), S(-27, -30),
		S(-34, -40), S(-21, -20), S(-19,   0), S( -9,   0),
		S(-29, -30), S( -4,   0), S(  2,  10), S(  4,  15),
		S(-25, -30), S(  3,   5), S( 12,  15), S( 14,  20),
		S(-23, -30), S(  4,   0), S( 16,  15), S( 17,  20),
		S(-11, -30), S(  6,   5), S( 15,  10), S( 12,  15),
		S(-40, -40), S(-20, -20), S(  1,   0), S( 13,   5),
		S(-76, -50), S(-34, -40), S(-25, -30), S(-23, -30),
	}, { /* bishop */
		S(-23, -20), S(-17, -10), S(-15, -10), S(-14, -10),
		S(-18, -10), S(  3,   0), S(  9,   0), S(  2,   0),
		S(-14, -10), S(  0,   0), S(  3,   5), S(  9,  10),
		S(-13, -10), S( 16,   5), S( 15,   5), S( 17,  10),
		S(-10, -10), S( 18,   0), S( 11,  10), S( 14,  10),
		S( -9, -10), S(  9,  10), S( -2,  10), S(  7,  10),
		S(-13, -10), S(  5,   5), S(  3,   0), S(  0,   0),
		S(-22, -20), S( -5, -10), S(-10, -10), S(-11, -10),
	}, { /* rook */
		S(-10,   0), S(-11,   0), S(-10,   0), S( -8,   0),
		S( -7,   5), S( 11,  10), S( 16,  10), S( 13,  10),
		S(-13,  -5), S( -5,   0), S( -3,   0), S( -3,   0),
		S(-14,  -5), S( -6,   0), S( -1,   0), S( -2,   0),
		S(-11,  -5), S( -8,   0), S(  0,   0), S(  0,   0),
		S(-13,  -5), S(-10,   0), S(  1,   0), S(  3,   0),
		S( -3,  -5), S(-12,   0), S( -1,   0), S(  8,   0),
		S(-18,   0), S(-14,   0), S(  0,   0), S(  5,   5),
	}, { /* queen */
		S(-14, -20), S( -6, -10), S( -7, -10), S( -5,  -5),
		S(-11, -10), S( -1,   0), S(  1,   0), S(  0,   0),
		S(-10, -10), S( -2,   0), S(  3,   5), S(  3,   5),
		S( -5,  -5), S(  3,   0), S(  2,   5), S(  4,   5),
		S(  1,   0), S(  6,   0), S(  4,   5), S(  5,   5),
		S(-10, -10), S(  5,   5), S(  5,   5), S(  6,   5),
		S(-12, -10), S( -1,   0), S(  6,   5), S(  8,   0),
		S(-13, -20), S( -5, -10), S( -4, -10), S( -1,  -5),
	}, { /* king */
		S(-88, -50), S(-94, -40), S(-98, -30), S(-93, -20),
		S(-81, -30), S(-78, -20), S(-81, -10), S(-85,   0),
		S(-42, -30), S(-61, -10), S(-72,  20), S(-81,  30),
		S(-23, -30), S(-39, -10), S(-41,  30), S(-52,  40),
		S(-11, -30), S(-18, -10), S(-28,  30), S(-31,  40),
		S(  0, -30), S(  0, -10), S( -5,  20), S(-15,  30),
		S( 34, -30), S( 42, -30), S( 25,   0), S( -5,   0),
		S( 36, -50), S( 51, -30), S( 33, -30), S(  0, -30),
	}
};

const int16_t pawn_shelter[4][7] = {
	{
		-13, 50, 52, 27, 13, 14, 5,
	}, {
		-50, 30, 27, -23, -25, -36, -31,
	}, {
		-23, 32, 20, -10, -13, -20, -14,
	}, {
		-45, -10, -15, -31, -40, -56, -70,
	}
};

const int16_t unblocked_storm[4][7] = {
	{
		8, 113, 70, -52, -27, -19, -5,
	}, {
		11, -10, -23, -19, -13, -11, -3,
	}, {
		5, -33, -30, -20, 4, -1, 0,
	}, {
		13, 7, -15, -7, -4, -1, 0,
	}
};

const int16_t blocked_storm[7] = {
	0, -7, -27, 5, 7, 11, 4,
};

const mevalue mobility_bonus[4][28] = {
	{	/* idx 0 to 8 */
		S(-51,-63), S(-42,-40), S(-13, -19), S(0, -9), S(5, 11), S(13, 19),
		S(25, 20), S(33, 21), S(40, 23),
	}, {	/* idx 0 to 13 */
		S(-45, -52), S(-37, -34), S(-15, -16), S(3, 0), S(13, 18), S(18, 27),
		S(27, 38), S(32, 44), S(36, 51), S(41, 58), S(45, 63), S(47, 68),
		S(48, 74), S(49, 80),
	}, {	/* idx 0 to 14 */
		S(-34, -52), S(-14, -28), S(0, -13), S(3, 0), S(5, 1), S(10, 3),
		S(13, 11), S(17, 18), S(23, 27), S(28, 33), S(34, 39), S(38, 48),
		S(41, 54), S(42, 58), S(43, 61),
	}, {	/* idx 0 to 27 */
		S(-10, -21), S(-8, -19), S(-2, -11), S(4, -2), S(8, 6), S(11, 10),
		S(14, 12), S(17, 16), S(19, 19), S(22, 24), S(24, 28), S(25, 31),
		S(27, 34), S(29, 38), S(30, 43), S(32, 48), S(35, 51), S(37, 56),
		S(40, 61), S(41, 66), S(42, 71), S(43, 75), S(46, 79), S(48, 82),
		S(51, 85), S(52, 89), S(53, 94), S(54, 101),
	}
};

mevalue evaluate_mobility(const struct position *pos, struct evaluationinfo *ei, int turn) {
	UNUSED(pos);
	return ei->mobility[turn];
}

mevalue evaluate_king(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	int king_square = ctz(pos->piece[turn][king]);

	/* pawn shelter */
	uint64_t ourpawns = pos->piece[turn][pawn] &
		~(passed_files(king_square, 1 - turn) | rank(king_square));
	uint64_t theirpawns = pos->piece[1 - turn][pawn] &
		~(passed_files(king_square, 1 - turn) | rank(king_square));

	uint64_t b;
	int center = CLAMP(king_square % 8, 1, 6);
	int ourrank, theirrank;
	for (int f = center - 1; f <= center + 1; f++) {
		/* this is a mess but calculates perspective wise
		 * highest rank for pawns
		 */
		b = ourpawns & file(f);
		/* king is on semi open file */
		if (!b)
			eval += S(-15, 0);
		if (turn)
			ourrank = b ? (63 - clz(b)) / 8 : 0;
		else
			ourrank = b ? 7 - ctz(b) / 8 : 0;
		b = theirpawns & file(f);
		if (!b)
			eval += S(-5, 0);
		if (turn)
			theirrank = b ? ctz(b) / 8 : 0;
		else
			theirrank = b ? 7 - (63 - clz(b)) / 8 : 0;

		int d = MIN(f, 7 - f);
		eval += S(pawn_shelter[d][ourrank], 0);
		if (ourrank && ourrank + 1 == theirrank)
			eval += S(blocked_storm[ourrank], 0);
		else
			eval += S(unblocked_storm[d][theirrank], 0);
	}

	/* king safety */
	uint64_t weak_squares = ei->attacked_squares[1 - turn][all] &
				~(ei->attacked2_squares[turn]) &
				(~ei->attacked_squares[turn][all] | ei->attacked_squares[turn][king] | ei->attacked_squares[turn][queen]);

	int king_danger =       ei->king_attack_units[1 - turn]
			+  40 * popcount(weak_squares & ei->king_ring[turn])
			- 200 * !pos->piece[1 - turn][queen];
	if (king_danger > 0)
		eval -= S(king_danger * king_danger / 2048, king_danger / 8);

	return eval;
}

mevalue evaluate_knights(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	uint64_t b = pos->piece[turn][knight];

	uint64_t outpost = turn ? (RANK_4 | RANK_5 | RANK_6) : (RANK_3 | RANK_4 | RANK_5);

	while (b) {
		int square = ctz(b);
		uint64_t squareb = bitboard(square);
		
		uint64_t attacks = knight_attacks(square, 0);
		ei->attacked2_squares[turn] |= attacks & ei->attacked_squares[turn][all];
		ei->attacked_squares[turn][knight] |= attacks;
		ei->attacked_squares[turn][all] |= attacks;
		/* mobility (range of popcount is [0, 8]) */
		ei->mobility[turn] += mobility_bonus[knight - 2][popcount(attacks & ei->mobility_squares[turn])];

		/* king attacks */
		if (ei->king_ring[1 - turn] & attacks)
			ei->king_attack_units[turn] += 60 * (popcount(ei->king_ring[1 - turn] & attacks) + 1) / 2;

		/* outpost */
		if (squareb & outpost & ~ei->pawn_attack_span[1 - turn] & ei->attacked_squares[turn][pawn])
			eval += S(40, 15);
		else if (attacks & outpost & ~ei->pawn_attack_span[1 - turn] & ei->attacked_squares[turn][pawn])
			eval += S(15, 5);
		
		/* minor behind pawn */
		if (squareb & shift_color(pos->piece[turn][pawn], 1 - turn))
			eval += S(10, 0);
		
		/* penalty if piece is far from own king */
		eval += S(-4, -4) * distance(square, ctz(pos->piece[turn][king]));

		b = clear_ls1b(b);
	}
	return eval;
}

mevalue evaluate_bishops(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	uint64_t b = pos->piece[turn][bishop];
	while (b) {
		int square = ctz(b);
		uint64_t squareb = bitboard(square);
		
		uint64_t attacks = bishop_attacks(square, 0, all_pieces(pos) ^ pos->piece[turn][queen]);
		ei->attacked2_squares[turn] |= attacks & ei->attacked_squares[turn][all];
		ei->attacked_squares[turn][bishop] |= attacks;
		ei->attacked_squares[turn][all] |= attacks;
		/* mobility (range of popcount is [0, 13]) */
		ei->mobility[turn] += mobility_bonus[bishop - 2][popcount(attacks & ei->mobility_squares[turn])];

		/* king attacks */
		if (ei->king_ring[1 - turn] & attacks)
			ei->king_attack_units[turn] += 50 * (popcount(ei->king_ring[1 - turn] & attacks) + 1) / 2;
		
		/* bishop pair */
		if (popcount(b) >= 2)
			eval += S(20, 25);

		/* minor behind pawn */
		if (squareb & shift_color(pos->piece[turn][pawn], 1 - turn))
			eval += S(15, 0);
		
		/* penalty if piece is far from own king */
		eval += S(-3, -3) * distance(square, ctz(pos->piece[turn][king]));

		/* penalty for own pawns on same squares as bishop */
		eval += S(-1, -4) * popcount(same_colored_squares(square) & pos->piece[turn][pawn]);

		b = clear_ls1b(b);
	}
	return eval;
}

mevalue evaluate_rooks(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	uint64_t b = pos->piece[turn][rook];
	while (b) {
		int square = ctz(b);
		
		uint64_t attacks = rook_attacks(square, 0, all_pieces(pos) ^ pos->piece[turn][rook] ^ pos->piece[turn][queen]);
		ei->attacked2_squares[turn] |= attacks & ei->attacked_squares[turn][all];
		ei->attacked_squares[turn][rook] |= attacks;
		ei->attacked_squares[turn][all] |= attacks;
		/* mobility (range of popcount is [0, 14]) */
		int mobility = popcount(attacks & ei->mobility_squares[turn]);
		ei->mobility[turn] += mobility_bonus[rook - 2][mobility];

		/* king attacks */
		if (ei->king_ring[1 - turn] & attacks)
			ei->king_attack_units[turn] += 60 * (popcount(ei->king_ring[1 - turn] & attacks) + 1) / 2;

		/* bonus on semiopen files */
		if (!(pos->piece[turn][pawn] & file(square))) {
			eval += S(30, 15);
		}
		/* penalty if blocked by uncastled king */
		else if (mobility <= 5) {
			int kf = ctz(pos->piece[turn][king]) % 8;
			if ((kf < e1) == ((square % 8) < kf))
				eval += S(-30, 0) * (1 + !(pos->castle & (turn ? 0x3 : 0xC)));
		}

		b = clear_ls1b(b);
	}
	return eval;
}

mevalue evaluate_queens(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	uint64_t b = pos->piece[turn][queen];
	while (b) {
		int square = ctz(b);
		
		uint64_t attacks = queen_attacks(square, 0, all_pieces(pos));
		ei->attacked2_squares[turn] |= attacks & ei->attacked_squares[turn][all];
		ei->attacked_squares[turn][queen] |= attacks;
		ei->attacked_squares[turn][all] |= attacks;
		/* mobility (range of popcount is [0, 27]) */
		int mobility = popcount(attacks & ei->mobility_squares[turn]);
		ei->mobility[turn] += mobility_bonus[queen - 2][mobility];

		/* king attacks */
		if (ei->king_ring[1 - turn] & attacks)
			ei->king_attack_units[turn] += 70 * (popcount(ei->king_ring[1 - turn] & attacks) + 1) / 2;

		/* undeveloped minor pieces when moving queen */
		if (turn)
			eval += (square != d1) * S(-8, 0) * popcount((pos->piece[white][knight] | pos->piece[white][bishop]) & RANK_1);
		else
			eval += (square != d8) * S(-8, 0) * popcount((pos->piece[black][knight] | pos->piece[black][bishop]) & RANK_8);

		b = clear_ls1b(b);
	}
	return eval;
}

mevalue evaluate_space(const struct position *pos, struct evaluationinfo *ei, int turn) {
	const uint64_t center = (FILE_C | FILE_D | FILE_E | FILE_F) & (turn ? (RANK_2 | RANK_3 | RANK_4) : (RANK_5 | RANK_6 | RANK_7));

	const uint64_t safe_center = center & ~pos->piece[turn][pawn] & ~ei->attacked_squares[1 - turn][pawn];

	uint64_t behind = shift_color(pos->piece[turn][pawn], 1 - turn);
	behind |= shift_color(behind, 1 - turn);
	behind |= shift_color(behind, 1 - turn);

	return 3 * S(popcount(safe_center) + popcount(behind & safe_center), 0);
}

double game_phase(const struct position *pos) {
	const int piece_phase[] = { 0, -1, 5, 5, 10, 20 };
	double phase = 0;
	for (int color = black; color <= white; color++)
		for (int piece = pawn; piece < king; piece++)
			phase += (double)popcount(pos->piece[color][piece]) * piece_phase[piece];
	phase = (phase - 30) / 70;
	phase = CLAMP(phase, 0, 1);
	return phase;
}

void tables_init(const struct position *pos, struct evaluationinfo *ei) {

	ei->attacked_squares[white][pawn] = shift_north_west(pos->piece[white][pawn]) | shift_north_east(pos->piece[white][pawn]);
	ei->attacked_squares[black][pawn] = shift_south_west(pos->piece[black][pawn]) | shift_south_east(pos->piece[black][pawn]);

	ei->pawn_attack_span[white] = fill_north(ei->attacked_squares[white][pawn]);
	ei->pawn_attack_span[black] = fill_south(ei->attacked_squares[black][pawn]);

	memset(ei->attacked_squares, 0, sizeof(ei->attacked_squares));

	for (int turn = 0; turn < 2; turn++) {
		ei->mobility[turn] = 0;
		ei->mobility_squares[turn] = ~pos->piece[turn][king];
		/* enemy pawn attacks */
		ei->mobility_squares[turn] &= ~ei->attacked_squares[1 - turn][pawn];
		/* blocked pawns */
		ei->mobility_squares[turn] &= ~(shift_color(pos->piece[black][all] | pos->piece[white][all], 1 - turn) & pos->piece[turn][pawn]);

		ei->king_attack_units[turn] = 0;
		int king_square = ctz(pos->piece[turn][king]);
		int x = CLAMP(king_square % 8, 1, 6);
		int y = CLAMP(king_square / 8, 1, 6);
		king_square = 8 * y + x;
		/* king ring */
		ei->king_ring[turn] = king_attacks(king_square, 0) | bitboard(king_square);
		/* but not defended by two own pawns,
		 * if it is only defended by one pawn it could pinned.
		 */
		ei->king_ring[turn] &= ~(shift_color_west(pos->piece[turn][pawn], turn) & shift_color_east(pos->piece[turn][pawn], turn));
		ei->attacked_squares[turn][king] = king_attacks(ctz(pos->piece[turn][king]), 0);
		ei->attacked_squares[turn][all] = ei->attacked_squares[turn][king] | ei->attacked_squares[turn][pawn];
		ei->attacked2_squares[turn] = ei->attacked_squares[turn][king] & ei->attacked_squares[turn][pawn];
	}
}

mevalue evaluate_psqtable(const struct position *pos, struct evaluationinfo *ei, int turn) {
	UNUSED(ei);
	mevalue eval = 0;
	uint64_t bitboard;
	int square;
	for (int j = pawn; j <= king; j++) {
		bitboard = pos->piece[turn][j];
		while (bitboard) {
			square = ctz(bitboard);
			eval += psqtable[turn][j][square];
			bitboard = clear_ls1b(bitboard);
		}
	}

	return eval;
}

mevalue evaluate_turn(const struct position *pos, struct evaluationinfo *ei, int turn) {
	UNUSED(ei);
	return (pos->turn == turn) * S(15, 5);
}

int16_t evaluate_classical(const struct position *pos) {
	struct evaluationinfo ei;

	tables_init(pos, &ei);

	mevalue eval = evaluate_psqtable(pos, &ei, white) - evaluate_psqtable(pos, &ei, black);

	eval += evaluate_pawns(pos, &ei, white) - evaluate_pawns(pos, &ei, black);
	eval += evaluate_knights(pos, &ei, white) - evaluate_knights(pos, &ei, black);
	eval += evaluate_bishops(pos, &ei, white) - evaluate_bishops(pos, &ei, black);
	eval += evaluate_rooks(pos, &ei, white) - evaluate_rooks(pos, &ei, black);
	eval += evaluate_queens(pos, &ei, white) - evaluate_queens(pos, &ei, black);
	eval += evaluate_king(pos, &ei, white) - evaluate_king(pos, &ei, black);
	
	eval += evaluate_mobility(pos, &ei, white) - evaluate_mobility(pos, &ei, black);

	eval += evaluate_space(pos, &ei, white) - evaluate_space(pos, &ei, black);

	int16_t ret = mevalue_evaluation(eval, game_phase(pos));

	return pos->turn ? ret : -ret;
}

void print_mevalue(mevalue eval);
mevalue evaluate_print_x(const char *name, const struct position *pos, struct evaluationinfo *ei,
		mevalue (*evaluate_x)(const struct position *, struct evaluationinfo *ei, int));

void evaluate_print(const struct position *pos) {
	struct evaluationinfo ei;
	tables_init(pos, &ei);

	mevalue eval = 0;

	printf("+-------------+-------------+-------------+-------------+\n"
	       "| Term        |    White    |    Black    |    Total    |\n"
	       "|             |   MG    EG  |   MG    EG  |   MG    EG  |\n"
	       "+-------------+-------------+-------------+-------------+\n");
	eval += evaluate_print_x("PSQT", pos, &ei, &evaluate_psqtable);
	eval += evaluate_print_x("Knights", pos, &ei, &evaluate_knights);
	eval += evaluate_print_x("Bishops", pos, &ei, &evaluate_bishops);
	eval += evaluate_print_x("Rooks", pos, &ei, &evaluate_rooks);
	eval += evaluate_print_x("Queens", pos, &ei, &evaluate_queens);
	eval += evaluate_print_x("Pawns", pos, &ei, &evaluate_pawns);
	eval += evaluate_print_x("King", pos, &ei, &evaluate_king);
	eval += evaluate_print_x("Mobility", pos, &ei, &evaluate_mobility);
	eval += evaluate_print_x("Space", pos, &ei, &evaluate_space);
	eval += evaluate_print_x("Turn", pos, &ei, &evaluate_turn);
	printf("+-------------+-------------+-------------+-------------+\n");
	printf("| Total       |             |             | ");
	print_mevalue(eval);
	printf(" |\n");
	printf("+-------------+-------------+-------------+-------------+\n");
	printf("Phase: %.2f\n", game_phase(pos));
	printf("Evaluation: %+.2f\n", (float)mevalue_evaluation(eval, game_phase(pos)) / 100);
}

void print_mevalue(mevalue eval) {
	int m = mevalue_mg(eval);
	int e = mevalue_eg(eval);
	if (ABS(m) >= 10000)
		printf("%+.0f ", (float)m / 100);
	else if (ABS(m) >= 1000)
		printf("%+.1f", (float)m / 100);
	else
		printf("%+.2f", (float)m / 100);
	printf(" ");
	if (ABS(e) >= 10000)
		printf("%+.0f ", (float)e / 100);
	else if (ABS(e) >= 1000)
		printf("%+.1f", (float)e / 100);
	else
		printf("%+.2f", (float)e / 100);
}

mevalue evaluate_print_x(const char *name, const struct position *pos, struct evaluationinfo *ei,
		mevalue (*evaluate_x)(const struct position *, struct evaluationinfo *ei, int)) {
	mevalue w = evaluate_x(pos, ei, white);
	mevalue b = evaluate_x(pos, ei, black);
	char namepadded[32];
	strcpy(namepadded, name);
	int len = strlen(name);
	int i;
	for (i = 0; i < 11 - len; i++)
		namepadded[len + i] = ' ';
	namepadded[len + i] = '\0';
	printf("| %s | ", namepadded);
	print_mevalue(w);
	printf(" | ");
	print_mevalue(b);
	printf(" | ");
	print_mevalue(w - b);
	printf(" |\n");
	return w - b;
}

void evaluate_init(void) {
	memset(psqtable, 0, sizeof(psqtable));
	for (int turn = 0; turn < 2; turn++) {
		for (int piece = 1; piece <= 6; piece++) {
			for (int square = 0; square < 64; square++) {
				int x = square % 8;
				int y = square / 8;
				int factor = (piece == pawn) ? 8 : 4;
				if (x >= 4 && piece != pawn)
					x = 7 - x;
				if (turn == white)
					y = 7 - y;
				psqtable[turn][piece][square] = white_psqtable[piece - 1][factor * y + x] +
					piece_value[piece - 1];
				init_status("populating evaluation lookup table");
			}
		}
	}
}
