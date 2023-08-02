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
#include "nnue.h"

mevalue king_on_open_file     = S(-13, -4);
mevalue outpost_bonus  	      = S(47, 16);
mevalue outpost_attack        = S(16, 15);
mevalue minor_behind_pawn     = S(6, 4);
mevalue knight_far_from_king  = S(-5, -8);
mevalue bishop_far_from_king  = S(-6, -3);
mevalue bishop_pair           = S(20, 63);
mevalue pawn_on_bishop_square = S(-4, -10);
mevalue rook_on_open_file     = S(16, 11);
mevalue blocked_rook          = S(-28, -4);
mevalue undeveloped_piece     = S(-8, -32);
mevalue defended_minor        = S(4, 9);
mevalue tempo_bonus           = S(5, 5);

int weak_squares_danger       = 83;
int enemy_no_queen_bonus      = 429;
int knight_attack_danger      = 58;
int bishop_attack_danger      = 53;
int rook_attack_danger        = 58;
int queen_attack_danger       = 41;
int king_danger               = 0;

int phase_max_material        = 7822;
int phase_min_material        = 922;
int phase_knight              = 352;
int phase_bishop              = 309;
int phase_rook                = 651;
int phase_queen               = 1437;

int division = 8;

const int material_value[7] = { 0, 1, 3, 3, 5, 9, 0 };

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
		if (!b && f == center) {
			//ei->king_on_open_file[turn] += 1;
			eval += king_on_open_file;
		}
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
		ei->pawn_shelter[turn][d * 7 + ourrank] += 1;
		eval += pawn_shelter[d * 7 + ourrank];
		if (ourrank && ourrank + 1 == theirrank) {
			//ei->blocked_storm[turn][ourrank] += 1;
			eval += blocked_storm[ourrank];
		}
		else {
			//ei->unblocked_storm[turn][d * 7 + theirrank] += 1;
			eval += unblocked_storm[d * 7 + theirrank];
		}
	}

	/* king safety */
	ei->weak_squares[turn] = ei->attacked_squares[1 - turn][all] &
				 ~(ei->attacked2_squares[turn]) &
				 (~ei->attacked_squares[turn][all] | ei->attacked_squares[turn][king] | ei->attacked_squares[turn][queen]);

	ei->king_danger[turn] = knight_attack_danger * ei->king_attack_units[1 - turn][knight]
			      + bishop_attack_danger * ei->king_attack_units[1 - turn][bishop]
			      + rook_attack_danger * ei->king_attack_units[1 - turn][rook]
			      + queen_attack_danger * ei->king_attack_units[1 - turn][queen]
			      + weak_squares_danger * popcount(ei->weak_squares[turn] & ei->king_ring[turn])
			      - enemy_no_queen_bonus * !pos->piece[1 - turn][queen]
			      + king_danger;
	if (ei->king_danger[turn] > 0)
		eval -= S(ei->king_danger[turn] * ei->king_danger[turn] / 2048, ei->king_danger[turn] / division);

	return eval;
}

mevalue evaluate_knights(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	uint64_t b = pos->piece[turn][knight];

	uint64_t outpost = turn ? (RANK_4 | RANK_5 | RANK_6) : (RANK_3 | RANK_4 | RANK_5);

	while (b) {
		ei->material += phase_knight;
		ei->material_value[turn] += material_value[knight];

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
			ei->king_attack_units[turn][knight] += popcount(ei->king_ring[1 - turn] & attacks) + 1;

		/* outpost */
		if (squareb & outpost & ~ei->pawn_attack_span[1 - turn] & ei->attacked_squares[turn][pawn]) {
			//ei->outpost_bonus[turn] += 1;
			eval += outpost_bonus;
		}
		else if (attacks & outpost & ~ei->pawn_attack_span[1 - turn] & ei->attacked_squares[turn][pawn]) {
			//ei->outpost_attack[turn] += 1;
			eval += outpost_attack;
		}
		
		/* minor behind pawn */
		if (squareb & shift_color(pos->piece[turn][pawn], 1 - turn)) {
			//ei->minor_behind_pawn[turn] += 1;
			eval += minor_behind_pawn;
		}

		/* defended minor */
		if (squareb & ei->attacked_squares[turn][pawn]) {
			//ei->defended_minor[turn] += 1;
			eval += defended_minor;
		}
		
		/* penalty if piece is far from own king */
		//ei->knight_far_from_king[turn] += distance(square, ctz(pos->piece[turn][king]));
		eval += knight_far_from_king * distance(square, ctz(pos->piece[turn][king]));

		b = clear_ls1b(b);
	}
	return eval;
}

