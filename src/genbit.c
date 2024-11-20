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
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <math.h>
#include <getopt.h>
#ifdef SYZYGY
#include <tbprobe.h>
#endif

#include "position.h"
#include "move.h"
#include "movegen.h"
#include "util.h"
#include "magicbitboard.h"
#include "bitboard.h"
#include "moveorder.h"
#include "attackgen.h"
#include "search.h"
#include "evaluate.h"
#include "tables.h"
#include "option.h"
#include "transposition.h"
#include "history.h"
#include "timeman.h"
#include "endgame.h"
#include "io.h"

/* <x * 1024 * 1024> gives a <8 * x> MiB hash table.
 * In particular x = 128 gives a 1024 MiB hash table.
 */
#define HASH_SIZE (128 * 1024 * 1024)

#define HASH_INDEX (HASH_SIZE - 1)

atomic_uint_least64_t *hash_table;

ssize_t tt_MiB = 12 * 1024;

int random_move_ply = 25;
int random_moves = 7;
int min_ply = 16;
int max_ply = 400;
int draw_ply = 80;
int draw_eval = 10;
int eval_limit = 3000;
#ifdef SYZYGY
char *syzygy = NULL;
#endif

const int report_dot_every = 1000;
const int dots_per_clear = 20;
const int report_every = 200000;

time_t start = 0;

const move_t synchronize_threads = M(a1, b4, 0, 0);

struct threadinfo {
	int threadn;
	atomic_long available;
	int depth;
	uint64_t seed;
	int fd[2];

	struct transpositiontable tt;
};

pthread_mutex_t lock;

atomic_int stop = 0;

timepoint_t last_time;
long last_fens = 0;
long dot_last_fens = 0;
void report(long curr_fens, long fens) {
	if (curr_fens != last_fens && curr_fens % report_every == 0) {
		timepoint_t tp = time_now();
		time_t total = fens * (time(NULL) - start) / curr_fens;
		time_t done = start + total;
		printf("\r%lld%% %ld fens at %ld fens/second. Eta is %s", 100ll * curr_fens / fens, curr_fens,
				(long)(tp - last_time ? (double)TPPERSEC * (curr_fens - last_fens) / (tp - last_time) : 0),
				ctime(&done));
		fflush(stdout);
		last_time = tp;
		last_fens = curr_fens;
	}
}

void report_dot(long curr_fens) {
	if (curr_fens != dot_last_fens && curr_fens % report_dot_every == 0) {
		if (curr_fens % (dots_per_clear * report_dot_every) == 0)
			printf("\33[2K\r");
		printf(".");
		fflush(stdout);
		dot_last_fens = curr_fens;
	}
}

static inline int position_already_written(struct position *pos) {
	uint64_t index = pos->zobrist_key & HASH_INDEX;
	uint64_t old_zobrist_key = atomic_load(&hash_table[index]);
	if (old_zobrist_key == pos->zobrist_key)
		return 1;
	atomic_store(&hash_table[index], pos->zobrist_key);
	return 0;
}

int probable_long_draw(struct history *h, int32_t eval, int *drawn_score_count) {
	if (h->ply >= draw_ply && abs(eval) <= draw_eval)
		++*drawn_score_count;
	else
		*drawn_score_count = 0;

	if (*drawn_score_count >= 8)
		return 1;

	return 0;
}

void random_move_flags(int *random_move, uint64_t *seed) {
	for (int i = 0; i < random_move_ply; i++)
		random_move[i] = (i < random_moves);

	for (int i = random_move_ply - 1; i > 0; i--) {
		int j = xorshift64(seed) % (i + 1);
		int t = random_move[i];
		random_move[i] = random_move[j];
		random_move[j] = t;
	}
}

struct threadinfo *choose_thread(struct threadinfo *threadinfo, int threads) {
	int thread;
	int ret;
	int64_t most = -1;
	for (ret = 0, thread = 0; thread < threads; thread++) {
		long available = atomic_load(&threadinfo[thread].available);
		assert(available >= 0);
		if (available >= most) {
			most = available;
			ret = thread;
		}
	}
	return most > 0 ? &threadinfo[ret] : NULL;
}

