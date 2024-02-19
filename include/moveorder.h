/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2024 Isak Ellmer
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

#ifndef MOVE_ORDER_H
#define MOVE_ORDER_H

#include <stdint.h>

#include "move.h"
#include "position.h"
#include "search.h"

extern int mvv_lva_lookup[7 * 7];

static inline int mvv_lva(int attacker, int victim) {
	return mvv_lva_lookup[attacker + 7 * victim];
}

int see_geq(struct position *pos, const move_t *move, int32_t value);

uint64_t generate_defenders(struct position *pos, const move_t *move);

void moveorder_init(void);

#endif
