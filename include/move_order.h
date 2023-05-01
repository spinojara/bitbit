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

#ifndef MOVE_ORDER_H
#define MOVE_ORDER_H

#include <stdint.h>

#include "move.h"
#include "position.h"

void next_move(move *move_list, uint64_t *evaluation_list, move **ptr);

move *order_moves(struct position *pos, move *move_list, uint64_t *evaluation_list, uint8_t depth, uint8_t ply, void *e, int pv_flag, move pv_moves[256][256], move killer_moves[][2], uint64_t history_moves[13][64]);

int see_geq(struct position *pos, const move *m, int16_t value);

void move_order_init(void);

#endif
