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

#include <limits.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "util.h"
#include "transposition.h"
#include "interface.h"
#include "nnue.h"
#if TUNE
#include "tune.h"
#endif

#define OPTION_TRANSPOSITION 1
#define OPTION_HISTORY       1
#define OPTION_ENDGAME       1
#define OPTION_DAMP          1
#define OPTION_PONDER        0
#define OPTION_DETERMINISTIC 1

int option_transposition  = OPTION_TRANSPOSITION;
int option_history        = OPTION_HISTORY;
int option_endgame        = OPTION_ENDGAME;
int option_damp           = OPTION_DAMP;
int option_ponder         = OPTION_PONDER;
int option_deterministic  = OPTION_DETERMINISTIC;
char option_debugtt[4096] = "";

void print_options(void) {
	printf("option name Clear Hash type button\n");
	printf("option name Hash type spin default %u min 0 max %u\n", TT, INT_MAX);
	printf("option name UseHash type check default %s\n", OPTION_TRANSPOSITION ? "true" : "false");
	printf("option name Ponder type check default %s\n", OPTION_PONDER ? "true" : "false");
	printf("option name FileNNUE type string\n");
	printf("option name BuiltinNNUE type button\n");
	printf("option name Deterministic type check default %s\n", OPTION_DETERMINISTIC ? "true" : "false");
	printf("option name DebugHash type string\n");
#if TUNE
	print_tune();
#endif
}

void setoption(int argc, char **argv, struct transpositiontable *tt) {
#if TUNE
	settune(argc, argv);
#endif
	if (argc < 3)
		return;

	if (!strcasecmp(argv[2], "clear"))
		transposition_clear(tt);
	else if (!strcasecmp(argv[2], "builtinnnue"))
		builtin_nnue();

	if (argc < 5)
		return;

	int set = argv[4][0] == 't' || argv[4][0] == 'T';
	if (!strcasecmp(argv[2], "hash")) {
		errno = 0;
		char *endptr;
		long MiB = strtol(argv[4], &endptr, 10);
		if (!errno && *endptr == '\0') {
			if (MiB > 0) {
				transposition_free(tt);
				if (transposition_alloc(tt, MiB * 1024 * 1024)) {
					fprintf(stderr, "error: failed to allocate transposition table\n");
					tt->size = 0;
					option_transposition = 0;
				}
			}
			else {
				tt->size = 0;
				option_transposition = 0;
			}
		}
	}
	else if (!strcasecmp(argv[2], "usehash"))
		option_transposition = set && (tt->size > 0);
	else if (!strcasecmp(argv[2], "ponder"))
		option_ponder = set;
	else if (!strcasecmp(argv[2], "filennue"))
		file_nnue(argv[4]);
	else if (!strcasecmp(argv[2], "deterministic"))
		option_deterministic = set;
	else if (!strcasecmp(argv[2], "debughash")) {
		strncpy(option_debugtt, argv[4], sizeof(option_debugtt));
		option_debugtt[sizeof(option_debugtt) - 1] = '\0';
	}
}
