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
#include "search.h"

extern int mvv_lva_lookup[13 * 13];

static inline int mvv_lva(int attacker, int victim) {
	return mvv_lva_lookup[attacker + 13 * victim];
}

int see_geq(struct position *pos, const move *m, int32_t value);

void moveorder_init(void);

#endif
