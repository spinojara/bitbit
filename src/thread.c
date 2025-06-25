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

#include "thread.h"

#include <string.h>

#include <pthread.h>

#include "search.h"
#include "timeman.h"

#ifndef NDEBUG
int thread_init_done = 0;
#endif

pthread_mutex_t uci;

struct threadinfo {
	struct position *pos;
	int depth;
	struct timeinfo ti;
	struct transpositiontable *tt;
	struct history *history;
};

int is_allowed(const char *arg) {
	assert(thread_init_done);
	pthread_mutex_lock(&uci);
	if (!atomic_load_explicit(&uciponder, memory_order_relaxed) && !strcmp(arg, "ponderhit")) {
		pthread_mutex_unlock(&uci);
		return 0;
	}

	if (!atomic_load_explicit(&ucigo, memory_order_relaxed)) {
		pthread_mutex_unlock(&uci);
		return 1;
	}

	if (!strcmp(arg, "stop") || !strcmp(arg, "ponderhit")) {
		pthread_mutex_unlock(&uci);
		return 1;
	}

	pthread_mutex_unlock(&uci);
	return 0;
}

void search_stop(void) {
	assert(thread_init_done);
	pthread_mutex_lock(&uci);
	if (ucigo) {
		atomic_store_explicit(&ucistop, 1, memory_order_relaxed);
		atomic_store_explicit(&uciponder, 0, memory_order_relaxed);
	}
	pthread_mutex_unlock(&uci);
}

void search_ponderhit(void) {
	assert(thread_init_done);
	pthread_mutex_lock(&uci);
	atomic_store_explicit(&uciponder, 0, memory_order_relaxed);
	pthread_mutex_unlock(&uci);
}

void *search_thread(void *arg) {
	assert(thread_init_done);
	struct threadinfo *tdi = arg;
	struct position *pos = tdi->pos;
	int depth = tdi->depth;
	struct timeinfo ti = tdi->ti;
	struct transpositiontable *tt = tdi->tt;
	struct history *history = tdi->history;
	free(arg);
	move_t move[2];
	search(pos, depth, 1, &ti, move, tt, history, 1);
	pthread_mutex_lock(&uci);
	atomic_store_explicit(&ucistop, 0, memory_order_relaxed);
	atomic_store_explicit(&ucigo, 0, memory_order_relaxed);
	atomic_store_explicit(&uciponder, 0, memory_order_relaxed);
	print_bestmove(pos, move[0], move[1]);
	pthread_mutex_unlock(&uci);
	return NULL;
}

void search_start(struct position *pos, int depth, struct timeinfo *ti, struct transpositiontable *tt, struct history *history) {
	struct threadinfo *tdi = malloc(sizeof(*tdi));
	if (!tdi) {
		fprintf(stderr, "error: failed to allocate thread info\n");
		exit(5);
	}
	tdi->pos = pos;
	tdi->depth = depth;
	tdi->ti = *ti;
	tdi->tt = tt;
	tdi->history = history;
	pthread_t thread;
	pthread_attr_t attr;

	if (pthread_attr_init(&attr) ||
			pthread_attr_setstacksize(&attr, 8 * 1024 * 1024) ||
			pthread_create(&thread, &attr, &search_thread, tdi) ||
			pthread_detach(thread)) {
		fprintf(stderr, "error: failed to create thread\n");
		exit(4);
	}
}

void thread_init(void) {
	pthread_mutex_init(&uci, NULL);

#ifndef NDEBUG
	thread_init_done = 1;
#endif
}

void thread_term(void) {
	pthread_mutex_destroy(&uci);
}
