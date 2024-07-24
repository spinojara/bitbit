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

#ifndef TIMEMAN_H
#define TIMEMAN_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>
#include <stdatomic.h>

#include "search.h"
#include "move.h"
#include "position.h"

extern volatile atomic_int uciponder;

/* timepoint_t is given in nanoseconds. */
typedef int64_t timepoint_t;

#define TPPERSEC 1000000000l
#define TPPERMS     1000000l

struct timeinfo {
	int stop_on_time;

	int movestogo;

	timepoint_t etime[2];
	timepoint_t einc[2];

	timepoint_t movetime;

	timepoint_t start;
	timepoint_t optimal;
	timepoint_t maximal;

	int us;

	move_t best_move;
	double best_move_changes;

	double multiplier;
};

void time_init(struct position *pos, struct timeinfo *ti);

int stop_searching(struct timeinfo *si, move_t best_move);

timepoint_t time_now(void);

static inline timepoint_t time_since(const struct timeinfo *ti) {
	return time_now() - ti->start;
}

/* We should check at least a couple of times per millisecond.
 * We are usually above 1 million nps. Checking every 256 nodes
 * means we check every 256 / 1000000 = 0.000512 s = 0.256 ms.
 * 0x100 = 256.
 */
static inline int check_time(const struct searchinfo *si) {
	return !(si->nodes & (0x100 - 1)) && si->ti &&
		time_since(si->ti) >= si->ti->maximal && si->ti->stop_on_time &&
		!atomic_load_explicit(&uciponder, memory_order_relaxed);
}

#endif
