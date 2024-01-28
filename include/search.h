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

#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "move.h"
#include "interface.h"
#include "transposition.h"

#define DEPTH_MAX (0x100)

typedef int64_t timepoint_t;

struct searchstack {
	move_t move;
	move_t excluded_move;
};

struct searchinfo {
	uint64_t nodes;

	timepoint_t time_start;
	timepoint_t time_optimal;
	timepoint_t time_stop;

	move_t pv[DEPTH_MAX][DEPTH_MAX];
	move_t killers[DEPTH_MAX][2];
	int64_t history_moves[13][64];

	struct transpositiontable *tt;
	struct history *history;

	int root_depth;

	int interrupt;
};

int32_t search(struct position *pos, int depth, int verbose, int etime, int movetime, move_t *move, struct transpositiontable *tt, struct history *history, int iterative);

void search_init(void);

#endif
