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

#include "timeman.h"

#include <stdint.h>
#include <stdio.h>

#include "util.h"

int time_man(int etime, int32_t saved_evaluation[256], uint8_t depth) {
	int var = 0;
	if (depth > 2)
		var = variance(saved_evaluation + depth - 3, 3);
	int a = (double)etime / 20 * (1 + (double)var / 4096);
	return a;
}

void time_init(struct position *pos, int etime, struct searchinfo *si) {
	if (!etime)
		return;

	int moves_left = MAX(25, 50 - pos->fullmove);

	si->time_optimal = 1000 * etime / moves_left + si->time_start;
	si->time_stop = 1000 * etime / 10 + si->time_start;
}

int stop_searching(struct searchinfo *si) {
	return time_now() >= si->time_optimal;
}
