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

#include "util.h"

enum {
	TYPE_INT,
	TYPE_DOUBLE,
};

struct tune {
	char *name;
	int type;
	void *p;
};

#undef TUNE
#define TUNE(x, y, z) { .name = x, .type = y, .p = z }

extern int razor1;
extern int razor2;
extern int futility;
extern double maximal;
extern double instability1;
extern double instability2;

struct tune tunes[] = {
	TUNE("razor1", TYPE_INT, &razor1),
	TUNE("razor2", TYPE_INT, &razor2),
	TUNE("futility", TYPE_INT, &futility),
	TUNE("maximal", TYPE_DOUBLE, &maximal),
	TUNE("instability1", TYPE_DOUBLE, &instability1),
	TUNE("instability2", TYPE_DOUBLE, &instability2),
};

int rdi(double f) {
	return (int)(f < 0.0 ? f - 0.5 : f + 0.5);
}

void print_tune(void) {
	for (size_t i = 0; i < SIZE(tunes); i++)
		printf("option name %s type string default %lf\n", tunes[i].name, tunes[i].type == TYPE_INT ? *(int *)tunes[i].p : *(double *)tunes[i].p);
}

void settune(int argc, char **argv) {
	if (argc < 5)
		return;

	char *endptr;
	errno = 0;
	double value = strtod(argv[4], &endptr);

	if (*endptr != '\0' || errno)
		return;

	for (size_t i = 0; i < SIZE(tunes); i++) {
		struct tune *tune = &tunes[i];
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
