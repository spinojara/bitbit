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

mevalue psqtable[2][7][64];

//mevalue piece_value[6] = { S( 90, 125), S( 420, 351), S( 450, 352), S( 576, 597), S(1237, 1077), };
mevalue piece_value[6] = { S(  79, 139), S( 418, 504), S( 453, 545), S( 537, 869), S(1182,1695), };

/* from white's perspective, files a to d on a regular board */
mevalue white_psqtable[6][64] = {
	{ /* pawn */
		S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
		S( 36,  6), S( 15,  8), S( 10, -4), S( -2, -6), S( 24, -1), S(-21,  7), S( 36, 13), S(-34, 23),
		S( 14, 19), S(-10, 15), S(-11,-10), S( 30,-20), S( 11,-20), S( -5,  0), S( 19, -1), S(  6, 11),
		S(  3, 17), S( -6,  6), S(-13,  4), S(  2, -6), S( -9, -5), S( -2,  4), S( 16,  4), S( 11,  4),
		S( -5, -4), S(-13,  1), S(  0,  3), S(  3,  1), S( -1,  1), S( 11,  2), S( -4, -1), S( -1, -6),
		S(-14, -9), S(-22, -5), S(  4,  4), S( -2,  8), S(  5,  3), S( -1,  4), S(  1, -7), S(-10,-19),
		S(-15, -7), S(  1, -1), S( 11,  5), S( 11, 10), S(  8, 10), S( 14, 12), S( 16, -3), S( -9,-11),
		S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
	}, { /* knight */
		S(-77,-27), S(-73,-11), S(-57,-11), S(-24,  0),
		S( -2,-17), S(-16, 20), S( 21,  0), S( 31, -8),
		S( 23, -4), S( 19,  3), S( 38, 10), S( 54,  8),
		S( 24,  1), S(  3, 14), S(  9, 17), S(  7, 30),
		S(  2, 10), S( -6,  4), S(  8, 13), S( -2, 25),
		S( -7, -9), S( 10,-15), S(  0, -8), S(  5,  5),
		S( -6,-21), S(  9,-16), S( -6, -2), S( 10, -2),
		S(-32,  7), S(-14,-19), S(-15,-17), S( -8,-12),
	}, { /* bishop */
		S(  6, -5), S(-71, -2), S(-71, 12), S(-57, -3),
		S(-20,  0), S(  5,  5), S( -3,  6), S(-11,  2),
		S( -1,  7), S( -6, 19), S( 41, 10), S( 36,  8),
		S( -6, -8), S(  0,  9), S( -7,  7), S( 17, 21),
		S(  5, -4), S(-19, -3), S(-11,  6), S( -3, 18),
		S( 17, -7), S(  9, -9), S( 10,  2), S( -8, 12),
		S( 15, -5), S( 20, -9), S(  6, -9), S( -1, -6),
		S( -6, 12), S( -9,-19), S(-20, -1), S( -9, -6),
	}, { /* rook */
		S( 62, 10), S( 38, 25), S( 10, 28), S(  3, 17),
		S( 29, 26), S( 17, 25), S( 36, 22), S( 59, 12),
		S(  9, 26), S( 16, 17), S( 39, 16), S( 53, 15),
		S( 12, 10), S( -5, 21), S( 18, 16), S( 24,  9),
		S( -9,  2), S(-21,  8), S(-18, 10), S(-12,  1),
		S(-22, -9), S(-19,-10), S(-33,-11), S(-16,-11),
		S(-36,-25), S( -7,-31), S( -9,-20), S(  4,-30),
		S(  1,-13), S( -5,-12), S(  5,-15), S( 15,-21),
	}, { /* queen */
		S(  1,-12), S( -6,  7), S(-11, 22), S(  4,  8),
		S(  0, -1), S(  4,  4), S(-12, 28), S(-25, 52),
		S(  7,-10), S( -2, 18), S(  3, 37), S(  1, 41),
		S( -6, 14), S(-15, 48), S(-24, 48), S(-18, 68),
		S( -7, 19), S(-14, 32), S(-15, 41), S(-10, 62),
		S(  4,-15), S(  4,  0), S( -2,  1), S( -1, 22),
		S( -3,-20), S( 15,-45), S( 14,-37), S( 13,-26),
		S(  6,-66), S(-10,-53), S(  1,-67), S(  7,-49),
	}, { /* king */
		S(-23,-88), S( 77, 33), S( 29,  1), S( 40,  1),
		S( 21,  1), S( 28, 47), S( 64, 43), S( 34, 32),
		S(-32, 35), S(  8, 88), S( 26, 41), S( 39, 47),
		S(-39, 29), S(-30, 64), S(  4, 52), S(  6, 52),
		S(-54,  2), S(-49, 42), S(-54, 28), S(-56, 39),
		S(-10,-10), S(-45, 17), S(-30, 10), S(-53, 16),
		S( -3,-25), S(-22,  1), S( -1,-12), S(-20, -7),
		S( 15,-57), S( 14,-32), S(  2,-49), S( 21,-57),
	}
};

