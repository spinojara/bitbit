/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2025 Isak Ellmer
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

#define _POSIX_C_SOURCE 199309L
#include "timeman.h"

#include <time.h>
#include <stdint.h>
#include <stdio.h>

#include "util.h"
#include "option.h"

#ifdef CLOCK_MONOTONIC_RAW
#define CLOCK CLOCK_MONOTONIC_RAW
#else
#define CLOCK CLOCK_MONOTONIC
#endif

#ifdef TUNE
#define CONST
#else
#define CONST const
#endif

CONST double maximal = 0.8;
CONST double instability1 = 0.5;
CONST double instability2 = 1.1;

void time_init(struct position *pos, struct timeinfo *ti) {
	if (!ti)
		return;

	ti->start = time_now();

	if (!ti->stop_on_time)
		ti->stop_on_time = ti->movetime || ti->etime[0] || ti->etime[1] || ti->einc[0] || ti->einc[1];
	if (!ti->stop_on_time)
		return;

	if (ti->movetime) {
		ti->optimal = ti->maximal = ti->movetime;
		return;
	}

	ti->us = pos->turn;

	ti->best_move = 0;
	ti->best_move_changes = -1.0;

	if (ti->movestogo <= 0)
		ti->movestogo = max(30, 60 - pos->fullmove / 2);

	timepoint_t time_left = ti->etime[ti->us] + ti->movestogo * ti->einc[ti->us];

	ti->optimal = time_left / ti->movestogo;
	ti->maximal = maximal * ti->etime[ti->us];

	if (option_ponder) {
		int them = other_color(ti->us);
		time_left = ti->etime[them] + ti->movestogo * ti->einc[them];
		ti->optimal += min(ti->optimal / 4, time_left / (2 * ti->movestogo));
	}
}

int stop_searching(struct timeinfo *ti, move_t best_move) {
	if (!ti || !ti->stop_on_time || atomic_load_explicit(&uciponder, memory_order_relaxed))
		return 0;

	if (!move_compare(ti->best_move, best_move))
		ti->best_move_changes++;
	else
		ti->best_move_changes /= 2;

	ti->best_move = best_move;

	double instability = instability1 + instability2 * ti->best_move_changes;

	timepoint_t t;
	return (t = time_since(ti)) >= ti->maximal ||
		t >= ti->optimal * instability;
}

timepoint_t time_now(void) {
	struct timespec tp;
	clock_gettime(CLOCK, &tp);
	return (timepoint_t)tp.tv_sec * TPPERSEC + (timepoint_t)tp.tv_nsec;
}
