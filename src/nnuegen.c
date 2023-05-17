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

#define FEN_CHUNKS (1000)

struct threadinfo {
	atomic_int available;
	int fd[2];
};

pthread_mutex_t lock;

int stop = 0;

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
	return &threadinfo[ret];
}
void write_thread(FILE *f, struct threadinfo *threadinfo, uint64_t *curr_fens, uint64_t fens) {
	int fd = threadinfo->fd[0];
	int16_t value = 1;
	uint64_t gen_fens = 0;
	while (value || gen_fens < FEN_CHUNKS) {
		if (read(fd, &value, sizeof(value)) == -1)
			printf("READ ERROR\n");
		write_le_uint(f, value, 2);
		if (read(fd, &value, sizeof(value)) == -1)
			printf("READ ERROR\n");
		gen_fens++;
		if (*curr_fens + gen_fens < fens) {
			write_le_uint(f, value, 2);
		}
		else {
			write_le_uint(f, 0, 2);
			pthread_mutex_lock(&lock);
			stop = 1;
			pthread_mutex_unlock(&lock);
			break;
		}
		if (fens / 100 && (*curr_fens + gen_fens) % (fens / 100) == 0 && 100 * (*curr_fens + gen_fens) / fens) {
			printf("%ld%%\n", 100 * (*curr_fens + gen_fens) / fens);
			fflush(stdout);
		}
	}
	*curr_fens += gen_fens;
	atomic_fetch_sub(&threadinfo->available, gen_fens);
}

void *worker(void *arg) {
	struct threadinfo *threadinfo;
	pthread_mutex_lock(&lock);
	threadinfo = arg;
	int fd = threadinfo->fd[1];
	pthread_mutex_unlock(&lock);
	
	struct position pos[1];
	startpos(pos);
	move move_list[MOVES_MAX];
	move m;
	int16_t eval;

	unsigned int count = 0;
	while (1) {
		count++;
		m = 0;
		eval = search(pos, 3, 0, 0, 0, &m, NULL, 0);
		eval = CLAMP(eval, -20000, 20000);
		if (write(fd, &eval, sizeof(eval)) == -1)
			printf("WRITE ERROR\n");

		/* CHECK FOR TWOFOLD */
		if (!m || pos->fullmove >= 50 || eval == 20000 || eval == -20000) {
			m = 0;
			if (write(fd, &m, sizeof(eval)) == -1)
				printf("WRITE ERROR\n");
			startpos(pos);
		}
		else if (rand() % 5 == 0) {
			generate_all(pos, move_list);
			m = move_list[rand() % move_count(move_list)];
			if (write(fd, &m, sizeof(eval)) == -1)
				printf("WRITE ERROR\n");
			do_move(pos, &m);
		}
		else {
			if (write(fd, &m, sizeof(eval)) == -1)
				printf("WRITE ERROR\n");
			do_move(pos, &m);
		}
		if (count >= FEN_CHUNKS) {
			pthread_mutex_lock(&lock);
			int stop_t = stop;
			pthread_mutex_unlock(&lock);
			atomic_fetch_add(&threadinfo->available, count);
			count = 0;
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
	pawn_init();

	option_transposition = 0;
	option_nnue = 0;

	srand(time(NULL));

	int n_threads = 10;

	pthread_t thread[n_threads];
	struct threadinfo threadinfo[n_threads];

	memset(threadinfo, 0, sizeof(threadinfo));

	pthread_mutex_init(&lock, NULL);

	for (int i = 0; i < n_threads; i++) {
		if (pipe(threadinfo[i].fd) == -1)
			exit(1);
		pthread_create(&thread[i], NULL, worker, &threadinfo[i]);
	}

	struct timeval start, end;
	gettimeofday(&start, NULL);

	uint64_t fens = 1000000;

	FILE *f = fopen("nnue.bin", "wb");

	uint64_t curr_fens = 0;
	while (curr_fens < fens)
		write_thread(f, choose_thread(threadinfo, n_threads), &curr_fens, fens);

	fclose(f);

	gettimeofday(&end, NULL);
	long uelapsed = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
	printf("time: %ld\n", uelapsed / 1000000);
	if (uelapsed)
		printf("fens per second: %ld\n", fens * 1000000 / uelapsed);

	for (int i = 0; i < n_threads; i++)
		pthread_join(thread[i], NULL);

	pthread_mutex_destroy(&lock);

	pawn_term();
}
