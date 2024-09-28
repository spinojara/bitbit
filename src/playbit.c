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

#define _GNU_SOURCE
#include <sys/mman.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/param.h>
#include <signal.h>

#include "search.h"
#include "move.h"
#include "transposition.h"
#include "option.h"
#include "nnue.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "bitboard.h"
#include "moveorder.h"
#include "position.h"
#include "transposition.h"
#include "endgame.h"
#include "history.h"
#include "util.h"
#include "movegen.h"
#include "movepicker.h"
#include "timeman.h"
#include "io.h"
#include "polyglot.h"

const struct searchinfo gsi = { 0 };

static const char *prefix;

static long max_file_size = 1024 * 1024;
static int random_moves_min = 1;
static int random_moves_max = 8;

static atomic_int s = 0;

static pthread_mutex_t filemutex;

static inline void stop(void) {
	if (!atomic_load_explicit(&s, memory_order_relaxed))
		fprintf(stderr, "broadcasting stop signal...\n");
	atomic_store_explicit(&s, 1, memory_order_relaxed);
}

static inline int stopped(void) {
	return atomic_load_explicit(&s, memory_order_relaxed);
}

FILE *newfile(void) {
	if (stopped())
		return NULL;

	static long n = 1;
	static char date[64] = { 0 };

	pthread_mutex_lock(&filemutex);

	char name[BUFSIZ];
	char tmp[64];
	time_t t = time(NULL);
	struct tm tm;
	strftime(tmp, 64, "%Y%m%d", localtime_r(&t, &tm));
	if (strcmp(tmp, date)) {
		memcpy(date, tmp, 64);
		sprintf(name, "%s/selfplay-%s", prefix, date);

		errno = 0;
		if (mkdir(name, 0777) && errno != EEXIST) {
			fprintf(stderr, "error: failed to create directory %s\n", name);
			stop();
			pthread_mutex_unlock(&filemutex);
			return NULL;
		}
		
		n = 1;
	}

	int fd;
	for (fd = -1; n < LONG_MAX && fd == -1; n++) {
		sprintf(name, "%s/selfplay-%s/%ld.bit", prefix, date, n);
		errno = 0;
		fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd == -1 && errno != EEXIST) {
			fprintf(stderr, "error: failed to create file %s\n", name);
			stop();
			pthread_mutex_unlock(&filemutex);
			return NULL;
		}
	}

	pthread_mutex_unlock(&filemutex);

	return fd != -1 ? fdopen(fd, "wb") : NULL;
}

void custom_search(struct position *pos, uint64_t nodes, move_t moves[MOVES_MAX], int64_t evals[MOVES_MAX], struct transpositiontable *tt, struct history *history, uint64_t seed) {
	struct searchinfo si = { 0 };
	si.tt = tt;
	si.ti = NULL;
	si.history = history;
	si.max_nodes = nodes;
	si.seed = seed;

	struct searchstack ss[PLY_MAX + 1] = { 0 };

	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);
	refresh_endgame_key(pos);
	refresh_zobrist_key(pos);

	for (int i = 0; i < MOVES_MAX; i++)
		evals[i] = -VALUE_INFINITE;
	movegen_legal(pos, moves, MOVETYPE_ALL);

	struct movepicker mp = { 0 };
	mp.move = moves;
	mp.eval = evals;

	int ply = 0;
	history_store(pos, si.history, ply);

	for (int depth = 1; depth <= PLY_MAX / 2 && !si.interrupt; depth++) {
		for (int i = 0; moves[i]; i++) {
			move_t *move = &moves[i];

			do_zobrist_key(pos, move);
			do_endgame_key(pos, move);
			do_move(pos, move);
			do_accumulator(pos, move);
			ss[1].move = *move;
			si.nodes++;
			
			int32_t eval = -negamax(pos, depth - 1, ply + 1, -VALUE_MATE, VALUE_MATE, 0, &si, ss + 2);
			if (!si.interrupt)
				evals[i] = eval;

			undo_zobrist_key(pos, move);
			undo_endgame_key(pos, move);
			undo_move(pos, move);
			undo_accumulator(pos, move);
		}

		si.done_depth = depth;

		sort_moves(&mp);
	}
}

int32_t evaluate_material(const struct position *pos) {
	int32_t eval = 0;
	for (int piece = PAWN; piece < KING; piece++)
		eval += popcount(pos->piece[WHITE][piece]) * material_value[piece];
	for (int piece = PAWN; piece < KING; piece++)
		eval -= popcount(pos->piece[BLACK][piece]) * material_value[piece];
	return pos->turn ? eval : -eval;
}