void write_thread(FILE *f, struct threadinfo *threadinfo, long *curr_fens, long fens) {
	int fd = threadinfo->fd[0];
	int16_t eval;
	move_t move = 0;
	long gen_fens = 0;
	long written_fens = 0;

	struct position pos;

	while (1) {
		if (read(fd, &move, 2) != 2) {
			fprintf(stderr, "MAIN THREAD READ ERROR\n");
			exit(1);
		}
		if (move == synchronize_threads) {
			break;
		}
		if (write_move(f, move)) {
			fprintf(stderr, "MAIN THREAD WRITE ERROR\n");
			exit(1);
		}
		if (!move) {
			if (read(fd, &pos, offsetof(struct position, accumulation)) != offsetof(struct position, accumulation)) {
				fprintf(stderr, "MAIN THREAD READ ERROR\n");
				exit(1);
			}
			if (write_position(f, &pos) || write_result(f, RESULT_UNKNOWN)) {
				fprintf(stderr, "MAIN THREAD WRITE ERROR\n");
				exit(1);
			}
		}
		if (read(fd, &eval, sizeof(eval)) != sizeof(eval)) {
			fprintf(stderr, "MAIN THREAD READ ERROR\n");
			exit(1);
		}
		if (write_eval(f, eval) || write_flag(f, 0)) {
			fprintf(stderr, "MAIN THREAD WRITE ERROR\n");
			exit(1);
		}
		gen_fens++;
		if (eval != VALUE_NONE)
			written_fens++;

		report_dot(*curr_fens + written_fens);
		report(*curr_fens + written_fens, fens);

		if (*curr_fens + written_fens >= fens) {
			atomic_store(&stop, 1);
			printf("\n");
			break;
		}
	}
	*curr_fens += written_fens;
	atomic_fetch_sub(&threadinfo->available, gen_fens);
}

