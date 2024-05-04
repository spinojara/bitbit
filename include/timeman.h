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
#include <sys/time.h>
#include <stddef.h>
#include <stdatomic.h>

#include "search.h"
#include "move.h"
#include "position.h"

extern volatile atomic_int uciponder;

typedef int64_t timepoint_t;

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

static inline timepoint_t time_now(void) {
	struct timeval t;
	gettimeofday(&t, NULL);
	return (timepoint_t)t.tv_sec * 1000000 + t.tv_usec;
}

static inline timepoint_t time_since(const struct timeinfo *ti) {
	return time_now() - ti->start;
}

static inline int check_time(const struct timeinfo *ti) {
	if (atomic_load_explicit(&uciponder, memory_order_relaxed) || !ti)
		return 0;
	if (ti->stop_on_time && time_since(ti) >= ti->maximal)
		return 1;
	return 0;
}

#endif
