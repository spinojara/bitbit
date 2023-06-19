/* bitbit, a bitboard based chess engine written in c.  * Copyright (C) 2022 Isak Ellmer
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

#define _DEFAULT_SOURCE /* usleep */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

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
#include "pawn.h"
#include "option.h"
#include "transposition.h"
#include "history.h"
#include "timeman.h"

/* <x * 1024 * 1024> gives a <8 * x> hash table MiB */
#define HASH_SIZE (128 * 1024 * 1024)

#define HASH_INDEX (HASH_SIZE - 1)

atomic_uint_least64_t *hash_table;

const int random_move_max_ply = 25;
const int random_move_count = 7;
const int write_min_ply = 16;
const int write_max_ply = 400;
const int adj_draw_ply = 80;
const int eval_limit = 3000;

const int report_dot_every = 10000;
const int report_every = 200000;

const move synchronize_threads = M(a1, b4, 0, 0);

struct threadinfo {
	int threadn;
	atomic_int available;
	int depth;
	uint64_t seed;
	int fd[2];
};

pthread_mutex_t lock;

atomic_int stop = 0;

time_point last_time;
uint64_t last_fens = 0;
uint64_t dot_last_fens = 0;
void report(uint64_t curr_fens, uint64_t fens) {
	if (curr_fens != last_fens && curr_fens % report_every == 0) {
		time_point tp = time_now();
		printf("\r%ld%% %ld fens at %ld fens/second\n", 100 * curr_fens / fens, curr_fens, tp - last_time ? 1000000 * (curr_fens - last_fens) / (tp - last_time) : 0);
		fflush(stdout);
		last_time = tp;
		last_fens = curr_fens;
	}
}

