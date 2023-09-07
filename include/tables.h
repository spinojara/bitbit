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

extern score_t piece_value[6];

extern score_t psqtable[2][7][64];

extern score_t white_psqtable[6][64];

extern score_t pawn_shelter[28];

extern score_t unblocked_storm[28];

extern score_t unblockable_storm[28];

extern score_t blocked_storm[28];

extern score_t mobility[4][28];

void tables_init(void);

#endif
