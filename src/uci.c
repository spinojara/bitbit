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
#include "util.h"
#include "magic_bitboard.h"
#include "attack_gen.h"
#include "bitboard.h"
#include "evaluate.h"
#include "transposition_table.h"
#include "interface.h"

int main() {
	int i, j;
	char line[BUFSIZ];
	struct position *pos = malloc(sizeof(struct position));
	printf("id name bitbit\n");
	printf("id author Isak Ellmer\n");
	printf("uciok\n");

	util_init();
	magic_bitboard_init();
	attack_gen_init();
	bitboard_init();
	evaluate_init();
	transposition_table_init();

	int quit = 0;
	while (!quit) {
		if (!fgets(line, sizeof(line), stdin))
			continue;

		if (strncmp(line, "isready", 7) == 0) {
			printf("readyok\n");
		}
		else if (strncmp(line, "position fen", 12) == 0) {
			char *fen[6];
			for (i = 0, j = 12; line[j]; j++) {
				if (line[j] == ' ') {
					if (i < 6)
						fen[i] = line + j + 1;
					line[j] = '\0';
					i++;
				}
				if (line[j] == '\n')
					line[j] = '\0';
			}
			pos_from_fen(pos, i, fen);
			print_position(pos, 0);
		}
		else if (strncmp(line, "position startpos", 17) == 0) {
			char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
			pos_from_fen(pos, SIZE(fen), fen);
			if (strncmp(line + 18, "moves", 5) == 0) {
				char str[6];
				move *m = malloc(sizeof(move));
				for (i = 23; line[i]; i++) {
					if (line[i] == ' ' && i + 6 < BUFSIZ) {
						memcpy(str, line + i + 1, 6);
						for (j = 0; j < 6; j++)
							if (str[j] == ' ' || str[j] == '\n')
								str[j] = '\0';
						*m = string_to_move(pos, str);
						do_move(pos, m);
					}
				}
				free(m);
			}
		}
		else if (strncmp(line, "ucinewgame", 10) == 0) {
			char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
			pos_from_fen(pos, SIZE(fen), fen);
		}
		else if (strncmp(line, "go", 2) == 0) {
			char *ptr;
			int depth = 255;
			if ((ptr = strstr(line, "depth")))
				depth = atoi(ptr + 6);
			move *m = malloc(sizeof(move));
			evaluate(pos, depth, m, 0, 3, NULL);
			printf("bestmove ");
			print_move(m);
			printf("\n");
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

	transposition_table_term();
}
