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

#include "evaluate.h"

#include <string.h>

#include "init.h"

mevalue psqtable[2][7][64];

mevalue piece_value[6] = { S(pawn_mg, pawn_eg), S(knight_mg, knight_eg), S(bishop_mg, bishop_eg), S(rook_mg, rook_eg), S(queen_mg, queen_eg), S(0, 0) };

/* from white's perspective, files a to d on a regular board */
mevalue white_psqtable[6][64] = {
	{ /* pawn */
		S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
		S( 12, 18), S( 11, 29), S(  9, 36), S( 17, 29), S( 12, 19), S(  9, 33), S(  6, 18), S(  7,  5),
		S(  9, 10), S( 12, 22), S( 11, 29), S( 19, 16), S( 19, 12), S( 20, 15), S(  9, 15), S( 38, 12),
		S(  0, -2), S( -2,  0), S( -3, -7), S(  8, -9), S(  8, -5), S(  0,  2), S( -4,  0), S( -9, -5),
		S(-10,-15), S(-14,-12), S(  2,-11), S( 13, -8), S(  9, -7), S( 29, -2), S( -7, -7), S(-12,-22),
		S( -9,-18), S( -7,-13), S( -3, -6), S( -4, -1), S(  2,  4), S( -2,  1), S( 14, -9), S( -6,-22),
		S(-12, -9), S(  8, -7), S( -5,  0), S(  3, -9), S(-11,  5), S( 18,  6), S( 23, -6), S( -3,-15),
		S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
	}, { /* knight */
		S(-89,-56), S(-33,-36), S(-30,-26), S(-25,-26),
		S(-29,-33), S(-19,-18), S(-14, -4), S( -3,  6),
		S(-25,-27), S(  5,  3), S( 11, 12), S( 17, 13),
		S(-11,-17), S(  2,  0), S( 17, 12), S(  9, 12),
		S(-15,-22), S(  2,  1), S(  1, 10), S(  1,  6),
		S(-20,-35), S(  5, -4), S( -7, -3), S(  0,  2),
		S(-29,-35), S(-18,-24), S( -9,-13), S( -5, -5),
		S(-73,-47), S( -4,-31), S(-22,-30), S(-17,-27),
	}, { /* bishop */
		S(-24,-50), S(-17,-32), S(-15,-23), S(-14,-26),
		S(-26,-31), S(  0,-15), S(  0, -8), S(  0,  2),
		S( -1,-24), S(  7,  1), S( 13,  8), S(  9,  3),
		S( -9,-18), S(  2, -1), S( 10,  5), S( 13,  5),
		S( -1,-21), S(  3, -2), S( -1,  1), S(  9,  1),
		S(  7,-30), S( 10, -5), S(  5, -2), S(  3,  1),
		S( -4,-33), S( 18,-14), S(  5,-11), S( -9, -7),
		S(-17,-42), S( -1,-28), S(-13,-17), S( -8,-24),
	}, { /* rook */
		S( -5,-31), S( -8,-23), S( -6,-12), S( -5,-12),
		S( -2, -9), S(  3, -9), S( 14, -4), S( 15,  4),
		S( -8,-12), S( -1, -3), S(  0,  2), S(  1,  7),
		S(-17,-11), S(-10, -4), S(  0,  1), S(  1,  1),
		S(-22,-15), S(-11, -8), S( -9,  0), S( -6, -5),
		S(-21,-28), S(-15,-12), S( -6,-12), S( -7,-16),
		S(-43,-34), S(-12,-25), S( -2,-20), S(  0,-23),
		S( -4,-30), S( -7,-30), S( -1,-22), S(  8,-24),
	}, { /* queen */
		S( -9,-52), S( -3,-34), S( -3,-26), S( -2,-25),
		S( -8,-31), S(-12,-17), S( -1, -6), S(  0,  5),
		S(  1,-26), S(  2,  3), S(  4, 11), S(  4, 13),
		S( -2,-19), S( -6,  3), S( -3, 12), S( -2, 10),
		S(  1,-24), S(  2,  0), S( -1,  8), S( -5,  7),
		S( -7,-34), S(  9, -2), S(  4, -4), S( -3, -1),
		S(-10,-35), S(  2,-24), S( 11,-18), S( 11, -9),
		S(-10,-47), S( -6,-32), S( -4,-32), S(  2,-29),
	}, { /* king */
		S(-85,-50), S(-93,-30), S(-97,-21), S(-90, -7),
		S(-78,-20), S(-76,  5), S(-78, 11), S(-82, 12),
		S(-41,-16), S(-59, 23), S(-71, 27), S(-80, 16),
		S(-23,-19), S(-39,  6), S(-41, 11), S(-50,  6),
		S(-12,-27), S(-19, -1), S(-29,  8), S(-32,  8),
		S( -1,-31), S( -2,-15), S( -8, -3), S(-17,  6),
		S( 30,-30), S( 16,-21), S( 19,-12), S(  0,-11),
		S( 27,-48), S( 29,-26), S( 34,-29), S( 69,-36),
	}
};

