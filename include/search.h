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

#ifndef SEARCH_H
#define SEARCH_H

#include <time.h>

#include "position.h"
#include "move.h"
#include "interface.h"

enum {
	NODE_ROOT = 0,
	NODE_PV = 1,
	NODE_CUT = 2,
	NODE_ALL = 3,
	NODE_OTHER = 4,
};

enum {
	FLAG_NONE = 64,
	FLAG_NULL_MOVE = 65,
};

struct searchinfo {
	uint64_t nodes;
	clock_t clock_stop;
	move pv_moves[256][256];
	move killer_moves[256][2];
	uint64_t history_moves[13][64];
	struct history *history;
	uint8_t root_depth;
};

int16_t search(struct position *pos, uint8_t depth, int verbose, int etime, int movetime, move *m, struct history *history, int iterative);

void search_init(void);

#endif
