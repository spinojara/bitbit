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

#ifndef TIME_MAN_H
#define TIME_MAN_H

#include <stdint.h>
#include <sys/time.h>
#include <stddef.h>

#include "search.h"
#include "position.h"

typedef int64_t time_point;

int time_man(int etime, int32_t saved_evaluation[256], int depth);

void time_init(struct position *pos, int etime, struct searchinfo *si);

int stop_searching(struct searchinfo *si);

static inline time_point time_now(void) {
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000000 + t.tv_usec;
}

static inline void check_time(struct searchinfo *si) {
	if (si->time_stop && time_now() >= si->time_stop)
		si->interrupt = 1;
}

#endif
