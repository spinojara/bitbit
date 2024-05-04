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

#include "interface.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <stdatomic.h>

#include "bitboard.h"
#include "util.h"
#include "position.h"
#include "perft.h"
#include "init.h"
#include "search.h"
#include "timeman.h"
#include "evaluate.h"
#include "transposition.h"
#include "version.h"
#include "history.h"
#include "option.h"
#include "thread.h"

#define LINESIZE 16384
#define ARGSIZE 4096

struct command {
	char *name;
	int (*ptr)(int argc, char **argv);
};

int interface_version(int argc, char **argv);
int interface_help(int argc, char **argv);
int interface_quit(int argc, char **argv);
int interface_ponderhit(int argc, char **argv);
int interface_stop(int argc, char **argv);
int interface_clear(int argc, char **argv);
int interface_move(int argc, char **argv);
int interface_undo(int argc, char **argv);
int interface_flip(int argc, char **argv);
int interface_mirror(int argc, char **argv);
int interface_perft(int argc, char **argv);
int interface_position(int argc, char **argv);
int interface_stop(int argc, char **argv);
int interface_eval(int argc, char **argv);
int interface_go(int argc, char **argv);
int interface_tt(int argc, char **argv);
int interface_isready(int argc, char **argv);
int interface_uci(int argc, char **argv);
int interface_ucinewgame(int argc, char **argv);
int interface_setoption(int argc, char **argv);

#define COMMAND(name) { #name, interface_##name }

static const struct command commands[] = {
	COMMAND(help),
	COMMAND(move),
	COMMAND(undo),
	COMMAND(flip),
	COMMAND(mirror),
	COMMAND(perft),
	COMMAND(position),
	COMMAND(clear),
	COMMAND(quit),
	COMMAND(stop),
	COMMAND(ponderhit),
	COMMAND(eval),
	COMMAND(go),
	COMMAND(version),
	COMMAND(tt),
	COMMAND(isready),
	COMMAND(uci),
	COMMAND(setoption),
	COMMAND(ucinewgame),
};

struct position pos;
struct transpositiontable tt;
struct history history;

int interface_help(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	printf("The following commands are available in bitbit:\n");
	for (size_t k = 0; k < SIZE(commands); k++)
		printf("%s\n", commands[k].name);
	
	return DONE;
}

int interface_move(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (argc < 2) {
		return ERR_MISS_ARG;
	}
	else {
		move_t move = string_to_move(&pos, argv[1]);
		if (move)
			history_next(&pos, &history, move);
		else
			return ERR_BAD_ARG;
	}
	return DONE;
}

int interface_undo(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (history.ply)
		history_previous(&pos, &history);
	return DONE;
}

int interface_flip(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (!generate_checkers(&pos, pos.turn)) {
		do_null_move(&pos, 0);
		history_reset(&pos, &history);
	}
	else {
		printf("error: cannot flip the move\n");
	}
	return DONE;
}

int interface_mirror(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	mirror_position(&pos);
	return DONE;
}

