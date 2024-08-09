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

#define OPTION_NNUE          1
#define OPTION_PURE_NNUE     0
#define OPTION_TRANSPOSITION 1
#define OPTION_HISTORY       1
#define OPTION_ENDGAME       1
#define OPTION_DAMP          1
#define OPTION_PONDER        0
#define OPTION_ELO           0

int option_nnue          = OPTION_NNUE;
int option_pure_nnue     = OPTION_PURE_NNUE;
int option_transposition = OPTION_TRANSPOSITION;
int option_history       = OPTION_HISTORY;
int option_endgame       = OPTION_ENDGAME;
int option_damp          = OPTION_DAMP;
int option_ponder        = OPTION_PONDER;
int option_elo           = OPTION_ELO;

void print_options(void) {
	printf("option name Clear Hash type button\n");
	printf("option name NNUE type check default %s\n", OPTION_NNUE ? "true" : "false");
	printf("option name PureNNUE type check default %s\n", OPTION_PURE_NNUE ? "true" : "false");
	printf("option name Hash type spin default %u min 0 max %u\n", TT, INT_MAX);
	printf("option name Usehash type check default %s\n", OPTION_TRANSPOSITION ? "true" : "false");
	printf("option name Ponder type check default %s\n", OPTION_PONDER ? "true" : "false");
	printf("option name Elo type spin default %d min 0 max %u\n", OPTION_ELO, INT_MAX);
	printf("option name FileNNUE type string\n");
	printf("option name BuiltinNNUE type button\n");
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
	if (!strcasecmp(argv[2], "nnue"))
		option_nnue = set;
	else if (!strcasecmp(argv[2], "hash")) {
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
	else if (!strcasecmp(argv[2], "elo")) {
		errno = 0;
		char *endptr;
		long elo = strtol(argv[4], &endptr, 10);
		if (!errno && *endptr == '\0' && elo >= 0)
			option_elo = elo;
	}
	else if (!strcasecmp(argv[2], "usehash"))
		option_transposition = set && (tt->size > 0);
	else if (!strcasecmp(argv[2], "ponder"))
		option_ponder = set;
	else if (!strcasecmp(argv[2], "purennue"))
		option_pure_nnue = set;
	else if (!strcasecmp(argv[2], "filennue"))
		file_nnue(argv[4]);
}
