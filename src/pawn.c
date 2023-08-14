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

mevalue backward_pawn  = S(-9, 8);
mevalue supported_pawn = S( 13, -5);
mevalue isolated_pawn  = S(-7, -18);
mevalue doubled_pawn   = S(-11,-17);
mevalue connected_pawns[7] = { S(  0,  0), S(  1, -1), S(  2,  2), S(  6,  4), S(  8,  7), S( 19, 17), S(106, 47) };
mevalue passed_pawn[7]    = { S(  0,  0), S(-24, -5), S(-20,  0), S(  0, 14), S( 31, 28), S( 59, 38), S(123, 37), };
mevalue passed_file[4]   = { S( 34, 31), S( 24, 26), S(  8, 11), S( -4,  6), };

/* mostly inspiration from stockfish */
mevalue evaluate_pawns(const struct position *pos, struct evaluationinfo *ei, int color) {
	UNUSED(ei);
	uint64_t pawns[2];
	if (color) {
		pawns[white] = pos->piece[white][pawn];
		pawns[black] = pos->piece[black][pawn];
	}
	else {
		pawns[white] = rotate_bytes(pos->piece[black][pawn]);
		pawns[black] = rotate_bytes(pos->piece[white][pawn]);
	}
	/* we are now always evaluating from white's perspective */
	mevalue eval = 0;

	uint64_t b = pawns[white];
	uint64_t neighbours, doubled, stoppers, support, phalanx, lever, leverpush, blocker;
	int backward, passed;
	int square;
	uint64_t squareb;
	while (b) {
		square = ctz(b);
		squareb = bitboard(square);

		int r = rank_of(square);
		int f = file_of(square);
		
		/* uint64_t */
		doubled    = pawns[white] & bitboard(square - 8);
		neighbours = pawns[white] & adjacent_files(square);
		stoppers   = pawns[black] & passed_files(square, white);
		blocker    = pawns[black] & bitboard(square + 8);
		support    = neighbours & rank(square - 8);
		phalanx    = neighbours & rank(square);
		lever      = pawns[black] & (shift_north_west(squareb) | shift_north_east(squareb));
		leverpush  = pawns[black] & (shift_north(shift_north_west(squareb) | shift_north_east(squareb)));

		/* int */
		backward   = !(neighbours & passed_files(square + 8, black)) && (leverpush | blocker);
		passed     = !(stoppers ^ lever) || (!(stoppers ^ lever ^ leverpush) && popcount(phalanx) >= popcount(leverpush));
		passed    &= !(passed_files(square, white) & file(square) & pawns[white]);

		if (backward) {
			//ei->backward_pawn[color] += 1;
			eval += backward_pawn;
		}

		if (support | phalanx) {
			//ei->supported_pawn[color] += popcount(support);
			//ei->connected_pawns[color][r] += 2 + (phalanx != 0);
			eval += connected_pawns[r] * (2 + (phalanx != 0)) + supported_pawn * popcount(support);
		}

		if (passed) {
			//ei->passed_pawn[color][r] += 1;
			//ei->passed_file[color][MIN(f, 7 - f)] += 1;
			eval += passed_pawn[r] + passed_file[MIN(f, 7 - f)];
		}

		if (!neighbours) {
			//ei->isolated_pawn[color] += 1;
			eval += isolated_pawn;
		}
		
		if (!support && doubled) {
			//ei->doubled_pawn[color] += 1;
			eval += doubled_pawn;
		}

		b = clear_ls1b(b);
	}

	return eval;
}
