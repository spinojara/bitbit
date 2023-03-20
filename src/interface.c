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
#include <signal.h>

#include "bitboard.h"
#include "util.h"
#include "position.h"
#include "perft.h"
#include "search.h"
#include "evaluate.h"
#include "transposition_table.h"
#include "version.h"
#include "interrupt.h"
#include "history.h"
#include "init.h"

struct func {
	char *name;
	int (*ptr)(int argc, char **argv);
};

int interface_help(int argc, char **argv);
int interface_move(int argc, char **argv);
int interface_undo(int argc, char **argv);
int interface_flip(int argc, char **argv);
int interface_perft(int argc, char **argv);
int interface_position(int argc, char **argv);
int interface_clear(int argc, char **argv);
int interface_stop(int argc, char **argv);
int interface_quit(int argc, char **argv);
int interface_eval(int argc, char **argv);
int interface_go(int argc, char **argv);
int interface_version(int argc, char **argv);
int interface_tt(int argc, char **argv);
int interface_isready(int argc, char **argv);
int interface_uci(int argc, char **argv);
int interface_ucinewgame(int argc, char **argv);

struct func func_arr[] = {
	{ "help"       , interface_help       , },
	{ "move"       , interface_move       , },
	{ "undo"       , interface_undo       , },
	{ "flip"       , interface_flip       , },
	{ "perft"      , interface_perft      , },
	{ "position"   , interface_position   , },
	{ "clear"      , interface_clear      , },
	{ "quit"       , interface_quit       , },
	{ "eval"       , interface_eval       , },
	{ "go"         , interface_go         , },
	{ "version"    , interface_version    , },
	{ "tt"         , interface_tt         , },
	{ "isready"    , interface_isready    , },
	{ "uci"        , interface_uci        , },
	{ "ucinewgame" , interface_ucinewgame , },
};

struct position *pos = NULL;
struct history *history = NULL;

int interface_help(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	printf("The following commands are available in bitbit:\n");
	for (size_t k = 0; k < SIZE(func_arr); k++)
		printf("%s\n", func_arr[k].name);
	
	return DONE;
}

int interface_move(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (argc < 2) {
		return ERR_MISS_ARG;
	}
	else {
		move m = string_to_move(pos, argv[1]);
		if (m)
			move_next(&pos, &history, m);
		else
			return ERR_BAD_ARG;
	}
	return DONE;
}

int interface_undo(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (history)
		move_previous(&pos, &history);
	else
		printf("error: no move to undo\n");
	return DONE;
}

int interface_flip(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (!generate_checkers(pos, pos->turn)) {
		delete_history(&history);
		do_null_move(pos, 0);
	}
	else {
		printf("error: cannot flip the move\n");
	}
	return DONE;
}

int interface_perft(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (argc < 2)
		return ERR_MISS_ARG;

	clock_t t = clock();
	uint64_t p = perft(pos, strint(argv[1]), 1);
	t = clock() - t;
	if (interrupt)
		return DONE;
	printf("nodes: %" PRIu64 "\n", p);
	printf("time: %.2f\n", (double)t / CLOCKS_PER_SEC);
	if (t != 0)
		printf("mpns: %" PRIu64 "\n",
			(p * CLOCKS_PER_SEC / ((uint64_t)t * 1000000)));
	
	return DONE;
}

int interface_position(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	int n;
	for (n = 0; n < argc; n++)
		if (strcmp(argv[n], "moves") == 0)
			break;
	if (argc < 2) {
		print_position(pos, 0);
		char t[128];
		printf("turn: %c\n", "bw"[pos->turn]);
		printf("castling: %s\n", castle_string(t, pos->castle));
		printf("en passant: %s\n", pos->en_passant ? algebraic(t, pos->en_passant) : "-");
		printf("halfmove: %i\n", pos->halfmove);
		printf("fullmove: %i\n", pos->fullmove);
		printf("zobrist key: 0x%" PRIX64 "\n", pos->zobrist_key);
		printf("fen: %s\n", pos_to_fen(t, pos));
		print_history_pgn(history);
	}
	else if (strcmp(argv[1], "startpos") == 0) {
		char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
		pos_from_fen(pos, SIZE(fen), fen);
		delete_history(&history);
		for (int i = n + 1; i < argc; i++) {
			if (string_to_move(pos, argv[i]))
				move_next(&pos, &history, string_to_move(pos, argv[i]));
			else
				break;
		}
	}
	else if (strcmp(argv[1], "fen") == 0) {
		if (fen_is_ok(n - 2, argv + 2)) {
			pos_from_fen(pos, n - 2, argv + 2);
			delete_history(&history);
			for (int i = n + 1; i < argc; i++) {
				if (string_to_move(pos, argv[i]))
					move_next(&pos, &history, string_to_move(pos, argv[i]));
				else
					break;
			}
		}
		else {
			return ERR_BAD_ARG;
		}
	}
	else {
		return ERR_BAD_ARG;
	}
	return DONE;
}

int interface_clear(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	printf("\033[1;1H\033[2J");
	return DONE;
}

int interface_stop(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	interrupt = 1;
	return EXIT_LOOP;
}

int interface_quit(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	interface_stop(argc, argv);
	return EXIT_LOOP;
}

int interface_eval(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	print_evaluation(pos);
	return DONE;
}

