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

#include "timeman.h"

#include <stdint.h>
#include <stdio.h>

#include "util.h"

void time_init(struct position *pos, struct timeinfo *ti) {
	if (!ti)
		return;

	ti->start = time_now();

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
	ti->tries = 0;

	if (ti->movestogo <= 0)
		ti->movestogo = max(30, 60 - pos->fullmove / 2);

	timepoint_t time_left = ti->etime[ti->us] + ti->movestogo * ti->einc[ti->us];

	ti->optimal = time_left / ti->movestogo;
	ti->maximal = 0.8 * ti->etime[ti->us];
}

int stop_searching(struct timeinfo *ti, move_t best_move) {
	if (!ti)
		return 0;

	if (!move_compare(ti->best_move, best_move))
		ti->best_move_changes++;
	else
		ti->best_move_changes /= 2;

	ti->best_move = best_move;
	ti->tries++;

	double margin = 1.2;
	double instability = 0.8 + 2.0 * ti->best_move_changes;

	return ti->stop_on_time && (time_since(ti) >= ti->maximal ||
		time_since(ti) * margin >= ti->optimal * instability);
}