int interface_perft(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (argc < 2)
		return ERR_MISS_ARG;

	clock_t t = clock();
	uint64_t p = perft(&pos, strint(argv[1]), 1);
	t = clock() - t;
	printf("nodes: %" PRIu64 "\n", p);
	printf("time: %.2f\n", (double)t / CLOCKS_PER_SEC);
	if (t != 0)
		printf("mnps: %" PRIu64 "\n",
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
		print_position(&pos);
		printf("current: ");
		print_fen(&pos);
		printf("start:   ");
		print_fen(&history.start);
		print_history_pgn(&history);
	}
	else if (strcmp(argv[1], "startpos") == 0) {
		startpos(&pos);
		startkey(&pos);
		history_reset(&pos, &history);
		for (int i = n + 1; i < argc; i++) {
			move_t move = string_to_move(&pos, argv[i]);
			if (move)
				history_next(&pos, &history, move);
			else
				break;
		}
	}
	else if (strcmp(argv[1], "fen") == 0) {
		if (fen_is_ok(n - 2, argv + 2)) {
			pos_from_fen(&pos, n - 2, argv + 2);
			refresh_zobrist_key(&pos);
			history_reset(&pos, &history);
			for (int i = n + 1; i < argc; i++) {
				move_t move = string_to_move(&pos, argv[i]);
				if (move)
					history_next(&pos, &history, move);
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
	search_stop();
	return DONE;
}

int interface_quit(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	interface_stop(argc, argv);
	return EXIT_LOOP;
}

int interface_ponderhit(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	search_ponderhit();
	return DONE;
}

int interface_eval(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	evaluate_print(&pos);
	return DONE;
}

int interface_go(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	int depth = 255;
	struct timeinfo ti = { 0 };
	for (int i = 1; i < argc; i++) {
		if (i < argc - 1) {
			if (strcmp(argv[i], "depth") == 0)
				depth = strint(argv[i + 1]);
			else if (strcmp(argv[i], "wtime") == 0)
				ti.etime[WHITE] = 1000 * strint(argv[i + 1]);
			else if (strcmp(argv[i], "btime") == 0)
				ti.etime[BLACK] = 1000 * strint(argv[i + 1]);
			else if (strcmp(argv[i], "winc") == 0)
				ti.einc[WHITE] = 1000 * strint(argv[i + 1]);
			else if (strcmp(argv[i], "binc") == 0)
				ti.einc[BLACK] = 1000 * strint(argv[i + 1]);
			else if (strcmp(argv[i], "movetime") == 0)
				ti.movetime = 1000 * strint(argv[i + 1]);
			else if (strcmp(argv[i], "movestogo") == 0)
				ti.movestogo = strint(argv[i + 1]);
		}
		if (strcmp(argv[i], "ponder") == 0)
			uciponder = 1;
	}

	ucigo = 1;
	search_start(&pos, depth, &ti, &tt, &history);
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
	printf("simd: %s\n", simd);
	printf("transposition table size: %" PRIu64 " B (%" PRIu64 " MiB)\n", tt.size * sizeof(*tt.table), tt.size * sizeof(*tt.table) / (1024 * 1024));
	printf("transposition entry size: %" PRIu64 " B\n", sizeof(struct transposition));
	return DONE;
}

int interface_tt(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	printf("pv    nodes: %4d pm\n", transposition_occupancy(&tt, BOUND_EXACT));
	printf("cut   nodes: %4d pm\n", transposition_occupancy(&tt, BOUND_LOWER));
	printf("all   nodes: %4d pm\n", transposition_occupancy(&tt, BOUND_UPPER));
	printf("total nodes: %4d pm\n", transposition_occupancy(&tt, 0));
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
	printf("id name bitbit " XSTR(VERSION) "\n");
	printf("id author Isak Ellmer\n");
	print_options();
	printf("uciok\n");
	return DONE;
}

int interface_setoption(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	if (argc >= 5 && !strcasecmp(argv[2], "hash")) {
		size_t MiB = strint(argv[4]);
		transposition_free(&tt);
		if (transposition_alloc(&tt, MiB * 1024 * 1024)) {
			fprintf(stderr, "error: failed to allocate transposition table\n");
			exit(1);
		}
		if (!MiB)
			option_transposition = 0;
	}
	else {
		setoption(argc, argv, &tt);
	}
	return DONE;
}

int interface_ucinewgame(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	startpos(&pos);
	startkey(&pos);
	history_reset(&pos, &history);
	transposition_clear(&tt);
	return DONE;
}

int next_char(int argc, char **argv) {
	static int argindex = 1;
	static size_t charindex = 0;

	int c = argindex < argc ? argv[argindex][charindex++] : getchar();

	switch (c) {
	case '\r':
	case '\n':
	case ',':
		return '\0';
	case '\0':
		if (argindex < argc) {
			argindex++;
			charindex = 0;
		}
		return argindex == argc ? '\0' : ' ';
	case ' ':
		return ' ';
	default:
		return c;
	}
}

void parse_line(int *argc, char *argv[ARGSIZE], int margc, char **margv, char *line) {
	*argc = 0;
	int next = 1;

	int i;
	int c = 1;
	for (i = 0; i < LINESIZE && c && c != EOF; ) {
		c = next_char(margc, margv);
		switch (c) {
		case ' ':
			if (!next) {
				line[i++] = '\0';
				next = 1;
			}
			break;
		case EOF:
			/* fallthrough */
		case '\0':
			line[i++] = '\0';
			break;
		default:
			if (next) {
				if (*argc == ARGSIZE)
					goto error;
				argv[(*argc)++] = &line[i];
				next = 0;
			}
			line[i++] = c;
			break;
		}
	}
	if (i == LINESIZE)
		goto error;
	return;
error:
	fprintf(stderr, "error: too many arguments\n");
	exit(2);
}

int is_allowed(const char *arg) {
	if (!atomic_load_explicit(&uciponder, memory_order_relaxed) && !strcmp(arg, "ponderhit"))
		return 0;

	if (!atomic_load_explicit(&ucigo, memory_order_relaxed))
		return 1;

	if (!strcmp(arg, "stop") || !strcmp(arg, "ponderhit"))
		return 1;

	return 0;
}

int parse(int margc, char **margv) {
	char line[LINESIZE];
	int ret = -1;
	int argc;
	char *argv[ARGSIZE];

	parse_line(&argc, argv, margc, margv, line);

	if (argc && !is_allowed(argv[0])) {
		fprintf(stderr, "error: %s: illegal command\n", argv[0]);
		return DONE;
	}

	if (argc) {
		const struct command *f = NULL;
		for (size_t k = 0; k < SIZE(commands); k++)
			if (strcmp(commands[k].name, argv[0]) == 0)
				f = &commands[k];
		if (f)
			ret = f->ptr(argc, argv);

		switch(ret) {
		case -1:
			fprintf(stderr, "error: %s: command not found\n", argv[0]);
			break;
		case ERR_MISS_ARG:
			fprintf(stderr, "error: %s: missing argument\n", argv[0]);
			break;
		case ERR_BAD_ARG:
			fprintf(stderr, "error: %s: bad argument\n", argv[0]);
			break;
		}
	}

	return ret;
}

void interface_init(void) {
	startpos(&pos);
	startkey(&pos);
	if (transposition_alloc(&tt, TT * 1024 * 1024)) {
		fprintf(stderr, "error: failed to allocate transposition table\n");
		exit(1);
	}
	history_reset(&pos, &history);
	thread_init();
}

void interface_term(void) {
	thread_term();
	transposition_free(&tt);
}

int interface(int argc, char **argv) {
	interface_init();
	while (parse(argc, argv) != EXIT_LOOP);
	interface_term();
	return 0;
}
