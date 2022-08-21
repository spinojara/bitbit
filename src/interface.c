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
struct move_linked *move_last = NULL;

void move_next(move m) {
	struct move_linked *t = move_last;
	move_last = malloc(sizeof(struct move_linked));
	move_last->previous = t;
	move_last->move = malloc(sizeof(move));
	move_last->pos = malloc(sizeof(struct position));
	copy_position(move_last->pos, pos);
	*(move_last->move) = m;
	do_move(pos, move_last->move);
}

void move_previous() {
	undo_move(pos, move_last->move);
	if (!move_last)
		return;
	struct move_linked *t = move_last;
	move_last = move_last->previous;
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
	if (arg->f) {
		/* should check if king can now be captured */
		swap_turn(pos);
	}
	else if (arg->r) {
		if (move_last) {
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
		if (string_is_int(arg->argv[1]) && atoi(arg->argv[1]) >= 0) {
			clock_t t = clock();
			uint64_t p = perft(pos, atoi(arg->argv[1]), arg->v);
			t = clock() - t;
			if (!interrupt)
			printf("nodes: %" PRIu64 "\n", p);
			if (arg->t && !interrupt) {
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
	if (arg->i) {
		interactive_setpos(pos);
		while(move_last)
			move_previous();
	}
	else if (arg->r) {
		random_pos(pos, 32);
		while (move_last)
			move_previous();
	}
	else if (arg->argc < 2) {
		return ERR_MISS_ARG;
	}
	else {
		if (fen_is_ok(arg->argc - 1, arg->argv + 1)) {
			pos_from_fen(pos, arg->argc - 1, arg->argv + 1);
			while (move_last)
				move_previous();
		}
		else {
			return ERR_BAD_ARG;
		}
	}
	return ERR_MISS_ARG;
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
	print_position(pos);
	if (arg->v) {
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
		evaluate(pos, 255, NULL, arg->v, -1, move_last);
	}
	else {
		if (string_is_int(arg->argv[1]) && atoi(arg->argv[1]) >= 0) {
			move *m = malloc(sizeof(move));
			clock_t t = clock();
			if (arg->d)
				evaluate(pos, atoi(arg->argv[1]), m, arg->v, -1, move_last);
			else
				evaluate(pos, 255, m, arg->v, atoi(arg->argv[1]), move_last);
			t = clock() - t;
			if (arg->t)
				printf("time: %.2f\n", (double)t / CLOCKS_PER_SEC);
			if (arg->m && *m && interrupt < 2) {
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

	if (arg->e) {
		transposition_table_clear();
		return DONE;
	}
	if (arg->s) {
		if (arg->argc < 2) {
			transposition_table_size_print(log_2(transposition_table_size() *
						sizeof(struct transposition)));
			printf("\n%d%%\n", transposition_table_occupancy());
			return DONE;
		}
		else {
			if (!string_is_int(arg->argv[1]))
				return ERR_BAD_ARG;
			int ret = allocate_transposition_table(atoi(arg->argv[1]));
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
	/* reset interrupt */
	struct arg *arg = calloc(1, sizeof(struct arg));
	if (!arg)
		return DONE;

	int i, j, l;
	int ret = -1;
	char *c;
	if (*argc > 1) {
		/* get arg->argc */
		arg->argc = 0;
		for (l = 1; l < *argc; l++) {
			if ((*argv)[l][0] != '-' || find_char("0123456789", (*argv)[l][1]) != -1)
				arg->argc++;
			if ((*argv)[l][0] == ',') {
				arg->argc--;
				break;
			}
		}

		if (!arg->argc) {
			*argc = 1;
			goto end_early;
		}

		arg->argv = malloc(arg->argc * sizeof(char *));

		/* get arg->argv */
		for (i = 0, j = 1; i + j < *argc; ) {
			if ((*argv)[i + j][0] == '-' && find_char("0123456789", (*argv)[i + j][1]) != 1) {
				for (c = (*argv)[i + j] + 1; *c != '\0'; c++) {
					switch (*c) {
					case 'v':
						arg->v = 1;
						break;
					case 't':
						arg->t = 1;
						break;
					case 'r':
						arg->r = 1;
						break;
					case 'f':
						arg->f = 1;
						break;
					case 'm':
						arg->m = 1;
						break;
					case 'h':
						arg->h = 1;
						break;
					case 's':
						arg->s = 1;
						break;
					case 'e':
						arg->e = 1;
						break;
					case 'i':
						arg->i = 1;
						break;
					case 'd':
						arg->d = 1;
						break;
					}
				}
				j++;
				continue;
			}
			if (i >= arg->argc)
				break;
			/* strlen + 1 for null character */
			arg->argv[i] = malloc((strlen((*argv)[i + j]) + 1) * sizeof(char));
			strcpy(arg->argv[i], (*argv)[i + j]);
			i++;
		}

		*argc -= l;
		*argv += l;
	}
	else {
		char line[BUFSIZ];
		/* prompt */
		printf("\r\33[2K> ");
		if (!fgets(line, sizeof(line), stdin)) {
			ret = 1;
			goto end_early;
		}

		/* get arg->argc and flags */
		arg->argc = 0;
		for (i = 0, j = 0, c = line; *c != '\0'; c++) {
			switch (*c) {
			case '\n':
				break;
			case ' ':
				i = 0;
				j = 0;
				break;
			case '-':
				if (!i) {
					/* will not cause segmentation fault */
					switch (c[1]) {
					case '\0':
					case '\n':
					case ' ':
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						break;
					default:
						j = 1;
					}
				}
				if (j)
					break;
				/* fallthrough */
			default:
				if (!i && !j) {
					i = 1;
					arg->argc++;
				}
				else if (j) {
					switch (*c) {
						case 'v':
							arg->v = 1;
							break;
						case 't':
							arg->t = 1;
							break;
						case 'r':
							arg->r = 1;
							break;
						case 'f':
							arg->f = 1;
							break;
						case 'm':
							arg->m = 1;
							break;
						case 'h':
							arg->h = 1;
							break;
						case 's':
							arg->s = 1;
							break;
						case 'e':
							arg->e = 1;
							break;
						case 'i':
							arg->i = 1;
							break;
						case 'd':
							arg->d = 1;
							break;
					}
				}
			}
		}

		if (!arg->argc)
			goto end_early;

		arg->argv = malloc(arg->argc * sizeof(char *));

		/* get arg->argv */
		for (i = 1, j = 0, c = line; *c != '\0'; c++) {
			switch (*c) {
			case '\n':
				break;
			case ' ':
				i = 1;
				break;
			case '-':
				if (i) {
					/* will not cause segmentation fault */
					switch (c[1]) {
					case '\0':
					case '\n':
					case ' ':
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						break;
					default:
						i = 0;
					}
				}
				/* fallthrough */
			default:
				if (i) {
					int t = find_char(c, ' ');
					if (t == -1)
						t = find_char(c, '\n');
					if (t == -1)
						t = find_char(c, '\0');
					arg->argv[j] = malloc((t + 1) * sizeof(char));
					strncpy(arg->argv[j], c, t);
					arg->argv[j][t] = '\0';
					j++;
				}
				i = 0;
			}
		}
	}

	interrupt = 0;
	ret = -1;
	if (arg->argc) {
		for (unsigned long k = 0; k < SIZE(func_arr); k++)
			if (strcmp(func_arr[k].name, arg->argv[0]) == 0)
				ret = func_arr[k].ptr(arg);
		if (ret == -1)
			printf("unknown command: %s\n", arg->argv[0]);
	}
	if (ret == ERR_MISS_ARG)
		printf("error: missing argument\n");
	else if (ret == ERR_BAD_ARG)
		printf("error: bad argument\n");
	else if (ret == ERR_MISS_FLAG)
		printf("error: missing flag\n");
end_early:;
	for (i = 0; i < arg->argc; i++)
		if (arg->argv[i])
			free(arg->argv[i]);
	if (arg->argv)
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
	while (move_last)
		move_previous();
}