int32_t search_material(struct position *pos, int alpha, int beta) {
	uint64_t checkers = generate_checkers(pos, pos->turn);
	int32_t eval = evaluate_material(pos), best_eval = -VALUE_INFINITE;

	if (!checkers) {
		if (eval >= beta)
			return beta;
		if (eval > alpha)
			alpha = eval;
		best_eval = eval;
	}

	struct pstate pstate;
	pstate_init(pos, &pstate);
	struct movepicker mp;
	movepicker_init(&mp, 1, pos, &pstate, 0, 0, 0, 0, &gsi);
	move_t move;
	while ((move = next_move(&mp))) {
		if (!legal(pos, &pstate, &move))
			continue;
		
		do_move(pos, &move);
		eval = -search_material(pos, -beta, -alpha);
		undo_move(pos, &move);
		
		if (eval > best_eval) {
			best_eval = eval;
			if (eval > alpha) {
				alpha = eval;
				if (eval >= beta)
					break;
			}
		}
	}

	return best_eval;
}

move_t random_move(struct position *pos, uint64_t *seed) {
	move_t moves[MOVES_MAX], filtered[MOVES_MAX] = { 0 };
	movegen_legal(pos, moves, MOVETYPE_ALL);

	int32_t eval = search_material(pos, -VALUE_INFINITE, VALUE_INFINITE);

	int nmoves = 0;
	for (int i = 0; moves[i]; i++) {
		move_t *move = &moves[i];
		do_move(pos, move);
		if (search_material(pos, -VALUE_INFINITE, VALUE_INFINITE) == -eval)
			filtered[nmoves++] = moves[i];
		undo_move(pos, move);
	}

	if (nmoves <= 1)
		return filtered[0];

	return filtered[uniformint(seed, 0, nmoves)];
}

void play_game(FILE *openings, struct transpositiontable *tt, uint64_t nodes, uint64_t *seed, FILE *out) {
	move_t moves[MOVES_MAX];
	int64_t evals[MOVES_MAX];

	struct history h = { 0 };
	struct position pos;

	char result = RESULT_UNKNOWN;
	move_t move = 0;

	int move_value_diff_threshold = 50;

	struct endgame *e;

	int draw_counter = 0;

	startpos(&pos);
	if (!polyglot_explore(openings, &pos, 10, seed)) {
		fprintf(stderr, "error: start position not found in opening book\n");
		stop();
		return;
	}

	int random_moves = uniformint(seed, random_moves_min, random_moves_max + 1);
	for (int i = 0; i < random_moves; i++) {
		move = random_move(&pos, seed);
		if (move)
			do_move(&pos, &move);
		else
			break;
	}

	movegen_legal(&pos, moves, MOVETYPE_ALL);
	if (!moves[0])
		return;

	history_reset(&pos, &h);
	refresh_endgame_key(&pos);
	refresh_zobrist_key(&pos);

	int32_t eval[POSITIONS_MAX] = { 0 };

	while (result == RESULT_UNKNOWN) {
		custom_search(&pos, nodes, moves, evals, tt, &h, *seed);
	
		move_t *bestmove = moves;
		eval[h.ply] = evals[0];
		int32_t eval_now = eval[h.ply];

		if (-8 <= eval_now && eval_now <= 8)
			draw_counter++;
		else
			draw_counter = 0;

		int is_check = generate_checkers(&pos, pos.turn) != 0;

		if (!*bestmove) {
			result = is_check ? (pos.turn ? RESULT_LOSS : RESULT_WIN) : RESULT_DRAW;
			break;
		}
		else if ((pos.halfmove >= 40 && draw_counter >= 8) || pos.halfmove >= 100 || repetition(&pos, &h, 0, 2))
			result = RESULT_DRAW;
		else if ((e = endgame_probe(&pos))) {
			int32_t v = endgame_evaluate(e, &pos);
			if (v != VALUE_NONE) {
				v *= 2 * pos.turn - 1;
				if (v > VALUE_WIN / 2)
					result = RESULT_WIN;
				else if (v < -VALUE_WIN / 2)
					result = RESULT_LOSS;
				else
					result = RESULT_DRAW;
			}
		}
		
		int nmoves;
		for (nmoves = 1; moves[nmoves]; nmoves++)
			if (evals[nmoves] < eval_now - move_value_diff_threshold)
				break;

		move_value_diff_threshold = max(15, move_value_diff_threshold - 2);

		int skip = is_capture(&pos, bestmove) || is_check || move_flag(bestmove);
		
		if (eval_now < VALUE_WIN) {
			/* Density on (0,1) is f(x)=1.5-x so we are slightly
			 * more likely to pick better moves. */
			double r = 1.5 - sqrt(2.25 - 2.0 * uniform(seed));
			r = MIN(MAX(r, 0.0), 0.999);
			move = moves[(int)(nmoves * r)];
		}
		else {
			move = *bestmove;
		}
		char str1[128], str2[16];
		printf("%s\n", pos_to_fen(str1, &pos));
		printf("%s (%s) %d\n", move_str_pgn(str1, &pos, &move), move_str_pgn(str2, &pos, bestmove), eval_now);

		if (skip)
			eval[h.ply] = VALUE_NONE;

		history_next(&pos, &h, move);
	}

	if (h.ply) {
		if (write_move(out, 0)) {
			fprintf(stderr, "error: failed to write move\n");
			stop();
		}
		if (write_position(out, &h.start)) {
			fprintf(stderr, "error: failed to write position\n");
			stop();
		}
		if (write_result(out, result)) {
			fprintf(stderr, "error: failed to write result\n");
			stop();
		}
		for (int i = 0; i <= h.ply; i++) {
			if (write_eval(out, eval[i])) {
				fprintf(stderr, "error: failed to write eval\n");
				stop();
			}
			if (i < h.ply) {
				if (write_move(out, h.move[i])) {
					fprintf(stderr, "error: failed to write move\n");
					stop();
				}
			}
		}
	}

}