mevalue mobility_bonus[4][28] = {
        {
                S(-44,-65), S(-29,-80), S(-18,-46), S(-11,-14), S( -4, -5), S(  3, 10), S( 12, 18), S( 20, 23), S( 26, 15),
        }, {
                S(-72,-119), S(-39,-110), S(-24,-58), S( -3,-20), S(  6, -7), S( 12, 10), S( 17, 18), S( 17, 24), S( 17, 30),
                S( 23, 29), S( 31, 29), S( 48, 27), S( 34, 19), S( 41, 21),
        }, {
                S(-101,-81), S(-38,-80), S(-17,-55), S( -2,-28), S( -1,-21), S(  3,-19), S(  1, -1), S(  4,  1), S(  9,  7),
                S( 14, 14), S( 21, 19), S( 29, 25), S( 30, 23), S( 27, 23), S( 48, 16),
        }, {
                S(-62,-53), S(-90,-74), S(-66,-75), S(-41,-118), S(-30,-83), S(-19,-68), S( -5,-67), S( -3,-44), S(  2,-31),
                S(  1, -2), S(  4,  0), S(  9,  4), S( 10, 15), S( 10, 23), S( 13, 23), S( 12, 26), S( 17, 18), S( 17, 31),
                S( 14, 34), S( 20, 31), S( 20, 28), S( 40, 14), S( 46, 22), S( 42, 16), S( 24,  3), S( 22,  9), S( 19, 21),
                S( -5, -4),
        }
};

mevalue pawn_shelter[28] = {
        S( 16,-31), S( 40,-38), S( 50,-37), S( 21,-18), S( 13, -8), S( 20,  2), S( 70, 20),
        S(-17,-11), S( 30,-18), S(  5,-18), S(-23,-14), S(-23, 10), S(-12, 16), S(  5, 35),
        S( -7,  2), S( 18,  9), S(  6,  1), S( -9, -2), S(  1,  9), S(  4, 14), S(-17, 15),
        S(-44,  7), S(-22, 12), S(-31,  1), S(-38, 16), S(-32, 17), S(-84, 21), S(-125, 14),
};

mevalue unblocked_storm[28] = {
        S(-25,-30), S(161,318), S(-13,183), S(-65, 54), S(-24,  2), S(-14,-23), S(-16,-19),
        S(-38,-16), S(107,332), S(-72,169), S(-67, 50), S(-40,  9), S(-19, -9), S(-19,-12),
        S( -4,-23), S( 15,164), S(-53,117), S(-24, 16), S(-11, -8), S(  1,-19), S(  6,-23),
        S(-15,-24), S( -6,184), S(-30, 94), S(-20, -9), S( -7,-25), S(  6,-33), S( -3,-30),
};

mevalue blocked_storm[7] = {
        S(  0,  0), S(-39,-61), S(  4,-28), S(  8,-38), S(  1,-57), S(  7,-35), S(  4,  0),
};

void tables_init(void) {
	memset(psqtable, 0, sizeof(psqtable));
	for (int turn = 0; turn < 2; turn++) {
		for (int piece = pawn; piece <= king; piece++) {
			for (int square = 0; square < 64; square++) {
				int f = file_of(square);
				int r = rank_of(square);
				int factor = (piece == pawn) ? 8 : 4;
				if (f >= 4 && piece != pawn)
					f = 7 - f;
				if (turn == white)
					r = 7 - r;
				psqtable[turn][piece][square] = white_psqtable[piece - 1][factor * r + f] +
					piece_value[piece - 1];
			}
		}
	}
}
