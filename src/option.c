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

#include "util.h"
#include "transposition.h"
#include "interface.h"

#define OPTION_NNUE          0
#define OPTION_TRANSPOSITION 1
#define OPTION_HISTORY       1
#define OPTION_ENDGAME       1
#define OPTION_DAMP          1

int option_nnue          = OPTION_NNUE;
int option_transposition = OPTION_TRANSPOSITION;
int option_history       = OPTION_HISTORY;
int option_endgame       = OPTION_ENDGAME;
int option_damp          = OPTION_DAMP;

void print_options(void) {
	printf("option name clear hash type button\n");
	printf("option name nnue type check default %s\n", OPTION_NNUE ? "true" : "false");
	printf("option name hash type spin default %u min 0 max %u\n", TT, INT_MAX);
	printf("option name usehash type check default %s\n", OPTION_TRANSPOSITION ? "true" : "false");
}

void setoption(int argc, char **argv, struct transpositiontable *tt) {
	if (argc < 3)
		return;

	if (!strcasecmp(argv[2], "clear"))
		transposition_clear(tt);

	if (argc < 5)
		return;

	int set = argv[4][0] == 't' || argv[4][0] == 'T';
	if (!strcasecmp(argv[2], "nnue"))
		option_nnue = set;
	else if (!strcasecmp(argv[2], "usehash"))
		option_transposition = set && (tt->size > 0);
}
