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

int main(int argc, char **argv) {
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);

	int i, j;
	char line[10 * BUFSIZ];
	struct position *pos = malloc(sizeof(struct position));
	struct history *history = NULL;
	char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
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

		interrupt = 0;
		if (strncmp(line, "isready", 7) == 0) {
			printf("readyok\n");
		}
		else if (strncmp(line, "position fen", 12) == 0) {
			delete_history(&history);
			transposition_table_clear();
			char *ptr[6];
			for (i = 0, j = 12; line[j]; j++) {
				if (line[j] == ' ') {
					if (i < 6)
						ptr[i] = line + j + 1;
					line[j] = '\0';
					i++;
				}
				if (line[j] == '\n')
					line[j] = '\0';
			}
			pos_from_fen(pos, i, ptr);
		}
		else if (strncmp(line, "position startpos", 17) == 0) {
			delete_history(&history);
			pos_from_fen(pos, SIZE(fen), fen);
			if (strncmp(line + 18, "moves", 5) == 0) {
				char str[6];
				move m;
				for (i = 23; line[i]; i++) {
					if (line[i] == ' ' && i + 6 < 10 * BUFSIZ) {
						memcpy(str, line + i + 1, 6);
						for (j = 0; j < 6; j++)
							if (str[j] == ' ' || str[j] == '\n')
								str[j] = '\0';
						m = string_to_move(pos, str);
						move_next(&pos, &history, m);
					}
				}
			}
		}
		else if (strncmp(line, "ucinewgame", 10) == 0) {
			delete_history(&history);
			transposition_table_clear();
			pos_from_fen(pos, SIZE(fen), fen);
		}
		else if (strncmp(line, "go", 2) == 0) {
			char str[16], *ptr;
			int depth = 255;
			int t = 2;
			if (argc > 1)
				t = str_to_int(argv[1]);
			if ((ptr = strstr(line, "depth"))) {
				memcpy(str, ptr + 6, 16);
				for (i = 0; i < 16; i++)
					if (str[i] == ' ' || str[i] == '\n')
						str[i] = '\0';
				depth = str_to_int(str);
			}
			if ((ptr = strstr(line, pos->turn ? "wtime" : "btime"))) {
				memcpy(str, ptr + 6, 16);
				for (i = 0; i < 16; i++)
					if (str[i] == ' ' || str[i] == '\n')
						str[i] = '\0';
				t = MIN(t, str_to_int(str) / 1000);
			}
			move *m = malloc(sizeof(move));
			evaluate(pos, depth, m, 0, t, history);
			printf("bestmove ");
			print_move(m);
			printf("\n");
			free(m);
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
	delete_history(&history);
	transposition_table_term();
	return 0;
}