mevalue evaluate_bishops(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	uint64_t b = pos->piece[turn][bishop];
	while (b) {
		ei->material += phase_bishop;
		ei->material_value[turn] += material_value[bishop];

		int square = ctz(b);
		uint64_t squareb = bitboard(square);
		
		/* Attacks, including x-rays through both sides' queens and own bishops. */
		uint64_t attacks = bishop_attacks(square, 0, all_pieces(pos) ^ pos->piece[white][queen] ^ pos->piece[black][queen] ^ pos->piece[turn][bishop]);
		ei->attacked2_squares[turn] |= attacks & ei->attacked_squares[turn][all];
		ei->attacked_squares[turn][bishop] |= attacks;
		ei->attacked_squares[turn][all] |= attacks;
		/* mobility (range of popcount is [0, 13]) */
		ei->mobility[turn] += mobility_bonus[bishop - 2][popcount(attacks & ei->mobility_squares[turn])];

		/* king attacks */
		if (ei->king_ring[1 - turn] & attacks)
			ei->king_attack_units[turn][bishop] += popcount(ei->king_ring[1 - turn] & attacks) + 1;

		/* bishop pair */
		if (popcount(b) >= 2) {
			//ei->bishop_pair[turn] += 1;
			eval += bishop_pair;
		}

		/* minor behind pawn */
		if (squareb & shift_color(pos->piece[turn][pawn], 1 - turn)) {
			//ei->minor_behind_pawn[turn] += 1;
			eval += minor_behind_pawn;
		}

		/* defended minor */
		if (squareb & ei->attacked_squares[turn][pawn]) {
			//ei->defended_minor[turn] += 1;
			eval += defended_minor;
		}
		
		/* penalty if piece is far from own king */
		//ei->bishop_far_from_king[turn] += distance(square, ctz(pos->piece[turn][king]));
		eval += bishop_far_from_king * distance(square, ctz(pos->piece[turn][king]));

		/* penalty for own pawns on same squares as bishop */
		//ei->pawn_on_bishop_square[turn] += popcount(same_colored_squares(square) & pos->piece[turn][pawn]);
		eval += pawn_on_bishop_square * popcount(same_colored_squares(square) & pos->piece[turn][pawn]);

		b = clear_ls1b(b);
	}
	return eval;
}

mevalue evaluate_rooks(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	uint64_t b = pos->piece[turn][rook];
	while (b) {
		ei->material += phase_rook;
		ei->material_value[turn] += material_value[rook];

		int square = ctz(b);
		
		/* Attacks, including x-rays through both sides' queens and own rooks. */
		uint64_t attacks = rook_attacks(square, 0, all_pieces(pos) ^ pos->piece[white][queen] ^ pos->piece[black][queen] ^ pos->piece[turn][rook]);
		ei->attacked2_squares[turn] |= attacks & ei->attacked_squares[turn][all];
		ei->attacked_squares[turn][rook] |= attacks;
		ei->attacked_squares[turn][all] |= attacks;
		/* mobility (range of popcount is [0, 14]) */
		int mobility = popcount(attacks & ei->mobility_squares[turn]);
		ei->mobility[turn] += mobility_bonus[rook - 2][mobility];

		/* king attacks */
		if (ei->king_ring[1 - turn] & attacks)
			ei->king_attack_units[turn][rook] += popcount(ei->king_ring[1 - turn] & attacks) + 1;

		/* bonus on semiopen files */
		if (!(pos->piece[turn][pawn] & file(square))) {
			//ei->rook_on_open_file[turn] += 1;
			eval += rook_on_open_file;
		}

		/* penalty if blocked by uncastled king */
		else if (mobility <= 5) {
			int kf = ctz(pos->piece[turn][king]) % 8;
			if ((kf < e1) == ((square % 8) < kf)) {
				//ei->blocked_rook[turn] += (1 + !(pos->castle & (turn ? 0x3 : 0xC)));
				eval += blocked_rook * (1 + !(pos->castle & (turn ? 0x3 : 0xC)));
			}
		}

		b = clear_ls1b(b);
	}
	return eval;
}