void *worker(void *arg) {
	struct threadinfo *threadinfo;
	pthread_mutex_lock(&lock);
	threadinfo = arg;
	int fd = threadinfo->fd[1];
	int depth = threadinfo->depth;
	uint64_t seed = threadinfo->seed;
	int threadn = threadinfo->threadn;
	struct transpositiontable *tt = &threadinfo->tt;
	pthread_mutex_unlock(&lock);
	
	struct position pos;
	struct history h;
	startpos(&pos);
	startkey(&pos);
	history_reset(&pos, &h);
	move_t moves[MOVES_MAX];
	int *random_move = malloc(random_move_ply * sizeof(*random_move));
	random_move_flags(random_move, &seed);
	move_t move[2];
	int16_t eval;

	unsigned long gen_fens = 0;
	int drawn_score_count = 0;
	while (1) {
		move[0] = 0;
		/* Maybe randomly vary depth. */
		int depth_now = depth;
		eval = search(&pos, depth_now, 0, NULL, move, tt, &h, 0);

		/* Check for fens that we dont want to write. */
		int skip = is_capture(&pos, move) || generate_checkers(&pos, pos.turn) ||
			move_flag(move) || position_already_written(&pos) || !bernoulli(exp(-pos.halfmove / 8.0), &seed);

		int stop_game = !move[0] || (eval != VALUE_NONE && abs(eval) > eval_limit) ||
				pos.halfmove >= 100 || h.ply >= max_ply ||
				repetition(&pos, &h, 0, 2) || probable_long_draw(&h, eval, &drawn_score_count) ||
				endgame_probe(&pos);

		if (skip)
			eval = VALUE_NONE;
		else if (popcount(all_pieces(&pos)) <= 2)
			eval = 0;
#ifdef SYZYGY
		else if (syzygy && popcount(all_pieces(&pos)) <= TB_LARGEST) {
			uint64_t white = pos.piece[WHITE][ALL];
			uint64_t black = pos.piece[BLACK][ALL];
			uint64_t kings = pos.piece[WHITE][KING] | pos.piece[BLACK][KING];
			uint64_t queens = pos.piece[WHITE][QUEEN] | pos.piece[BLACK][QUEEN];
			uint64_t rooks = pos.piece[WHITE][ROOK] | pos.piece[BLACK][ROOK];
			uint64_t bishops = pos.piece[WHITE][BISHOP] | pos.piece[BLACK][BISHOP];
			uint64_t knights = pos.piece[WHITE][KNIGHT] | pos.piece[BLACK][KNIGHT];
			uint64_t pawns = pos.piece[WHITE][PAWN] | pos.piece[BLACK][PAWN];
			unsigned rule50 = 0;
			unsigned castling = 0;
			unsigned ep = 0;
			int turn = pos.turn;
			unsigned ret = tb_probe_wdl(white, black, kings, queens, rooks, bishops, knights, pawns, rule50, castling, ep, turn);
			if (ret == TB_DRAW)
				eval = 0;
#if 0
			else if (ret == TB_WIN)
				eval = VALUE_WIN;
			else if (ret == TB_LOSS)
				eval = -VALUE_WIN;
#endif
		}
#endif

		if (!stop_game && h.ply < random_move_ply && random_move[h.ply]) {
			movegen_legal(&pos, moves, MOVETYPE_ALL);
			move[0] = moves[xorshift64(&seed) % move_count(moves)];
		}

		if (stop_game) {
			eval = VALUE_NONE;
			if (h.ply >= min_ply) {
				if (!write(fd, &eval, 2)) {
					fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
					exit(1);
				}
				if (!write(fd, &synchronize_threads, 2)) {
					fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
					exit(1);
				}

				atomic_fetch_add(&threadinfo->available, gen_fens);
			}
			if (atomic_load(&stop))
				break;
			gen_fens = 0;

			startpos(&pos);
			startkey(&pos);
			history_reset(&pos, &h);
			random_move_flags(random_move, &seed);

			continue;
		}
		else {
			history_next(&pos, &h, move[0]);
		}

		if (h.ply == min_ply) {
			move[0] = 0;
			if (!write(fd, move, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
			gen_fens++;
			if (!write(fd, &pos, offsetof(struct position, accumulation))) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
		}
		if (h.ply > min_ply) {
			if (!write(fd, &eval, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
			gen_fens++;
			if (!write(fd, move, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
		}

	}
	fprintf(stderr, "Exited thread %d\n", threadn);
	return NULL;
}

int main(int argc, char **argv) {
	int threads = 1;
	int fens = 0;
	int depth = 0;
	char *path;
	static struct option opts[] = {
		{ "random-moves",        required_argument, NULL, 'm' },
		{ "random-move-ply",     required_argument, NULL, 'M' },
		{ "min-ply",             required_argument, NULL, 'n' },
		{ "max-ply",             required_argument, NULL, 'N' },
		{ "draw-ply",            required_argument, NULL, 'd' },
		{ "draw-eval",           required_argument, NULL, 'e' },
		{ "eval-limit",          required_argument, NULL, 'l' },
		{ "jobs",                required_argument, NULL, 'j' },
		{ "tt",                  required_argument, NULL, 't' },
#ifdef SYZYGY
		{ "syzygy",              required_argument, NULL, 'z' },
#endif
	};
	
	char *endptr;
	int c, option_index = 0;
	int error = 0;
	while ((c = getopt_long(argc, argv, "m:M:n:N:d:e:l:j:t:z:", opts, &option_index)) != -1) {
		switch (c) {
		case 'm':
			random_moves = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
		case 'M':
			random_move_ply = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
		case 'n':
			min_ply = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
		case 'N':
			max_ply = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
		case 'd':
			draw_ply = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
		case 'e':
			draw_eval = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
		case 'l':
			eval_limit = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
		case 'j':
			threads = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
		case 't':
			tt_MiB = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				error = 1;
			break;
#ifdef SYZYGY
		case 'z':
			syzygy = optarg;
			break;
#endif
		default:
			error = 1;
			break;
		}
	}
	if (error)
		return 1;

	if (optind + 2 >= argc ||
			(depth = strtol(argv[optind], &endptr, 10)) <= 0 || *endptr != '\0' ||
			(fens = strtol(argv[optind + 1], &endptr, 10)) <= 0 || *endptr != '\0') {
		fprintf(stderr, "usage: %s depth fens file\n", argv[0]);
		return 1;
	}
	path = argv[optind + 2];

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	tables_init();
	search_init();
	moveorder_init();
	position_init();
	transposition_init();
	endgame_init();
	history_init();

#ifdef SYZYGY
	if (syzygy) {
		if (!tb_init(syzygy)) {
			fprintf(stderr, "error: init for tablebases failed for path '%s'.\n", syzygy);
			return 1;
		}
		if (TB_LARGEST == 0) {
			fprintf(stderr, "error: no tablebases found for path '%s'.\n", syzygy);
			return 2;
		}
		printf("Tablebases found for up to %d pieces.\n", TB_LARGEST);
	}
	else {
#endif
		printf("Running without tablebases.\n");
#ifdef SYZYGY
	}
#endif

	hash_table = calloc(HASH_SIZE, sizeof(*hash_table));

	option_history       = 1;
	option_transposition = 1;
	option_nnue          = 0;
	option_endgame       = 1;
	option_damp          = 1;

	if (tt_MiB < 0)
		option_transposition = 0;

	uint64_t seed = time(NULL);

	pthread_t *thread = malloc(threads * sizeof(*thread));
	struct threadinfo *threadinfo = calloc(threads, sizeof(*threadinfo));

	pthread_mutex_init(&lock, NULL);

	for (int i = 0; i < threads; i++) {
		if (pipe(threadinfo[i].fd) == -1)
			exit(1);
		threadinfo[i].depth = depth;
		threadinfo[i].seed = seed + i;
		threadinfo[i].threadn = i;
		transposition_alloc(&threadinfo[i].tt, tt_MiB * 1024 * 1024 / threads);
		pthread_create(&thread[i], NULL, &worker, &threadinfo[i]);
	}

	start = time(NULL);

	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "error: failed to open file '%s'\n", path);
		return 3;
	}

	last_time = time_now();
	long curr_fens = 0;
	while (curr_fens < fens) {
		struct threadinfo *ti = choose_thread(threadinfo, threads);
		if (ti)
			write_thread(f, ti, &curr_fens, fens);
		else
			usleep(1000);
	}

	fclose(f);

	for (int i = 0; i < threads; i++) {
		pthread_join(thread[i], NULL);
		transposition_free(&threadinfo[i].tt);
	}

	pthread_mutex_destroy(&lock);

#ifdef SYZYGY
	if (syzygy)
		tb_free();
#endif
	free(hash_table);
}
