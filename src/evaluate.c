/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2025 Isak Ellmer
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

#include "evaluate.h"

#include <string.h>

#include "bitboard.h"
#include "util.h"
#include "attackgen.h"
#include "movegen.h"
#include "nnue.h"
#include "option.h"

const int material_value[7] = { 0, 100, 300, 300, 500, 1000, 0 };

void evaluate_print(struct position *pos) {
	char pieces[] = " PNBRQKpnbrqk";

	alignas(64) int16_t accumulation[2][K_HALF_DIMENSIONS];
	alignas(64) int32_t psqtaccumulation[2] = { 0 };
	printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");
	for (int r = 7; r >= 0; r--) {
		printf("|");
		for (int f = 0; f < 8; f++) {
			printf("   %c   |", pieces[pos->mailbox[make_square(f, r)]]);
		}
		printf("\n|");
		for (int f = 0; f < 8; f++) {
			int square = make_square(f, r);
			int piece = pos->mailbox[square];
			if (uncolored_piece(piece) && uncolored_piece(piece) != KING) {
				int16_t oldeval = psqtaccumulation[WHITE] - psqtaccumulation[BLACK];
				for (int color = 0; color < 2; color++) {
					int king_square = ctz(pos->piece[color][KING]);
					int index = make_index(color, square, piece, king_square);
					add_index_slow(index, accumulation, psqtaccumulation, color);
				}
				int16_t neweval = psqtaccumulation[WHITE] - psqtaccumulation[BLACK] - oldeval;

				if (abs(neweval) >= 2000)
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
	printf("Psqt: %+.2f\n", (double)(psqtaccumulation[WHITE] - psqtaccumulation[BLACK]) / 200);
	int32_t eval = evaluate_nnue(pos);
	printf("Positional %+.2f\n", (double)((2 * pos->turn - 1) * eval - (psqtaccumulation[WHITE] - psqtaccumulation[BLACK]) / 2) / 100);
	printf("NNUE Evaluation: %+.2f\n", (double)(2 * pos->turn - 1) * eval / 100);
}
