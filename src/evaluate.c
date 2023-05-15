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
#include "attackgen.h"
#include "movegen.h"
#include "pawn.h"
#include "tables.h"

mevalue king_on_open_file     = S(-8, 3);
mevalue outpost_bonus  	      = S(38, 11);
mevalue outpost_attack        = S(15, 9);
mevalue minor_behind_pawn     = S(3, 7);
mevalue knight_far_from_king  = S(-5, -1);
mevalue bishop_far_from_king  = S(-6, 0);
mevalue bishop_pair           = S(17, 50);
mevalue pawn_on_bishop_square = S(-6, -8);
mevalue rook_on_open_file     = S(18, 9);
mevalue blocked_rook          = S(-28, -9);
mevalue undeveloped_piece     = S(-12, -17);
mevalue defended_minor        = S(4, 5);

/* king danger */
int weak_squares_danger       = 34;
int enemy_no_queen_bonus      = 206;
int knight_king_attack_danger = 61;
int bishop_king_attack_danger = 53;
int rook_king_attack_danger   = 69;
int queen_king_attack_danger  = 78;

int tempo_bonus               = 7;

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
		if (!b && f == center)
			eval += king_on_open_file;
		if (turn)
			ourrank = b ? (63 - clz(b)) / 8 : 0;
		else
			ourrank = b ? 7 - ctz(b) / 8 : 0;
		b = theirpawns & file(f);
		if (turn)
			theirrank = b ? ctz(b) / 8 : 0;
		else
			theirrank = b ? 7 - (63 - clz(b)) / 8 : 0;

		int d = MIN(f, 7 - f);
		eval += pawn_shelter[d][ourrank];
		if (ourrank && ourrank + 1 == theirrank)
			eval += blocked_storm[ourrank];
		else
			eval += unblocked_storm[d][theirrank];
	}

	/* king safety */
	uint64_t weak_squares = ei->attacked_squares[1 - turn][all] &
				~(ei->attacked2_squares[turn]) &
				(~ei->attacked_squares[turn][all] | ei->attacked_squares[turn][king] | ei->attacked_squares[turn][queen]);

	int king_danger =       ei->king_attack_units[1 - turn]
			+ weak_squares_danger * popcount(weak_squares & ei->king_ring[turn])
			- enemy_no_queen_bonus * !pos->piece[1 - turn][queen];
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
			ei->king_attack_units[turn] += knight_king_attack_danger * (popcount(ei->king_ring[1 - turn] & attacks) + 1) / 2;

		/* outpost */
		if (squareb & outpost & ~ei->pawn_attack_span[1 - turn] & ei->attacked_squares[turn][pawn])
			eval += outpost_bonus;
		else if (attacks & outpost & ~ei->pawn_attack_span[1 - turn] & ei->attacked_squares[turn][pawn])
			eval += outpost_attack;
		
		/* minor behind pawn */
		if (squareb & shift_color(pos->piece[turn][pawn], 1 - turn))
			eval += minor_behind_pawn;

		/* defended minor */
		if (squareb & ei->attacked_squares[turn][pawn])
			eval += defended_minor;
		
		/* penalty if piece is far from own king */
		eval += knight_far_from_king * distance(square, ctz(pos->piece[turn][king]));

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
			ei->king_attack_units[turn] += bishop_king_attack_danger * (popcount(ei->king_ring[1 - turn] & attacks) + 1) / 2;

		/* bishop pair */
		if (popcount(b) >= 2)
			eval += bishop_pair;

		/* minor behind pawn */
		if (squareb & shift_color(pos->piece[turn][pawn], 1 - turn))
			eval += minor_behind_pawn;

		/* defended minor */
		if (squareb & ei->attacked_squares[turn][pawn])
			eval += defended_minor;
		
		/* penalty if piece is far from own king */
		eval += bishop_far_from_king * distance(square, ctz(pos->piece[turn][king]));

		/* penalty for own pawns on same squares as bishop */
		eval += pawn_on_bishop_square * popcount(same_colored_squares(square) & pos->piece[turn][pawn]);

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
			ei->king_attack_units[turn] += rook_king_attack_danger * (popcount(ei->king_ring[1 - turn] & attacks) + 1) / 2;

		/* bonus on semiopen files */
		if (!(pos->piece[turn][pawn] & file(square)))
			eval += rook_on_open_file;

		/* penalty if blocked by uncastled king */
		else if (mobility <= 5) {
			int kf = ctz(pos->piece[turn][king]) % 8;
			if ((kf < e1) == ((square % 8) < kf))
				eval += blocked_rook * (1 + !(pos->castle & (turn ? 0x3 : 0xC)));
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
			ei->king_attack_units[turn] += queen_king_attack_danger * (popcount(ei->king_ring[1 - turn] & attacks) + 1) / 2;

		/* undeveloped minor pieces when moving queen */
		if (turn)
			eval += (square != d1) * undeveloped_piece * popcount((pos->piece[white][knight] | pos->piece[white][bishop]) & RANK_1);
		else
			eval += (square != d8) * undeveloped_piece * popcount((pos->piece[black][knight] | pos->piece[black][bishop]) & RANK_8);

		b = clear_ls1b(b);
	}
	return eval;
}

/* from stockfish */
mevalue evaluate_space(const struct position *pos, struct evaluationinfo *ei, int turn) {
	const uint64_t center = (FILE_C | FILE_D | FILE_E | FILE_F) & (turn ? (RANK_2 | RANK_3 | RANK_4) : (RANK_5 | RANK_6 | RANK_7));

	const uint64_t safe_center = center & ~pos->piece[turn][pawn] & ~ei->attacked_squares[1 - turn][pawn];

	uint64_t behind = shift_color(pos->piece[turn][pawn], 1 - turn);
	behind |= shift_color(behind, 1 - turn);
	behind |= shift_color(behind, 1 - turn);

	int bonus = popcount(safe_center) + popcount(behind & safe_center & ~ei->attacked_squares[1 - turn][all]);
	int weight = popcount(pos->piece[turn][all]) - 3 + popcount((shift_north(pos->piece[white][pawn]) & pos->piece[black][pawn]));
	return S(bonus * weight * weight / 16, 0);
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

void evaluationinfo_init(const struct position *pos, struct evaluationinfo *ei) {
	memset(ei->attacked_squares, 0, sizeof(ei->attacked_squares));

	ei->attacked_squares[white][pawn] = shift_north_west(pos->piece[white][pawn]) | shift_north_east(pos->piece[white][pawn]);
	ei->attacked_squares[black][pawn] = shift_south_west(pos->piece[black][pawn]) | shift_south_east(pos->piece[black][pawn]);

	ei->pawn_attack_span[white] = fill_north(ei->attacked_squares[white][pawn]);
	ei->pawn_attack_span[black] = fill_south(ei->attacked_squares[black][pawn]);


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

int16_t evaluate_classical(const struct position *pos) {
	struct evaluationinfo ei;

	evaluationinfo_init(pos, &ei);

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

	return (pos->turn ? ret : -ret) + tempo_bonus;
}

void print_mevalue(mevalue eval);
mevalue evaluate_print_x(const char *name, const struct position *pos, struct evaluationinfo *ei,
		mevalue (*evaluate_x)(const struct position *, struct evaluationinfo *ei, int));

void evaluate_print(const struct position *pos) {
	struct evaluationinfo ei;
	evaluationinfo_init(pos, &ei);

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
