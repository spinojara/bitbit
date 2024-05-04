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

#include <stdatomic.h>

#include "position.h"
#include "move.h"
#include "interface.h"
#include "transposition.h"
#include "evaluate.h"

extern volatile atomic_int ucistop;
extern volatile atomic_int ucigo;
extern volatile atomic_int uciponder;

typedef int64_t timepoint_t;

struct searchstack {
	move_t move;
	move_t excluded_move;

	int32_t static_eval;
};

struct searchinfo {
	uint64_t nodes;

	move_t pv[PLY_MAX][PLY_MAX];
	move_t killers[PLY_MAX][2];
	move_t counter_move[13][64];
	int64_t quiet_history[13][64][64];
	int64_t capture_history[13][7][64];

	struct transpositiontable *tt;
	struct history *history;

	int root_depth, sel_depth;

	int interrupt;

	struct timeinfo *ti;
};

int32_t search(struct position *pos, int depth, int verbose, struct timeinfo *ti, move_t *move, struct transpositiontable *tt, struct history *history, int iterative);

void search_init(void);

#endif
