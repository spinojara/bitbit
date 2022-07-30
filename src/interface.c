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
#include "move.h"
#include "perft.h"
#include "evaluate.h"
#include "hash_table.h"
#include "version.h"

struct func {
	char *name;
	int (*ptr)(struct arg *arg);
};

struct move_linked {
	move *move;
	struct move_linked *next;
	struct move_linked *previous;
};

struct position *pos = NULL;
struct move_linked *move_last = NULL;

void move_next(move m) {
	if (!move_last) {
		move_last = malloc(sizeof(struct move_linked));
		move_last->previous = NULL;
	}
	else {
		move_last->next = malloc(sizeof(struct move_linked));
		move_last->next->previous = move_last;
		move_last = move_last->next;
	}
	move_last->next = NULL;
	move_last->move = malloc(sizeof(move));
	*(move_last->move) = m;
}

void move_previous() {
	if (!move_last)
		return;
	free(move_last->move);
	if (!move_last->previous) {
		free(move_last);
		move_last = NULL;
	}
	else {
		move_last = move_last->previous;
		free(move_last->next);
		move_last->next = NULL;
	}
}

int interface_help(struct arg *arg) {
	UNUSED(arg);
	printf(
	"The following commands are available in bitbit:\n"
	"exit\n"
	"help\n"
	"version\n"
	"clear [-h]\n"
	"setpos [-r] [fen]\n"
	"domove [-fr] [move]\n"
	"perft [-tv] [depth]\n"
	"eval [-hmtv] [depth]\n"
	"print [-v]\n"
	);
	return 1;
}

int interface_domove(struct arg *arg) {
	UNUSED(arg);
	if (arg->f) {
		/* should check if king can now be captured */
		swap_turn(pos);
	}
	else if (arg->r) {
		if (move_last) {
			undo_move_zobrist(pos, move_last->move);
			move_previous();
		}
		else {
			printf("error: no move to undo\n");
		}
	}
	else if (arg->argc < 2) {
		return 2;
	}
	else {
		move_next(string_to_move(pos, arg->argv[1]));
		if (*(move_last->move)) {
			do_move_zobrist(pos, move_last->move);
		}
		else {
			move_previous();
			return 3;
		}
	}
	return 1;
}

int interface_perft(struct arg *arg) {
	UNUSED(arg);
	if (arg->argc < 2) {
		return 2;
	}
	else {
		if (string_is_int(arg->argv[1])) {
			clock_t t = clock();
			uint64_t p = perft(pos, atoi(arg->argv[1]), 1, arg->v);
			t = clock() - t;
			if (arg->t) {
				printf("time: %.2f\n", (double)t / CLOCKS_PER_SEC);
				if (t != 0)
					printf("mpns: %i\n", (int)(p * CLOCKS_PER_SEC / (t * 1000000)));
			}
		}
		else {
			return 3;
		}
	}
	return 1;
}

int interface_setpos(struct arg *arg) {
	UNUSED(arg);
	if (arg->r) {
		random_pos(pos, 32);
		while (move_last)
			move_previous();
	}
	else if (arg->argc < 2) {
		return 2;
	}
	else {
		if (fen_is_ok(arg->argc - 1, arg->argv + 1)) {
			pos_from_fen(pos, arg->argc - 1, arg->argv + 1);
			while (move_last)
				move_previous();
		}
		else {
			return 3;
		}
	}
	return 1;
}

int interface_clear(struct arg *arg) {
	UNUSED(arg);
	if (arg->h)
		hash_table_clear();
	else
#if defined(_WIN32)
		if (system("cls"))
#else
		if (system("clear"))
#endif
			return 0;
	return 1;
}

int interface_exit(struct arg *arg) {
	UNUSED(arg);
	return 0;
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
	return 1;
}

int interface_eval(struct arg *arg) {
	UNUSED(arg);
	if (arg->argc < 2) {
		return 2;
	}
	else {
		if (string_is_int(arg->argv[1])) {
			uint64_t s;
			move *m = malloc(sizeof(move));
			clock_t t = clock();
			if (arg->h)
				s = evaluate_hash(pos, atoi(arg->argv[1]), m, arg->v);
			else
				s = evaluate(pos, atoi(arg->argv[1]), m, arg->v);
			t = clock() - t;
			/* arg->v already sent to evaluate */
			if (!arg->v) {
				printf("%.2f ", (double)s / 100);
				print_move(m);
				printf("\n");
			}
			if (arg->t)
				printf("time: %.2f\n", (double)t / CLOCKS_PER_SEC);
			if (arg->m && *m) {
				move_next(*m);
				do_move_zobrist(pos, move_last->move);
			}
			free(m);
		}
		else {
			return 3;
		}
	}
	return 1;
}

int interface_version(struct arg *arg) {
	UNUSED(arg);

	printf("bitbit Copyright (C) 2022 Isak Ellmer  \n");
	printf("c compiler: %s\n", compiler);
	printf("environment: %s\n", environment);
	char t[8];
	printf("compilation date: %s\n", date(t));
	printf("hash table size: %" PRIu64 "B\n", (hash_table_size_bytes()
							/ sizeof(struct hash_entry))
			 				* sizeof(struct hash_entry));
	printf("hash entry size: %" PRIu64 "B\n", sizeof(struct hash_entry));

	return 1;
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
};

int parse(int *argc, char ***argv) {
	struct arg *arg = calloc(1, sizeof(struct arg));
	if (!arg)
		return 0;

	int i, j;
	int ret = 1;
	char *c;
	if (*argc > 1) {
		/* get arg->argc */
		arg->argc = 0;
		for (i = 1; i < *argc; i++) {
			if ((*argv)[i][0] != '-' || strlen((*argv)[i]) == 1)
				arg->argc++;
			if ((*argv)[i][0] == ',') {
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
			if ((*argv)[i + j][0] == '-' && strlen((*argv)[i + j]) != 1) {
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

		*argc -= arg->argc + 1;
		*argv += arg->argc + 1;
	}
	else {
		char line[BUFSIZ];
		/* prompt */
		printf("> ");
		if (!fgets(line, sizeof(line), stdin)) {
			ret = 0;
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

	if (arg->argc) {
		for (unsigned long k = 0; k < SIZE(func_arr); k++) {
			if (strcmp(func_arr[k].name, arg->argv[0]) == 0) {
				ret = func_arr[k].ptr(arg);
				goto end_early;
			}
		}
		printf("unknown command: %s\n", arg->argv[0]);
	}
end_early:;
	if (ret == 2)
		printf("error: missing argument\n");
	else if (ret == 3)
		printf("error: bad argument\n");
	else if (ret == 4)
		printf("error: missing flag\n");
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
	while (parse(&argc, &argv));
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
