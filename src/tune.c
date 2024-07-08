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

#include "option.h"

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <strings.h>
#include <string.h>

#include "transposition.h"
#include "util.h"

#define OPTION_NNUE          1
#define OPTION_TRANSPOSITION 1
#define OPTION_HISTORY       1
#define OPTION_ENDGAME       1
#define OPTION_DAMP          1
#define OPTION_PONDER        0
#define OPTION_ELO           0

int option_nnue          = OPTION_NNUE;
int option_transposition = OPTION_TRANSPOSITION;
int option_history       = OPTION_HISTORY;
int option_endgame       = OPTION_ENDGAME;
int option_damp          = OPTION_DAMP;
int option_ponder        = OPTION_PONDER;
int option_elo           = OPTION_ELO;

void print_options(void) {}

enum {
	TYPE_INT,
	TYPE_DOUBLE,
};

struct tune {
	char *name;
	int type;
	void *p;
};

#define TUNE(x, y, z) { .name = x, .type = y, .p = z }

int test_option;

struct tune tunes[] = {
	TUNE("test_option", TYPE_INT, &test_option),
};

int rdi(double f) {
	return (int)(f < 0.0 ? f - 0.5 : f + 0.5);
}

/* setoption tune <name> value <value> */
void setoption(int argc, char **argv, struct transpositiontable *tt) {
	UNUSED(tt);
	if (argc < 5)
		return;

	char *endptr;
	errno = 0;
	double value = strtod(argv[4], &endptr);

	if (*endptr != '\0' || errno)
		return;
	
	for (size_t i = 0; i < SIZE(tunes); i++) {
		struct tune *tune = &tune[i];
		if (strcasecmp(tune->name, argv[2]))
			continue;

		switch (tune->type) {
		case TYPE_INT:
			*(int *)tune->p = rdi(value);
			break;
		case TYPE_DOUBLE:
			*(double *)tune->p = value;
			break;
		}
	}
}
