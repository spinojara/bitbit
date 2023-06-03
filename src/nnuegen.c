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

#define FEN_CHUNKS (512)

/* <x * 1024 * 1024> gives a <8 * x> hash table MiB */
#define HASH_SIZE (128 * 1024 * 1024)

#define HASH_INDEX (HASH_SIZE - 1)

atomic_uint_least64_t *hash_table;

const int random_move_max_ply = 25;
const int write_min_ply = 16;
const int write_max_ply = 400;
const int eval_limit = 3000;

const int report_dot_every = 10000;
const int report_every = 200000;

const move synchronize_threads = M(a1, b4, 0, 0);

struct threadinfo {
	atomic_int available;
	int depth;
	uint64_t seed;
	int fd[2];
};

pthread_mutex_t lock;

int stop = 0;

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

struct threadinfo *choose_thread(struct threadinfo *threadinfo, int n_threads) {
	int thread;
	int ret;
	int64_t most = -(FEN_CHUNKS + 1);
	for (ret = 0, thread = 0; thread < n_threads; thread++) {
		int available = atomic_load(&threadinfo[thread].available);
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
		read(fd, &m, 2);
		if (m == synchronize_threads) {
			if (gen_fens >= FEN_CHUNKS)
				break;
			read(fd, &m, 2);
		}

		fwrite(&m, 2, 1, f);
		if (!m) {
			read(fd, &pos, sizeof(pos));
			fwrite(&pos, sizeof(pos), 1, f);
		}
		read(fd, &eval, sizeof(eval));
		fwrite(&eval, 2, 1, f);
		gen_fens++;
		if (eval != VALUE_NONE)
			written_fens++;

		report_dot(*curr_fens + written_fens);
		report(*curr_fens + written_fens, fens);

		if (*curr_fens + written_fens >= fens) {
			pthread_mutex_lock(&lock);
			stop = 1;
			pthread_mutex_unlock(&lock);
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
	pthread_mutex_unlock(&lock);
	
	struct position pos;
	struct history h;
	startpos(&pos);
	startkey(&pos);
	history_reset(&pos, &h);
	move move_list[MOVES_MAX];
	move m;
	int16_t eval;

	unsigned int gen_fens = 0;
	while (1) {
		gen_fens++;
		m = 0;
		/* maybe randomly vary depth */
		int depth_now = depth;
		eval = search(&pos, depth_now, 0, 0, 0, &m, &h, 0);

		/* check for fens that we dont want to write */
		if (is_capture(&pos, &m) ||
				generate_checkers(&pos, pos.turn) ||
				move_flag(&m) == 2 ||
				position_already_written(&pos))
			eval = VALUE_NONE;

		int stop_game = !m || (eval != VALUE_NONE && ABS(eval) > eval_limit) ||
				pos.halfmove >= 25 || h.ply >= write_max_ply ||
				is_repetition(&pos, &h, 0, 1);

		if (h.ply <= random_move_max_ply && !stop_game && xorshift64(&seed) % 3 == 0) {
			generate_all(&pos, move_list);
			m = move_list[xorshift64(&seed) % move_count(move_list)];
		}

		if (stop_game) {
			eval = VALUE_NONE;
			startpos(&pos);
			startkey(&pos);
			history_reset(&pos, &h);
			write(fd, &eval, 2);
			write(fd, &synchronize_threads, 2);
		}
		else {
			history_next(&pos, &h, m);
		}


		if (h.ply == write_min_ply && !stop_game) {
			m = 0;
			write(fd, &m, 2);
			write(fd, &pos, sizeof(struct partialposition));
		}
		if (h.ply > write_min_ply && !stop_game) {
			write(fd, &eval, 2);
			write(fd, &m, 2);
		}

		if (stop_game && gen_fens >= FEN_CHUNKS) {
			pthread_mutex_lock(&lock);
			int stop_t = stop;
			pthread_mutex_unlock(&lock);
			atomic_fetch_add(&threadinfo->available, gen_fens);
			gen_fens = 0;
			if (stop_t)
				break;
		}
	}
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
	pawn_init();

	hash_table = malloc(HASH_SIZE * sizeof(*hash_table));
	memset(hash_table, 0, HASH_SIZE * sizeof(*hash_table));

	option_history = 1;
	option_pawn = 0;
	option_transposition = 0;
	option_nnue = 0;

	int n_threads = 12;
	int depth = 3;
	uint64_t fens = 1000000;

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
