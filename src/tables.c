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
