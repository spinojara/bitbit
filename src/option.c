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

#include "option.h"
#include "util.h"
#include "string.h"
#include "strings.h"
#include "transposition.h"
#include "interface.h"

int option_nnue          = 1;
int option_transposition = 1;
int option_history       = 1;
int option_endgame       = 1;
int option_damp          = 1;

void print_options(void) {
	printf("option name clear hash type button\n");
	printf("option name nnue type check default true\n");
	printf("option name transposition type check default true\n");
}

void setoption(int argc, char **argv, void *p) {
	if (argc < 3)
		return;
	
	if (!strcasecmp(argv[2], "clear")) {
		transposition_clear(p);
	}
	else if (!strcasecmp(argv[2], "nnue")) {
		if (argc < 5)
			return;
		if (!strcasecmp(argv[4], "true"))
			option_nnue = 1;
		else if (!strcasecmp(argv[4], "false"))
			option_nnue = 0;
	}
	else if (!strcasecmp(argv[2], "transposition")) {
		if (argc < 5)
			return;
		if (!strcasecmp(argv[4], "true"))
			option_transposition = 1;
		else if (!strcasecmp(argv[4], "false"))
			option_transposition = 0;
	}
}
