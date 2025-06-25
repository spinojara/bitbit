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

#ifndef THREAD_H
#define THREAD_H

#include "position.h"
#include "transposition.h"
#include "history.h"

int is_allowed(const char *arg);

void search_stop(void);

void search_ponderhit(void);

void search_start(struct position *pos, int depth, struct timeinfo *ti, struct transpositiontable *tt, struct history *history);

void thread_init(void);

void thread_term(void);

#endif
