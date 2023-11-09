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

#include "pawn.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "bitboard.h"
#include "util.h"
#include "option.h"
#include "texeltune.h"

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
score_t evaluate_pawns(const struct position *pos, struct evaluationinfo *ei, int color) {
	UNUSED(ei);
	score_t eval = 0;

	unsigned up = color ? 8 : -8;
	unsigned down = color ? -8 : 8;

	uint64_t ourpawns = pos->piece[color][pawn];
	uint64_t theirpawns = pos->piece[other_color(color)][pawn];
	uint64_t b = pos->piece[color][pawn];
	uint64_t neighbours, doubled, stoppers, support, phalanx, lever, leverpush, blocker;
	int backward, passed;
	int square;
	uint64_t squareb;
	while (b) {
		square = ctz(b);
		squareb = bitboard(square);

		int r = rank_of(orient_horizontal(color, square));
		int f = file_of(square);
		int rf = MIN(f, 7 - f);
		
		/* uint64_t */
		doubled    = ourpawns & bitboard(square + down);
		neighbours = ourpawns & adjacent_files(square);
		stoppers   = theirpawns & passed_files(square, color);
		blocker    = theirpawns & bitboard(square + up);
		support    = neighbours & rank(square + down);
		phalanx    = neighbours & rank(square);
		lever      = theirpawns & shift_color(shift_west(squareb) | shift_east(squareb), color);
		leverpush  = theirpawns & shift_color2(shift_west(squareb) | shift_east(squareb), color);

		/* int */
		backward   = !(neighbours & passed_files(square + up, other_color(color))) && (leverpush | blocker);
		passed     = !(stoppers ^ lever) || (!(stoppers ^ lever ^ leverpush) && popcount(phalanx) >= popcount(leverpush));
		passed    &= !(passed_files(square, color) & file(square) & ourpawns);

		if (support | phalanx) {
			eval += connected_pawn[r] * (2 + (phalanx != 0)) + supported_pawn * popcount(support);
			if (TRACE) trace.supported_pawn[color] += popcount(support);
			if (TRACE) trace.connected_pawn[color][r] += 2 + (phalanx != 0);
		}

		if (passed) {
			eval += passed_pawn[r] + passed_file[rf];
			if (TRACE) trace.passed_pawn[color][r]++;
			if (TRACE) trace.passed_file[color][rf]++;

			int i = distance(square + up, ctz(pos->piece[color][king]));
			eval += i * distance_us[r];
			if (TRACE) trace.distance_us[color][r] += i;
			int j = distance(square + up, ctz(pos->piece[other_color(color)][king]));
			eval += j * distance_them[r];
			if (TRACE) trace.distance_them[color][r] += j;

			if (pos->mailbox[square + up]) {
				eval += passed_blocked[r];
				if (TRACE) trace.passed_blocked[color][r]++;
			}
		}
		else if (!neighbours) {
			eval += isolated_pawn[rf];
			if (TRACE) trace.isolated_pawn[color][rf]++;
		}
		if (doubled) {
			eval += doubled_pawn[rf];
			if (TRACE) trace.doubled_pawn[color][rf]++;
		}
		
		if (backward) {
			eval += backward_pawn[rf];
			if (TRACE) trace.backward_pawn[color][rf]++;
		}

		b = clear_ls1b(b);
	}

	return eval;
}
