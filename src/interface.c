/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022 Isak Ellmer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * * This program is distributed in the hope that it will be useful, * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "interface.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include "bitboard.h"
#include "util.h"
#include "position.h"
#include "perft.h"
#include "evaluate.h"
#include "transposition_table.h"
#include "version.h"
#include "interrupt.h"

#define DONE 0
#define EXIT_LOOP 1
#define ERR_MISS_ARG 2
#define ERR_BAD_ARG 3
#define ERR_MISS_FLAG 4

struct func {
	char *name;
	int (*ptr)(struct arg *arg);
};

struct position *pos = NULL;
struct history *history = NULL;

int flag(struct arg *arg, char f) {
	return arg->flag[(unsigned char)f];
}

void move_next(move m) {
	struct history *t = history;
	history = malloc(sizeof(struct history));
	history->previous = t;
	history->move = malloc(sizeof(move));
	history->pos = malloc(sizeof(struct position));
	copy_position(history->pos, pos);
	*(history->move) = m;
	do_move(pos, history->move);
}

void move_previous() {
	if (!history)
		return;
	undo_move(pos, history->move);
	struct history *t = history;
	history = history->previous;
	free(t->move);
	free(t->pos);
	free(t);
}

int interface_help(struct arg *arg) {
	UNUSED(arg);
	printf(
	"The following commands are available in bitbit:\n"
	"exit\n"
	"help\n"
	"version\n"
	"clear\n"
	"setpos [-r] [fen]\n"
	"domove [-fr] [move]\n"
	"perft [-tv] [depth]\n"
	"eval [-dmtv] [depth]\n"
	"print [-v]\n"
	"tt [-es] [size]\n"
	);
	return DONE;
}

int interface_domove(struct arg *arg) {
	UNUSED(arg);
	if (flag(arg, 'f')) {
		/* should check if king can now be captured */
		swap_turn(pos);
	}
	else if (flag(arg, 'r')) {
		if (history) {
			move_previous();
		}
		else {
			printf("error: no move to undo\n");
		}
	}
	else if (arg->argc < 2) {
		return ERR_MISS_ARG;
	}
	else {
		if (string_to_move(pos, arg->argv[1]))
			move_next(string_to_move(pos, arg->argv[1]));
		else
			return ERR_BAD_ARG;
	}
	return DONE;
}

int interface_perft(struct arg *arg) {
	UNUSED(arg);
	if (arg->argc < 2) {
		return ERR_MISS_ARG;
	}
	else {
		if (str_is_int(arg->argv[1]) && str_to_int(arg->argv[1]) >= 0) {
			clock_t t = clock();
			uint64_t p = perft(pos, str_to_int(arg->argv[1]), flag(arg, 'v'));
			t = clock() - t;
			if (!interrupt)
			printf("nodes: %" PRIu64 "\n", p);
			if (flag(arg, 't') && !interrupt) {
				printf("time: %.2f\n", (double)t / CLOCKS_PER_SEC);
				if (t != 0)
					printf("mpns: %" PRIu64 "\n",
						(p * CLOCKS_PER_SEC / ((uint64_t)t * 1000000)));
			}
		}
		else {
			return ERR_BAD_ARG;
		}
	}
	return DONE;
}

int interface_setpos(struct arg *arg) {
	UNUSED(arg);
	if (flag(arg, 'i')) {
		interactive_setpos(pos);
		while(history)
			move_previous();
	}
	else if (flag(arg, 'r')) {
		random_pos(pos, 32);
		while (history)
			move_previous();
	}
	else if (arg->argc < 2) {
		return ERR_MISS_ARG;
	}
	else {
		if (fen_is_ok(arg->argc - 1, arg->argv + 1)) {
			pos_from_fen(pos, arg->argc - 1, arg->argv + 1);
			while (history)
				move_previous();
		}
		else {
			return ERR_BAD_ARG;
		}
	}
	return DONE;
}

int interface_clear(struct arg *arg) {
	UNUSED(arg);
	printf("\033[1;1H\033[2J");
	return DONE;
}

int interface_exit(struct arg *arg) {
	UNUSED(arg);
	return EXIT_LOOP;
}

int interface_print(struct arg *arg) {
	UNUSED(arg);
	if (flag(arg, 'h')) {
		print_history_pgn(history);
		return DONE;
	}
	print_position(pos);
	if (flag(arg, 'v')) {
		char t[128];
		printf("turn: %c\n", "bw"[pos->turn]);
		printf("castling: %s\n", castle_string(t, pos->castle));
		printf("en passant: %s\n", pos->en_passant ? algebraic(t, pos->en_passant) : "-");
		printf("halfmove: %i\n", pos->halfmove);
		printf("fullmove: %i\n", pos->fullmove);
		printf("zobrist key: ");
		print_binary(pos->zobrist_key);
		printf("\n");
		printf("fen: %s\n", pos_to_fen(t, pos));
	}
	return DONE;
}

int interface_eval(struct arg *arg) {
	UNUSED(arg);
	if (arg->argc < 2) {
		evaluate(pos, 255, NULL, flag(arg, 'v'), -1, history);
	}
	else {
		if (str_is_int(arg->argv[1]) && str_to_int(arg->argv[1]) >= 0) {
			move *m = malloc(sizeof(move));
			clock_t t = clock();
			if (flag(arg, 'd'))
				evaluate(pos, str_to_int(arg->argv[1]), m, flag(arg, 'v'), -1, history);
			else
				evaluate(pos, 255, m, flag(arg, 'v'), str_to_int(arg->argv[1]), history);
			t = clock() - t;
			if (flag(arg, 't'))
				printf("time: %.2f\n", (double)t / CLOCKS_PER_SEC);
			if (flag(arg, 'm') && *m && interrupt < 2) {
				move_next(*m);
			}
			free(m);
		}
		else {
			return ERR_BAD_ARG;
		}
	}
	return DONE;
}