mevalue evaluate_queens(const struct position *pos, struct evaluationinfo *ei, int turn) {
	mevalue eval = 0;
	uint64_t b = pos->piece[turn][queen];
	/* Only add the material value of 1 queen per side. */
	while (b) {
		ei->material += phase_queen;
		ei->material_value[turn] += material_value[queen];

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
			ei->king_attack_units[turn][queen] += popcount(ei->king_ring[1 - turn] & attacks) + 1;

		/* undeveloped minor pieces when moving queen */
		if (turn) {
			//ei->undeveloped_piece[white] += (square != d1) * popcount((pos->piece[white][knight] | pos->piece[white][bishop]) & RANK_1);
			eval += (square != d1) * S(undeveloped_piece, 0) * popcount((pos->piece[white][knight] | pos->piece[white][bishop]) & RANK_1);
		}
		else {
			//ei->undeveloped_piece[black] += (square != d8) * popcount((pos->piece[black][knight] | pos->piece[black][bishop]) & RANK_8);
			eval += (square != d8) * S(undeveloped_piece, 0) * popcount((pos->piece[black][knight] | pos->piece[black][bishop]) & RANK_8);
		}

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

mevalue evaluate_turn(const struct position *pos, struct evaluationinfo *ei, int turn) {
	UNUSED(ei);
	return (pos->turn == turn) * tempo_bonus;
}

void evaluationinfo_init(const struct position *pos, struct evaluationinfo *ei) {
	ei->attacked_squares[white][pawn] = shift_north_west(pos->piece[white][pawn]) | shift_north_east(pos->piece[white][pawn]);
	ei->attacked_squares[black][pawn] = shift_south_west(pos->piece[black][pawn]) | shift_south_east(pos->piece[black][pawn]);

	ei->pawn_attack_span[white] = fill_north(ei->attacked_squares[white][pawn]);
	ei->pawn_attack_span[black] = fill_south(ei->attacked_squares[black][pawn]);

	for (int turn = 0; turn < 2; turn++) {
		ei->mobility_squares[turn] = ~pos->piece[turn][king];
		/* enemy pawn attacks */
		ei->mobility_squares[turn] &= ~ei->attacked_squares[1 - turn][pawn];
		/* blocked pawns */
		ei->mobility_squares[turn] &= ~(shift_color(pos->piece[black][all] | pos->piece[white][all], 1 - turn) & pos->piece[turn][pawn]);

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
	for (int piece = pawn; piece <= king; piece++) {
		bitboard = pos->piece[turn][piece];
		while (bitboard) {
			square = ctz(bitboard);
			eval += psqtable[turn][piece][square];
			bitboard = clear_ls1b(bitboard);
		}
	}

	return eval;
}

int32_t phase(const struct evaluationinfo *ei) {
	int32_t phase = CLAMP(ei->material, phase_min_material, phase_max_material);
	phase = (phase - phase_min_material) * PHASE / (phase_max_material - phase_min_material);
	return phase;
}

int32_t scale(const struct position *pos, const struct evaluationinfo *ei, int strongside) {
	int weakside = 1 - strongside;
	int32_t scale = NORMAL_SCALE;
	int32_t strongmaterial = ei->material_value[strongside];
	int32_t weakmaterial = ei->material_value[weakside];
	/* Scores also KBBKN as a draw. */
	if (!pos->piece[strongside][pawn] && strongmaterial - weakmaterial <= material_value[bishop])
		scale = (strongmaterial <= material_value[bishop]) ? 0 : (weakmaterial <= material_value[bishop]) ? 16 : 32;
	return scale;
}

int32_t evaluate_tapered(const struct position *pos, const struct evaluationinfo *ei) {
	int32_t p = phase(ei);
	int strongside = mevalue_eg(ei->eval) > 0;
	int32_t s = scale(pos, ei, strongside);
	int32_t ret = p * mevalue_mg(ei->eval) + (PHASE - p) * s * mevalue_eg(ei->eval) / NORMAL_SCALE;
	return ret / PHASE;
}

int32_t evaluate_classical_ei(const struct position *pos, struct evaluationinfo *ei) {
	evaluationinfo_init(pos, ei);

	ei->eval = evaluate_psqtable(pos, ei, white) - evaluate_psqtable(pos, ei, black);
	ei->eval += evaluate_pawns(pos, ei, white) - evaluate_pawns(pos, ei, black);
	ei->eval += evaluate_knights(pos, ei, white) - evaluate_knights(pos, ei, black);
	ei->eval += evaluate_bishops(pos, ei, white) - evaluate_bishops(pos, ei, black);
	ei->eval += evaluate_rooks(pos, ei, white) - evaluate_rooks(pos, ei, black);
	ei->eval += evaluate_queens(pos, ei, white) - evaluate_queens(pos, ei, black);
	ei->eval += evaluate_king(pos, ei, white) - evaluate_king(pos, ei, black);
	ei->eval += evaluate_mobility(pos, ei, white) - evaluate_mobility(pos, ei, black);
	ei->eval += evaluate_space(pos, ei, white) - evaluate_space(pos, ei, black);
	ei->eval += evaluate_turn(pos, ei, white) - evaluate_turn(pos, ei, black);

	int32_t ret = evaluate_tapered(pos, ei);
	return pos->turn ? ret : -ret;
}

int32_t evaluate_classical(const struct position *pos) {
	struct evaluationinfo ei = { 0 };
	return evaluate_classical_ei(pos, &ei);
}

void print_mevalue(mevalue eval);
mevalue evaluate_print_x(const char *name, const struct position *pos, struct evaluationinfo *ei,
		mevalue (*evaluate_x)(const struct position *, struct evaluationinfo *ei, int));

void evaluate_print(struct position *pos) {
	struct evaluationinfo ei = { 0 };
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
	double wt = (double)(pos->turn == white) * tempo_bonus / 100;
	double bt = (double)(pos->turn == black) * tempo_bonus / 100;
	eval += (2 * pos->turn - 1) * S(tempo_bonus, tempo_bonus);
	printf("| Tempo       | %+.2f %+.2f | %+.2f %+.2f | %+.2f %+.2f |\n", wt, wt, bt, bt, wt - bt, wt - bt);
	printf("+-------------+-------------+-------------+-------------+\n");
	printf("| Total       |             |             | ");
	print_mevalue(eval);
	printf(" |\n");
	printf("+-------------+-------------+-------------+-------------+\n");
	printf("Phase: %.2f\n", (double)phase(&ei) / PHASE);
	
	char pieces[] = " PNBRQKpnbrqk";

	alignas(64) int16_t accumulation[2][K_HALF_DIMENSIONS];
	alignas(64) int32_t psqtaccumulation[2] = { 0 };
	printf("\n+-------+-------+-------+-------+-------+-------+-------+-------+\n");
	for (int y = 7; y >= 0; y--) {
		printf("|");
		for (int x = 0; x < 8; x++) {
			printf("   %c   |", pieces[pos->mailbox[x + 8 * y]]);
		}
		printf("\n|");
		for (int x = 0; x < 8; x++) {
			int square = x + 8 * y;
			int piece = pos->mailbox[square];
			if (piece % 6) {
				int16_t oldeval = psqtaccumulation[white] - psqtaccumulation[black];
				for (int color = 0; color < 2; color++) {
					int king_square = ctz(pos->piece[color][king]);
					king_square = orient(color, king_square);
					int index = make_index(color, square, piece, king_square);
					add_index_slow(index, accumulation, psqtaccumulation, color);
				}
				int16_t neweval = psqtaccumulation[white] - psqtaccumulation[black] - oldeval;
				
				if (ABS(neweval) >= 2000)
					printf(" %+.1f |", (double)neweval / 200);
				else
					printf(" %+.2f |", (double)neweval / 200);
			}
			else {
				printf("       |");
			}
		}
		printf("\n+-------+-------+-------+-------+-------+-------+-------+-------+\n");
	}
	printf("Psqt: %+.2f\n", (double)(psqtaccumulation[white] - psqtaccumulation[black]) / 200);
	printf("Positional %+.2f\n", (double)((2 * pos->turn - 1) * evaluate_nnue(pos) - (psqtaccumulation[white] - psqtaccumulation[black]) / 2) / 100);
	printf("\n");
	printf("Classical Evaluation: %+.2f\n", (double)0 / 100);
	printf("NNUE Evaluation: %+.2f\n", (double)(2 * pos->turn - 1) * evaluate_nnue(pos) / 100);
}

void print_mevalue(mevalue eval) {
	int m = mevalue_mg(eval);
	int e = mevalue_eg(eval);
	if (ABS(m) >= 10000)
		printf("%+.0f ", (double)m / 100);
	else if (ABS(m) >= 1000)
		printf("%+.1f", (double)m / 100);
	else
		printf("%+.2f", (double)m / 100);
	printf(" ");
	if (ABS(e) >= 10000)
		printf("%+.0f ", (double)e / 100);
	else if (ABS(e) >= 1000)
		printf("%+.1f", (double)e / 100);
	else
		printf("%+.2f", (double)e / 100);
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
