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

#include "init.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "interface.h"

#define PRINT_DELAY_MS 1

struct counter {
	uint64_t done;
	uint64_t total;
	clock_t time;
};

struct counter *counter = NULL;

int init(int argc, char **argv) {
	if (argc > 1) {
		if (strcmp(argv[1], "--version") == 0) {
			interface_version(NULL);
			return 1;
		}
	}
	counter = malloc(sizeof(struct counter));
	counter->total = 366701;
	counter->done = 0;
	counter->time = clock();
	init_status("init");
	return 0;
}

void term() {
	free(counter);
}

void init_print(char *str) {
	printf("\033[2K[%" PRIu64 "/%" PRIu64 "] %s\r", counter->done, counter->total, str);
	fflush(stdout);
}

void init_status(char *str) {
	if (!counter)
		return;
	counter->done++;
	clock_t t = clock();
	if (1000 * (t - counter->time) > CLOCKS_PER_SEC * PRINT_DELAY_MS) {
		init_print(str);
		counter->time = t;
	}
}