struct threadinfo {
	int jobn;
	uint64_t seed;
	uint64_t nodes;
	size_t tt_size;
	const char *openings;
};

void *playthread(void *arg) {
	struct threadinfo *ti = (struct threadinfo *)arg;
	uint64_t seed = ti->seed;
	uint64_t nodes = ti->nodes;
	struct transpositiontable tt = { 0 };
	if (transposition_alloc(&tt, ti->tt_size)) {
		fprintf(stderr, "error: failed to allocate transposition table\n");
		stop();
	}

	FILE *openings = fopen(ti->openings, "rb");
	if (!openings) {
		fprintf(stderr, "error: failed to open file %s\n", ti->openings);
		stop();
	}

	while (!stopped()) {
		FILE *f = newfile();
		if (!f) {
			fprintf(stderr, "error: failed to create new file\n");
			stop();
			break;
		}

		while (ftell(f) < max_file_size && !stopped()) {
			play_game(openings, &tt, nodes, &seed, f);
		}

		fclose(f);

	}

	transposition_free(&tt);
	fclose(openings);

	printf("exiting thread %d\n", ti->jobn);

	return NULL;
}

static void sigint(int num) {
	(void)num;
	fprintf(stderr, "\nenter 'quit' to quit\n");
	signal(SIGINT, &sigint);
}

int main(int argc, char **argv) {
	signal(SIGINT, &sigint);

	int jobs = 1;
	int tt_MiB = 6 * 1024;
	int nodes = 15000;
	static struct option opts[] = {
		{ "jobs",    required_argument, NULL, 'j' },
		{ "tt",      required_argument, NULL, 't' },
		{ "nodes",   required_argument, NULL, 'n' },
	};

	char *endptr;
	int c, option_index = 0;
	int error = 0;
	while ((c = getopt_long(argc, argv, "j:t:", opts, &option_index)) != -1) {
		switch (c) {
		case 't':
			errno = 0;
			tt_MiB = strtol(optarg, &endptr, 10);
			if (errno || *endptr != '\0' || tt_MiB <= 1024) {
				error = 1;
				fprintf(stderr, "error: bad argument: tt\n");
			}
			break;
		case 'j':
			errno = 0;
			jobs = strtol(optarg, &endptr, 10);
			if (errno || *endptr != '\0' || jobs < 1 || jobs > 1024) {
				error = 1;
				fprintf(stderr, "error: bad argument: jobs\n");
			}
			break;
		case 'n':
			errno = 0;
			nodes = strtoll(optarg, &endptr, 10);
			if (errno || *endptr != '\0' || nodes < 100) {
				error = 1;
				fprintf(stderr, "error: bad argument: nodes\n");
			}
			break;
		default:
			error =1;
			break;
		}
	}
	if (error)
		return 1;

	if (optind + 1 >= argc) {
		fprintf(stderr, "usage: %s datadir openings\n", argv[0]);
		return 2;
	}
	prefix = argv[optind];
	if (strlen(prefix) >= BUFSIZ - 1024) {
		fprintf(stderr, "error: strlen of datadir is too large (%ld)\n", strlen(prefix));
		return 3;
	}

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	search_init();
	moveorder_init();
	position_init();
	transposition_init();
	endgame_init();
	history_init();
	nnue_init();

	option_history = 1;
	option_nnue = 1;
	option_pure_nnue = 1;
	option_transposition = 1;
	option_deterministic = 0;

	pthread_mutex_init(&filemutex, NULL);

	timepoint_t t = time_now();

	pthread_t *thread = malloc(jobs * sizeof(*thread));
	struct threadinfo *ti = malloc(jobs * sizeof(*ti));

	for (int i = 0; i < jobs; i++) {
		ti[i].jobn = i;
		ti[i].seed = t + i;
		ti[i].tt_size = tt_MiB / jobs;
		ti[i].nodes = nodes;
		ti[i].openings = argv[optind + 1];

		pthread_create(&thread[i], NULL, &playthread, &ti[i]);
		printf("started thread %d\n", i);
	}

	char buf[BUFSIZ];
	while (fgets(buf, sizeof(buf), stdin) && strcmp(buf, "stop\n") && strcmp(buf, "q\n") && strcmp(buf, "quit\n"));
	stop();

	for (int i = 0; i < jobs; i++)
		pthread_join(thread[i], NULL);

	pthread_mutex_destroy(&filemutex);

	free(ti);
	free(thread);
}
