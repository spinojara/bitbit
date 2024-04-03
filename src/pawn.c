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

#include "pawn.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "bitboard.h"
#include "util.h"
#include "option.h"
#include "texelbit.h"

CONST score_t supported_pawn     = S(  9, 12);
CONST score_t backward_pawn[4]   = { S(  2,  0), S( -6,-10), S( -7, -5), S( -7, -5), };
CONST score_t isolated_pawn[4]   = { S( -2,  1), S( -4, -8), S( -6, -3), S( -9, -5), };
CONST score_t doubled_pawn[4]    = { S(-10,-55), S(  5,-33), S(  5,-20), S( -3,-12), };
CONST score_t connected_pawn[7]  = { S(  0,  0), S(  2,  2), S(  4,  4), S(  4,  4), S(  8,  9), S( 31, 34), S( 70, 87), };
CONST score_t passed_pawn[7]     = { S(  0,  0), S( 53, 31), S( 36, 36), S(-34, 73), S(-41,114), S(175,152), S(432,282), };
CONST score_t passed_blocked[7]  = { S(  0,  0), S(  1, -1), S(  1,  3), S(  2,-14), S(  0,-17), S(  3,-35), S(-68,-93), };
CONST score_t passed_file[4]     = { S( -1, 11), S(-20,  7), S(-22, -5), S(-16,-20), };
CONST score_t distance_us[7]     = { S(  0,  0), S( -2, -3), S(  4, -9), S( 18,-18), S( 16,-23), S(-13,-20), S(-37,-20), };
CONST score_t distance_them[7]   = { S(  0,  0), S( -8, -2), S(-11,  2), S(-10, 13), S( -5, 25), S(-12, 41), S(  3, 42), };

/* Mostly inspiration from stockfish. */
score_t evaluate_pawns(const struct position *pos, struct evaluationinfo *ei, int us) {
	const int them = other_color(us);
	const unsigned up = us ? N : S;

	UNUSED(ei);
	score_t eval = 0;

	const int up_sq = us ? 8 : -8;
	const int down_sq = -up_sq;

	uint64_t ourpawns = pos->piece[us][PAWN];
	uint64_t theirpawns = pos->piece[them][PAWN];
	uint64_t b = pos->piece[us][PAWN];
	uint64_t neighbours, doubled, stoppers, support, phalanx, lever, leverpush, blocker;
	int backward, passed;
	int square;
	uint64_t squareb;
	while (b) {
		square = ctz(b);
		squareb = bitboard(square);

		int r = rank_of(orient_horizontal(us, square));
		int f = file_of(square);
		int rf = min(f, 7 - f);
		
		/* uint64_t */
		doubled    = ourpawns & bitboard(square + down_sq);
		neighbours = ourpawns & adjacent_files(square);
		stoppers   = theirpawns & passed_files(square, us);
		blocker    = theirpawns & bitboard(square + up_sq);
		support    = neighbours & rank(square + down_sq);
		phalanx    = neighbours & rank(square);
		lever      = theirpawns & shift(shift(squareb, E) | shift(squareb, W), up);
		leverpush  = theirpawns & shift_twice(shift(squareb, E) | shift(squareb, W), up);

		/* int */
		backward   = !(neighbours & passed_files(square + up_sq, them)) && (leverpush | blocker);
		passed     = !(stoppers ^ lever) || (!(stoppers ^ lever ^ leverpush) && popcount(phalanx) >= popcount(leverpush));
		passed    &= !(passed_files(square, us) & file(square) & ourpawns);

		if (support | phalanx) {
			eval += connected_pawn[r] * (2 + (phalanx != 0)) + supported_pawn * popcount(support);
			if (TRACE) trace.supported_pawn[us] += popcount(support);
			if (TRACE) trace.connected_pawn[us][r] += 2 + (phalanx != 0);
		}

		if (passed) {
			eval += passed_pawn[r] + passed_file[rf];
			if (TRACE) trace.passed_pawn[us][r]++;
			if (TRACE) trace.passed_file[us][rf]++;

			int i = distance(square + up_sq, ctz(pos->piece[us][KING]));
			eval += i * distance_us[r];
			if (TRACE) trace.distance_us[us][r] += i;
			int j = distance(square + up_sq, ctz(pos->piece[them][KING]));
			eval += j * distance_them[r];
			if (TRACE) trace.distance_them[us][r] += j;

			if (pos->mailbox[square + up_sq]) {
				eval += passed_blocked[r];
				if (TRACE) trace.passed_blocked[us][r]++;
			}
		}
		else if (!neighbours) {
			eval += isolated_pawn[rf];
			if (TRACE) trace.isolated_pawn[us][rf]++;
		}
		if (doubled) {
			eval += doubled_pawn[rf];
			if (TRACE) trace.doubled_pawn[us][rf]++;
		}
		
		if (backward) {
			eval += backward_pawn[rf];
			if (TRACE) trace.backward_pawn[us][rf]++;
		}

		b = clear_ls1b(b);
	}

	return eval;
}