void report_dot(uint64_t curr_fens) {
	if (curr_fens != dot_last_fens && curr_fens % report_dot_every == 0) {
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

int probable_long_draw(struct position *pos, struct history *h, int16_t eval, int *drawn_score_count) {
	if (h->ply >= adj_draw_ply && ABS(eval) <= 0)
		++*drawn_score_count;
	else
		*drawn_score_count = 0;

	if (*drawn_score_count >= 8)
		return 1;

	if (pos->piece[white][rook] || pos->piece[black][rook] ||
			pos->piece[white][queen] || pos->piece[black][queen] ||
			pos->piece[white][pawn] || pos->piece[black][pawn])
		return 0;

	int total_pieces = popcount(pos->piece[white][all] | pos->piece[black][all]);
	if (total_pieces <= 4)
		return 1;

	return 0;
}

void random_move_flags(int random_move[random_move_max_ply], uint64_t *seed) {
	for (int i = 0; i < random_move_max_ply; i++)
		random_move[i] = (i < random_move_count);

	for (int i = random_move_max_ply - 1; i > 0; i--) {
		int j = xorshift64(seed) % (i + 1);
		int t = random_move[i];
		random_move[i] = random_move[j];
		random_move[j] = t;
	}
}

struct threadinfo *choose_thread(struct threadinfo *threadinfo, int n_threads) {
	int thread;
	int ret;
	int64_t most = -1;
	for (ret = 0, thread = 0; thread < n_threads; thread++) {
		int available = atomic_load(&threadinfo[thread].available);
		if (available < 0)
			printf("%d\n", available);
		assert(available >= 0);
		if (available >= most) {
			most = available;
			ret = thread;
		}
	}
	return most > 0 ? &threadinfo[ret] : NULL;
}

void write_thread(FILE *f, struct threadinfo *threadinfo, uint64_t *curr_fens, uint64_t fens) {
	int fd = threadinfo->fd[0];
	int16_t eval;
	move m = 0;
	uint64_t gen_fens = 0;
	uint64_t written_fens = 0;

	struct partialposition pos;

	while (1) {
		if (!read(fd, &m, 2)) {
			fprintf(stderr, "MAIN THREAD READ ERROR\n");
			exit(1);
		}
		if (m == synchronize_threads) {
			break;
		}
		if (!fwrite(&m, 2, 1, f)) {
			fprintf(stderr, "MAIN THREAD WRITE ERROR\n");
			exit(1);
		}
		if (move_from(&m) == h8 && move_to(&m) == h8) {
			fprintf(stderr, "MAIN THREAD GOT BAD MOVE ERROR\n");
			exit(1);
		}
		if (!m) {
			if (!read(fd, &pos, sizeof(pos))) {
				fprintf(stderr, "MAIN THREAD READ ERROR\n");
				exit(1);
			}
			if (!fwrite(&pos, sizeof(pos), 1, f)) {
				fprintf(stderr, "MAIN THREAD WRITE ERROR\n");
				exit(1);
			}
		}
		if (!read(fd, &eval, sizeof(eval))) {
			fprintf(stderr, "MAIN THREAD READ ERROR\n");
			exit(1);
		}
		if (!fwrite(&eval, 2, 1, f)) {
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
			break;
		}
	}
	*curr_fens += written_fens;
	atomic_fetch_sub(&threadinfo->available, gen_fens);
}

void logstring(FILE *f, char *s) {
	time_t ct;
	struct tm lt;
	ct = time(NULL);
	localtime_r(&ct, &lt);
	char t[128];
	strftime(t, sizeof(t), "%H:%M:%S", &lt);
	fprintf(f, "%s %s", t, s);
}

void *worker(void *arg) {
	struct threadinfo *threadinfo;
	pthread_mutex_lock(&lock);
	threadinfo = arg;
	int fd = threadinfo->fd[1];
	int depth = threadinfo->depth;
	uint64_t seed = threadinfo->seed;
	int threadn = threadinfo->threadn;
	pthread_mutex_unlock(&lock);
	char filename[128];
	sprintf(filename, "nnuegen.%d.log", threadn);
	FILE *f = fopen(filename, "w");
	
	struct position pos;
	struct history h;
	startpos(&pos);
	startkey(&pos);
	history_reset(&pos, &h);
	move move_list[MOVES_MAX];
	int random_move[random_move_max_ply];
	random_move_flags(random_move, &seed);
	move m;
	int16_t eval;

	unsigned int gen_fens = 0;
	int drawn_score_count = 0;
	while (1) {
		m = 0;
		/* maybe randomly vary depth */
		int depth_now = depth;
		logstring(f, "search\n");
		eval = search(&pos, depth_now, 0, 0, 0, &m, &h, 0);
		logstring(f, "done\n");

		/* check for fens that we dont want to write */
		int skip = is_capture(&pos, &m) || generate_checkers(&pos, pos.turn) ||
			move_flag(&m) || position_already_written(&pos);

		if (skip)
			eval = VALUE_NONE;

		logstring(f, "stop_game\n");
		int stop_game = !m || (eval != VALUE_NONE && ABS(eval) > eval_limit) ||
				pos.halfmove >= 100 || h.ply >= write_max_ply ||
				is_repetition(&pos, &h, 0, 2) || probable_long_draw(&pos, &h, eval, &drawn_score_count);
		logstring(f, "done\n");

		logstring(f, "cmove\n");
		if (!stop_game && h.ply < random_move_max_ply && random_move[h.ply]) {
			generate_all(&pos, move_list);
			m = move_list[xorshift64(&seed) % move_count(move_list)];
		}
		logstring(f, "done\n");

		if (stop_game) {
			logstring(f, "reset\n");
			eval = VALUE_NONE;
			if (h.ply >= write_min_ply) {
				if (!write(fd, &eval, 2)) {
					fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
					exit(1);
				}
				if (!write(fd, &synchronize_threads, 2)) {
					fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
					exit(1);
				}

				atomic_fetch_add(&threadinfo->available, gen_fens);
				if (atomic_load(&stop)) {
					fprintf(stderr, "EXITED THREAD %d\n", threadn);
					break;
				}
			}
			gen_fens = 0;

			startpos(&pos);
			startkey(&pos);
			history_reset(&pos, &h);
			random_move_flags(random_move, &seed);

			logstring(f, "done\n");
			continue;
		}
		else {
			logstring(f, "move\n");
			history_next(&pos, &h, m);
			logstring(f, "done\n");
		}

		if (h.ply == write_min_ply) {
			logstring(f, "wpos\n");
			m = 0;
			if (!write(fd, &m, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
			gen_fens++;
			if (!write(fd, &pos, sizeof(struct partialposition))) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
			logstring(f, "done\n");
		}
		if (h.ply > write_min_ply) {
			logstring(f, "wmove\n");
			if (!write(fd, &eval, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
			gen_fens++;
			if (!write(fd, &m, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
			logstring(f, "done\n");
		}

	}
	fclose(f);
	fprintf(stderr, "EXITED THREAD %d\n", threadn);
	exit(1);
	return NULL;
}

int main(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	tables_init();
	search_init();
	moveorder_init();
	position_init();
	transposition_init();
	allocate_transposition_table(33);
	pawn_init();

	hash_table = malloc(HASH_SIZE * sizeof(*hash_table));
	memset(hash_table, 0, HASH_SIZE * sizeof(*hash_table));

	option_history = 1;
	option_pawn = 0;
	option_transposition = 1;
	option_nnue = 0;

	int n_threads = 12;
	int depth = 5;
	uint64_t fens = 500000000;

	uint64_t seed = time(NULL);

	pthread_t thread[n_threads];
	struct threadinfo threadinfo[n_threads];

	memset(threadinfo, 0, sizeof(threadinfo));

	pthread_mutex_init(&lock, NULL);

	for (int i = 0; i < n_threads; i++) {
		if (pipe(threadinfo[i].fd) == -1)
			exit(1);
		threadinfo[i].depth = depth;
		threadinfo[i].seed = seed + i;
		threadinfo[i].threadn = i;
		pthread_create(&thread[i], NULL, worker, &threadinfo[i]);
	}

	FILE *f = fopen("nnue.bin", "wb");

	last_time = time_now();
	uint64_t curr_fens = 0;
	while (curr_fens < fens) {
		struct threadinfo *ti = choose_thread(threadinfo, n_threads);
		if (ti)
			write_thread(f, ti, &curr_fens, fens);
		else
			usleep(1000);
	}

	fclose(f);

	for (int i = 0; i < n_threads; i++)
		pthread_join(thread[i], NULL);

	pthread_mutex_destroy(&lock);

	pawn_term();
	free(hash_table);
}
