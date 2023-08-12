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

#include "position.h"
#include "move.h"
#include "interface.h"
#include "transposition.h"

#define DEPTH_MAX (0x100)

typedef int64_t time_point;

enum {
	FLAG_NONE = 64,
	FLAG_NULL_MOVE = 65,
};

struct searchinfo {
	uint64_t nodes;

	time_point time_start;
	time_point time_optimal;
	time_point time_stop;

	move pv[DEPTH_MAX][DEPTH_MAX];
	move killers[DEPTH_MAX][2];
	int64_t history_moves[13][64];

	struct transpositiontable *tt;
	struct history *history;

	int root_depth;

	int interrupt;
};

int32_t search(struct position *pos, int depth, int verbose, int etime, int movetime, move *m, struct transpositiontable *tt, struct history *history, int iterative);

int32_t quiescence(struct position *pos, int ply, int32_t alpha, int32_t beta, struct searchinfo *si);

void search_init(void);

#endif
