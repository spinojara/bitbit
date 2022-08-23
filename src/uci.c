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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "position.h"
#include "interrupt.h"

int main() {
	char line[BUFSIZ];
	struct position *pos = malloc(sizeof(struct position));
	printf("id name bitbit\n");
	printf("id author Isak Ellmer\n");
	printf("uciok\n");

	int quit = 0;
	while (!quit) {
		if (!fgets(line, sizeof(line), stdin))
			continue;

		if (strncmp(line, "isready", 7) == 0) {
			printf("readyok\n");
		}
		else if (strncmp(line, "position fen", 12) == 0) {

		}
		else if (strncmp(line, "position startpos", 15) == 0) {

		}
		else if (strncmp(line, "ucinewgame", 10) == 0) {
			
		}
		else if (strncmp(line, "go", 2) == 0) {

		}
		else if (strncmp(line, "quit", 4) == 0) {
			quit = 1;
		}
		else if (strncmp(line, "uci", 3) == 0) {
			printf("id name bitbit\n");
			printf("id author Isak Ellmer\n");
			printf("uciok\n");
		}
	}
	free(pos);
}