mevalue pawn_shelter[4][7] = {
	{
		S( -9, 11), S( 40,-11), S( 53, -4), S( 26, -2), S( 13, -2), S( 15,  2), S(  5,  3),
	}, {
		S(-40,  6), S( 30, -2), S( 18, -3), S(-22,-11), S(-25, -2), S(-34,  2), S(-31,  5),
	}, {
		S(-12,  2), S( 29,  4), S( 10, -1), S(-11, -7), S(-11, -3), S(-18,  0), S(-12,  6),
	}, {
		S(-41,  0), S(-15,  0), S(-23, -1), S(-24,  0), S(-33,  6), S(-54,  2), S(-68,  3),
	}
};

mevalue unblocked_storm[4][7] = {
	{
		S( -4, -7), S(  0,  4), S( 10, 11), S(-52,  6), S(-18, -1), S( -8, -9), S( -9, -1),
	}, {
		S( -8, -6), S( -8,  2), S(-22,  7), S(-18,  4), S(-13,  0), S(  6,  0), S( -1, -4),
	}, {
		S( -5, -5), S(-31,  0), S(-30,  6), S(-21,  6), S(  4,  2), S(  1,  0), S(  5, -1),
	}, {
		S( -9, -3), S(  6,  0), S(-15,  3), S(-17,  6), S(  0,  3), S( 16,  6), S(  5, -1),
	}
};

mevalue blocked_storm[7] = {
	S(  0,  0), S(-12, -7), S(-16, -3), S(  8,-10), S(  2,-12), S( 14,  0), S(  4,  0),
};

mevalue mobility_bonus[4][28] = {
	{	/* idx 0 to 8 */
		S(-50,-61), S(-30,-40), S( -7,-19), S( -1,  1), S( 11,  3), S( 13,  6), S( 21,  9), S( 27, 12), S( 29, -2),
	}, {	/* idx 0 to 13 */
		S(-48,-51), S(-23,-32), S( -3,  1), S( 10, 16), S( 16, 26), S( 20, 35), S( 25, 42), S( 26, 49), S( 31, 45),
		S( 33, 47), S( 36, 45), S( 42, 34), S( 47, 55), S( 49, 31),
	}, {	/* idx 0 to 14 */
		S(-34,-51), S(-25,-32), S(  2,-25), S(  8,  0), S(  5,  6), S(  7,  6), S(  3, 15), S(  5, 21), S( 14, 24),
		S( 17, 31), S( 20, 37), S( 25, 38), S( 32, 41), S( 30, 41), S( 39, 41),
	}, {	/* idx 0 to 27 */
		S(-10,-21), S( -8,-19), S(  0,-11), S( 13, -1), S( 17,  3), S( 15,  7), S( 17, 11), S( 14, 14), S( 16, 12),
		S( 19, 20), S( 20, 25), S( 24, 29), S( 28, 30), S( 27, 38), S( 26, 42), S( 30, 42), S( 30, 47), S( 30, 49),
		S( 32, 53), S( 34, 59), S( 36, 61), S( 40, 62), S( 40, 69), S( 44, 67), S( 48, 80), S( 52, 80), S( 53, 91),
		S( 52, 97),
	}
};

void tables_init(void) {
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
