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

#include "init.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void version(void) {
	printf("bitbit " XSTR(VERSION) "\n");
	printf("Copyright (C) 2022-2024 Isak Ellmer\n");
}

int init(int argc, char **argv) {
	if (argc > 1) {
		if (strcmp(argv[1], "--version") == 0) {
			version();
			return 1;
		}
	}
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);
	printf("bitbit Copyright (C) 2022-2024 Isak Ellmer\n");
	return 0;
}
