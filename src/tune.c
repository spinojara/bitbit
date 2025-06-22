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
#include "search.h"

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
extern double red;
extern int asp;
extern double maximal;
extern double instability1;
extern double instability2;

extern int from_attack;
extern int into_attack;
extern int not_defended;
extern int check_threat;
extern double mvv_lva_factor;
extern double continuation_history_factor;
extern int goodquiet_threshold;

extern int quad_bonus;
extern int quad_malus;
extern double history_regularization;

extern int damp_factor;

extern int aspiration_depth;

struct tune tunes[] = {
	TUNE("razor1", TYPE_INT, &razor1),
	TUNE("razor2", TYPE_INT, &razor2),
	TUNE("futility", TYPE_INT, &futility),
	TUNE("maximal", TYPE_DOUBLE, &maximal),
	TUNE("instability1", TYPE_DOUBLE, &instability1),
	TUNE("instability2", TYPE_DOUBLE, &instability2),
	TUNE("reduction", TYPE_DOUBLE, &red),
	TUNE("aspiration", TYPE_INT, &asp),

	TUNE("fromattack", TYPE_INT, &from_attack),
	TUNE("intoattack", TYPE_INT, &into_attack),
	TUNE("notdefended", TYPE_INT, &not_defended),
	TUNE("checkthreat", TYPE_INT, &check_threat),
	TUNE("mvvlvafactor", TYPE_DOUBLE, &mvv_lva_factor),
	TUNE("continuationhistoryfactor", TYPE_DOUBLE, &continuation_history_factor),
	TUNE("goodquietthreshold", TYPE_INT, &goodquiet_threshold),

	TUNE("quadbonus", TYPE_INT, &quad_bonus),
	TUNE("quadmalus", TYPE_INT, &quad_malus),
	TUNE("historyregularization", TYPE_DOUBLE, &history_regularization),

	TUNE("dampfactor", TYPE_INT, &damp_factor),

	TUNE("aspirationdepth", TYPE_INT, &aspiration_depth),
};

int rdi(double f) {
	return (int)(f < 0.0 ? f - 0.5 : f + 0.5);
}

void print_tune(void) {
	for (size_t i = 0; i < SIZE(tunes); i++) {
		if (tunes[i].type == TYPE_INT)
			printf("option name %s type string default %d\n", tunes[i].name, *(int *)tunes[i].p);
		else
			printf("option name %s type string default %lf\n", tunes[i].name, *(double *)tunes[i].p);
	}
}

void settune(int argc, char **argv) {
	if (argc < 5)
		return;

	char *endptr;
	errno = 0;
	double value = strtod(argv[4], &endptr);

	if (*endptr != '\0' || errno)
		return;

	int hit = 0;
	for (size_t i = 0; i < SIZE(tunes); i++) {
		struct tune *tune = &tunes[i];
		if (strcasecmp(tune->name, argv[2]))
			continue;

		hit = 1;
		switch (tune->type) {
		case TYPE_INT:
			*(int *)tune->p = rdi(value);
			break;
		case TYPE_DOUBLE:
			*(double *)tune->p = value;
			break;
		}

		if (!strcasecmp(tune->name, "reduction"))
			search_init();
	}

	if (!hit) {
		printf("error: no option '%s'\n", argv[2]);
		exit(1);
	}
}
