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

/* <x * 1024 * 1024> gives a <8 * x> MiB hash table.
 * In particular x = 128 gives a 1024 MiB hash table.
 */
#define HASH_SIZE (128 * 1024 * 1024)

#define HASH_INDEX (HASH_SIZE - 1)

atomic_uint_least64_t *hash_table;

const size_t tt_GiB = 12;

const int random_move_max_ply = 25;
const int random_move_count = 7;
const int write_min_ply = 16;
const int write_max_ply = 400;
const int adj_draw_ply = 80;
const int eval_limit = 3000;

const int report_dot_every = 1000;
const int dots_per_clear = 20;
const int report_every = 200000;

time_t start = 0;

const move_t synchronize_threads = M(a1, b4, 0, 0);

struct threadinfo {
	int threadn;
	atomic_int available;
	int depth;
	uint64_t seed;
	int fd[2];

	struct transpositiontable tt;
};

pthread_mutex_t lock;

atomic_int stop = 0;

timepoint_t last_time;
uint64_t last_fens = 0;
uint64_t dot_last_fens = 0;
void report(uint64_t curr_fens, uint64_t fens) {
	if (curr_fens != last_fens && curr_fens % report_every == 0) {
		timepoint_t tp = time_now();
		time_t total = fens * (time(NULL) - start) / curr_fens;
		time_t done = start + total;
		printf("\r%ld%% %ld fens at %ld fens/second. Eta is %s", 100 * curr_fens / fens, curr_fens, tp - last_time ? 1000000 * (curr_fens - last_fens) / (tp - last_time) : 0,
				ctime(&done));
		fflush(stdout);
		last_time = tp;
		last_fens = curr_fens;
	}
}

void report_dot(uint64_t curr_fens) {
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
	if (h->ply >= adj_draw_ply && abs(eval) <= 0)
		++*drawn_score_count;
	else
		*drawn_score_count = 0;

	if (*drawn_score_count >= 8)
		return 1;

	return 0;
}

