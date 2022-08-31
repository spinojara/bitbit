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

#ifndef INTERFACE_H
#define INTERFACE_H

#include "move.h"

#define DONE 0
#define EXIT_LOOP 1
#define ERR_MISS_ARG 2
#define ERR_BAD_ARG 3
#define ERR_MISS_FLAG 4
#define ERR_BAD_FLAG 5

struct arg {
	int flag[256];
	int argc;
	char **argv;
};

struct history {
	move *move;
	struct position *pos;
	struct history *previous;
};

void interface(int argc, char **argv);

void interface_init();

void interface_term();

int interface_version(struct arg *arg);

void delete_history(struct history **h);

void move_next(struct position **p, struct history **h, move m);

#endif
