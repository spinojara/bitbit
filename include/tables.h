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

#ifndef TABLES_H
#define TABLES_H

#include "evaluate.h"

extern mevalue piece_value[6];

extern mevalue psqtable[2][7][64];

extern mevalue white_psqtable[6][64];

extern mevalue pawn_shelter[28];

extern mevalue unblocked_storm[28];

extern mevalue blocked_storm[7];

extern mevalue mobility_bonus[4][28];

void tables_init(void);

#endif