int interface_version(struct arg *arg) {
	UNUSED(arg);

	printf("bitbit " MACRO_VALUE(VERSION) "\n");
	printf("Copyright (C) 2022 Isak Ellmer  \n");
	printf("c compiler: %s\n", compiler);
	printf("environment: %s\n", environment);
	char t[8];
	printf("compilation date: %s\n", date(t));
	printf("transposition table size: ");
	transposition_table_size_print(TT);
	printf("\ntransposition entry size: %" PRIu64 "B\n", sizeof(struct transposition));

	return DONE;
}

int interface_tt(struct arg *arg) {
	UNUSED(arg);

	if (flag(arg, 'e')) {
		transposition_table_clear();
		return DONE;
	}
	if (flag(arg, 's')) {
		if (arg->argc < 2) {
			transposition_table_size_print(log_2(transposition_table_size() *
						sizeof(struct transposition)));
			printf("\n%d%%\n", transposition_table_occupancy());
			return DONE;
		}
		else {
			if (!str_is_int(arg->argv[1]))
				return ERR_BAD_ARG;
			int ret = allocate_transposition_table(str_to_int(arg->argv[1]));
			if (ret == 1)
				return ERR_BAD_ARG;
			if (ret == 2)
				return EXIT_LOOP;
			return DONE;
		}
	}
	return ERR_MISS_FLAG;
}

struct func func_arr[] = {
	{ "help",    interface_help,    },
	{ "domove",  interface_domove,  },
	{ "perft",   interface_perft,   },
	{ "setpos",  interface_setpos,  },
	{ "clear",   interface_clear,   },
	{ "exit",    interface_exit,    },
	{ "print",   interface_print,   },
	{ "eval",    interface_eval,    },
	{ "version", interface_version, },
	{ "tt",      interface_tt,      },
};

void handler(int num);

int parse(int *argc, char ***argv) {
	int ret;
	int i, j;
	char *c;
	struct arg *arg = malloc(sizeof(struct arg));
	for (i = 0; i < 256; i++)
		arg->flag[i] = 0;
	arg->argv = malloc(8 * sizeof(char *));
	for (i = 0; i < 8; i++)
		arg->argv[i] = malloc(128 * sizeof(char));

	arg->argc = 0;
	if (*argc > 1) {
		for (i = 1; i < *argc; i++) {
			c = (*argv)[i];
			/* skip */
			if (c[0] == ',') {
				break;
			}
			/* arg */
			else if (c[0] != '-' || c[1] < 'a' || c[1] > 'z' || i == 1) {
				arg->argc++;
				if (arg->argc < 9) {
					for (j = 0; j < 127 && c[j]; j++)
						arg->argv[arg->argc - 1][j] = c[j];
					arg->argv[arg->argc - 1][j] = '\0';
				}
			}
			/* flag */
			else {
				for (j = 1; c[j]; j++)
					arg->flag[(unsigned char)c[j]] = 1;
			}
		}
		
		*argc -= i;
		*argv += i;
	}
	else {
		char line[BUFSIZ];
		/* prompt */
		printf("\r\33[2K> ");
		if (!fgets(line, sizeof(line), stdin)) {
			ret = 1;
		}
		else {
			c = line;
			while (c[0] == ' ')
				c = c + 1;

			/* assumes always at start of word */
			for (; *c && *c != '\n'; c++) {
				/* flag */
				if (c[0] == '-' && i != 0 && c[-1] == ' ' && c[1] >= 'a' && c[1] <= 'z') {
					for (; c[0] && c[0] != ' ' && c[0] != '\n'; c++)
						arg->flag[(unsigned char)c[0]] = 1;
				}
				/* arg */
				else {
					arg->argc++;
					if (arg->argc < 9) {
						for (j = 0; j < 127 && c[0] && c[0] != ' ' && c[0] != '\n'; j++, c++)
							arg->argv[arg->argc - 1][j] = c[0];
						arg->argv[arg->argc - 1][j] = '\0';
					}
				}
			}
		}
	}

	interrupt = 0;
	if (ret != 1)
		ret = -1;
	if (arg->argc) {
		for (unsigned long k = 0; k < SIZE(func_arr); k++)
			if (strcmp(func_arr[k].name, arg->argv[0]) == 0)
				ret = func_arr[k].ptr(arg);
		if (ret == -1)
			printf("unknown command: %s\n", arg->argv[0]);
	}
	switch(ret) {
	case ERR_MISS_ARG:
		printf("error: missing argument\n");
		break;
	case ERR_BAD_ARG:
		printf("error: bad argument\n");
		break;
	case ERR_MISS_FLAG:
		printf("error: missing flag\n");
		break;
	}
	for (i = 0; i < 8; i++)
		free(arg->argv[i]);
	free(arg->argv);
	free(arg);
	return ret;
}

void interface(int argc, char **argv) {
	printf("\33[2Kbitbit Copyright (C) 2022 Isak Ellmer\n");
	while (parse(&argc, &argv) != 1);
}

void interface_init() {
	char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
	pos = malloc(sizeof(struct position));
	pos_from_fen(pos, SIZE(fen), fen);
}

void interface_term() {
	free(pos);
	while (history)
		move_previous();
}
