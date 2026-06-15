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

#include "option.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "search.h"
#include "util.h"

struct tune {
	const char *name;
	double start;
	void (*set)(double);
	double (*get)(void);
	int is_int_type;
};

size_t tunes_length;
struct tune *tunes;

char *remove_underscore(const char *var) {
	char *copy = malloc(strlen(var) + 1);

	size_t j, i;
	for (i = 0, j = 0; i < strlen(var); i++) {
		if (var[i] == '_')
			continue;
		copy[j++] = var[i];
	}
	copy[j] = '\0';

	return copy;
}

int tunecmp(const char *var1, const char *var2) {
	char *var1_clean = remove_underscore(var1);
	char *var2_clean = remove_underscore(var2);

	int ret          = strcasecmp(var1_clean, var2_clean);

	free(var1_clean);
	free(var2_clean);

	return ret;
}

void tune_variable(const char *name, double start, void (*set)(double), double (*get)(void), int is_int_type) {
	for (size_t i = 0; i < tunes_length; i++)
		if (!tunecmp(tunes[i].name, name)) {
			fprintf(stderr, "error: already tuning variable with name '%s'\n", name);
			exit(1);
		}

	tunes                   = realloc(tunes, (++tunes_length) * sizeof(*tunes));
	tunes[tunes_length - 1] = (struct tune){ name, start, set, get, is_int_type };
}

void print_tune(void) {
	if (tunes_length)
		printf("option name Restore Tune type button\n");
	for (size_t i = 0; i < tunes_length; i++) {
		struct tune *tune = &tunes[i];
		printf("option name %s type string default %g\n", tune->name, tune->get());
	}
}

void settune(int argc, char **argv) {
	if (!tunes_length)
		return;
	if (!strcasecmp(argv[2], "restore")) {
		for (size_t i = 0; i < tunes_length; i++)
			tunes[i].set(tunes[i].start);
		return;
	}
	if (argc < 5)
		return;

	char *endptr;
	errno        = 0;
	double value = strtod(argv[4], &endptr);

	if (*endptr != '\0' || errno)
		return;

	for (size_t i = 0; i < tunes_length; i++) {
		struct tune *tune = &tunes[i];
		if (tunecmp(tune->name, argv[2]))
			continue;

		tune->set(tune->is_int_type ? round(value) : value);
		search_init();
		break;
	}
}