void random_move_flags(int *random_move, uint64_t *seed) {
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
	move_t move = 0;
	uint64_t gen_fens = 0;
	uint64_t written_fens = 0;

	struct partialposition pos;

	while (1) {
		if (!read(fd, &move, 2)) {
			fprintf(stderr, "MAIN THREAD READ ERROR\n");
			exit(1);
		}
		if (move == synchronize_threads) {
			break;
		}
		if (!fwrite(&move, 2, 1, f)) {
			fprintf(stderr, "MAIN THREAD WRITE ERROR\n");
			exit(1);
		}
		if (move_from(&move) == h8 && move_to(&move) == h8) {
			fprintf(stderr, "MAIN THREAD GOT BAD MOVE ERROR\n");
			exit(1);
		}
		if (!move) {
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
	int *random_move = malloc(random_move_max_ply * sizeof(*random_move));
	random_move_flags(random_move, &seed);
	move_t move;
	int16_t eval;

	unsigned int gen_fens = 0;
	int drawn_score_count = 0;
	while (1) {
		move = 0;
		/* Maybe randomly vary depth. */
		int depth_now = depth;
		eval = search(&pos, depth_now, 0, 0, 0, &move, tt, &h, 0);

		/* Check for fens that we dont want to write. */
		int skip = is_capture(&pos, &move) || generate_checkers(&pos, pos.turn) ||
			move_flag(&move) || position_already_written(&pos) || !bernoulli(exp(-pos.halfmove / 8.0), &seed);

		int stop_game = !move || (eval != VALUE_NONE && abs(eval) > eval_limit) ||
				pos.halfmove >= 100 || h.ply >= write_max_ply ||
				is_repetition(&pos, &h, 0, 2) || probable_long_draw(&h, eval, &drawn_score_count) ||
				endgame_probe(&pos);

		if (skip)
			eval = VALUE_NONE;
		else if (popcount(all_pieces(&pos)) <= 2)
			eval = 0;
#ifdef SYZYGY
		else if (popcount(all_pieces(&pos)) <= TB_LARGEST) {
			uint64_t _white = pos.piece[white][all];
			uint64_t _black = pos.piece[black][all];
			uint64_t _kings = pos.piece[white][king] | pos.piece[black][king];
			uint64_t _queens = pos.piece[white][queen] | pos.piece[black][queen];
			uint64_t _rooks = pos.piece[white][rook] | pos.piece[black][rook];
			uint64_t _bishops = pos.piece[white][bishop] | pos.piece[black][bishop];
			uint64_t _knights = pos.piece[white][knight] | pos.piece[black][knight];
			uint64_t _pawns = pos.piece[white][pawn] | pos.piece[black][pawn];
			unsigned _rule50 = 0;
			unsigned _castling = 0;
			unsigned _ep = 0;
			int _turn = pos.turn;
			unsigned ret = tb_probe_wdl(_white, _black, _kings, _queens, _rooks, _bishops, _knights, _pawns, _rule50, _castling, _ep, _turn);
			if (ret == TB_DRAW) {
				eval = 0;
			}
			else if (ret == TB_WIN)
				eval = VALUE_WIN;
			else if (ret == TB_LOSS)
				eval = -VALUE_WIN;
		}
#endif

		if (!stop_game && h.ply < random_move_max_ply && random_move[h.ply]) {
			movegen_legal(&pos, moves, MOVETYPE_ALL);
			move = moves[xorshift64(&seed) % move_count(moves)];
		}

		if (stop_game) {
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

			continue;
		}
		else {
			history_next(&pos, &h, move);
		}

		if (h.ply == write_min_ply) {
			move = 0;
			if (!write(fd, &move, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
			gen_fens++;
			if (!write(fd, &pos, sizeof(struct partialposition))) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
		}
		if (h.ply > write_min_ply) {
			if (!write(fd, &eval, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
			gen_fens++;
			if (!write(fd, &move, 2)) {
				fprintf(stderr, "WRITE ERROR ON THREAD %d\n", threadn);
				exit(1);
			}
		}

	}
	fprintf(stderr, "EXITED THREAD %d\n", threadn);
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
	endgame_init();

#ifdef SYZYGY
	if (!tb_init(XSTR(SYZYGY))) {
		printf("Init for tablebase failed for path \"%s\".\n", XSTR(SYZYGY));
		exit(1);
	}
	printf("Tablebases found for up to %d pieces.\n", TB_LARGEST);
#endif

	hash_table = calloc(HASH_SIZE, sizeof(*hash_table));

	option_history       = 1;
	option_transposition = 1;
	option_nnue          = 0;
	option_endgame       = 1;
	option_damp          = 1;

	int n_threads = 12;
	int depth = 5;
	uint64_t fens = 500000;

	uint64_t seed = time(NULL);

	pthread_t *thread = malloc(n_threads * sizeof(*thread));
	struct threadinfo *threadinfo = calloc(n_threads, sizeof(*threadinfo));

	pthread_mutex_init(&lock, NULL);

	for (int i = 0; i < n_threads; i++) {
		if (pipe(threadinfo[i].fd) == -1)
			exit(1);
		threadinfo[i].depth = depth;
		threadinfo[i].seed = seed + i;
		threadinfo[i].threadn = i;
		transposition_alloc(&threadinfo[i].tt, tt_GiB * 1024 * 1024 * 1024 / n_threads);
		pthread_create(&thread[i], NULL, worker, &threadinfo[i]);
	}

	start = time(NULL);

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

	for (int i = 0; i < n_threads; i++) {
		pthread_join(thread[i], NULL);
		transposition_free(&threadinfo[i].tt);
	}

	pthread_mutex_destroy(&lock);

#ifdef SYZYGY
	tb_free();
#endif
	free(hash_table);
}