int interface_go(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	int depth = 255;
	int wtime = 0;
	int btime = 0;
	int winc = 0;
	int binc = 0;
	int movetime = 0;
	UNUSED(winc);
	UNUSED(binc);
	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], "depth") == 0)
			depth = strint(argv[i + 1]);
		if (strcmp(argv[i], "wtime") == 0)
			wtime = strint(argv[i + 1]);
		if (strcmp(argv[i], "btime") == 0)
			btime = strint(argv[i + 1]);
		if (strcmp(argv[i], "winc") == 0)
			winc = strint(argv[i + 1]);
		if (strcmp(argv[i], "binc") == 0)
			binc = strint(argv[i + 1]);
		if (strcmp(argv[i], "movetime") == 0)
			movetime = strint(argv[i + 1]);
	}
	evaluate(pos, depth, 1, pos->turn ? wtime : btime, movetime, NULL, history, 1);
	return DONE;
}

int interface_version(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);

	version();
	printf("c compiler: %s\n", compiler);
	printf("environment: %s\n", environment);
	char t[8];
	printf("compilation date: %s\n", date(t));
	printf("transposition table size: ");
	transposition_table_size_print(log_2(sizeof(struct transposition) * transposition_table_size()));
	printf("\ntransposition entry size: %" PRIu64 "B\n", sizeof(struct transposition));

	return DONE;
}

int interface_tt(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (argc < 2) {
		transposition_table_size_print(log_2(transposition_table_size() *
					sizeof(struct transposition)));
		printf("\n%d%%\n", transposition_table_occupancy());
	}
	else if (strcmp(argv[1], "clear") == 0) {
		transposition_table_clear();
	}
	else if (argc >= 3 && strcmp(argv[1], "set") == 0) {
		int ret = allocate_transposition_table(strint(argv[2]));
		if (ret == 2)
			return ERR_BAD_ARG;
		if (ret == 3)
			return EXIT_LOOP;
	}
	else {
		return ERR_BAD_ARG;
	}

	return DONE;
}

int interface_isready(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	printf("readyok\n");
	return DONE;
}

int interface_uci(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	printf("id name bitbit\n");
	printf("id author Isak Ellmer\n");
	printf("uciok\n");
	return DONE;
}

int interface_ucinewgame(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	srand(time(NULL));
	zobrist_key_init();
	char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
	pos_from_fen(pos, SIZE(fen), fen);
	delete_history(&history);
	transposition_table_clear();
	return DONE;
}

/* max amount of args */
#define ARGC 1024
/* long arg length */
#define ARGL 128
/* short arg length */
#define ARGLS 6

int arg_len(int argc) {
	return argc < 8 ? ARGL : ARGLS;
}

int parse(int *argc, char ***argv) {
	interrupt = 0;
	int ret = -1;
	int i, j;
	char *c;
	int argc_t = 0;
	char **argv_t;
	argv_t = malloc(ARGC * sizeof(char *));
	for (i = 0; i < ARGC; i++)
		argv_t[i] = malloc(arg_len(i) * sizeof(char));
	if (*argc > 1) {
		for (i = 1; i < *argc; i++) {
			if (argc_t >= ARGC)
				break;
			c = (*argv)[i];
			/* skip */
			if (c[0] == ',')
				break;
			/* arg */
			argc_t++;
			for (j = 0; j < arg_len(argc_t - 1) - 1 && c[j]; j++)
				argv_t[argc_t - 1][j] = c[j];
			argv_t[argc_t - 1][j] = '\0';
			
		}
		
		*argc -= i;
		*argv += i;
	}
	else {
		char line[BUFSIZ];
		if (fgets(line, sizeof(line), stdin)) {
			c = line;
			/* assumes always at start of word */
			for (; *c && *c != '\n'; c++) {
				if (argc_t >= ARGC)
					break;
				/* jump to start of word */
				while (c[0] == ' ')
					c = c + 1;
				/* break at end of line */
				if (c[0] == '\0' || c[0] == '\n')
					break;
				/* arg */
				argc_t++;
				for (j = 0; j < arg_len(argc_t - 1) - 1 && c[0] && c[0] != ' ' && c[0] != '\n'; j++, c++)
					argv_t[argc_t - 1][j] = c[0];
				argv_t[argc_t - 1][j] = '\0';
			}
		}
	}

	if (interrupt)
		ret = EXIT_LOOP;
	else if (argc) {
		struct func *f = NULL;
		for (size_t k = 0; k < SIZE(func_arr); k++)
			if (strcmp(func_arr[k].name, argv_t[0]) == 0)
				f = func_arr + k;
		if (f)
			ret = f->ptr(argc_t, argv_t);
		switch(ret) {
		case -1:
			printf("unknown command: %s\n", argv_t[0]);
			break;
		case ERR_MISS_ARG:
			printf("error: missing argument\n");
			break;
		case ERR_BAD_ARG:
			printf("error: bad argument\n");
			break;
		}
	}
	for (i = 0; i < ARGC; i++)
		free(argv_t[i]);
	free(argv_t);

	return ret;
}

int interface(int argc, char **argv) {
	printf("\33[2Kbitbit Copyright (C) 2022 Isak Ellmer\n");
	while (parse(&argc, &argv) != 1);
	return 0;
}

void interface_init(void) {
	char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
	pos = malloc(sizeof(struct position));
	pos_from_fen(pos, SIZE(fen), fen);
}

void interface_term(void) {
	free(pos);
	delete_history(&history);
}
