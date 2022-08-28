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

#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "position.h"
#include "move.h"

move *generate_all(struct position *pos, move *move_list);

move *generate_white(struct position *pos, move *move_list);

move *generate_black(struct position *pos, move *move_list);

int move_count(move *m);

int mate(struct position *pos);

int mate_white(struct position *pos);

int mate_black(struct position *pos);

move *generate_quiescence(struct position *pos, move *move_list);

move *generate_quiescence_white(struct position *pos, move *move_list);

move *generate_quiescence_black(struct position *pos, move *move_list);

int mobility_white(struct position *pos);

int mobility_black(struct position *pos);

#endif
