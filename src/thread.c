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

pthread_mutex_t uci;

struct threadinfo {
	struct position *pos;
	int depth;
	struct timeinfo ti;
	struct transpositiontable *tt;
	struct history *history;
};

int is_allowed(const char *arg) {
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
	pthread_mutex_lock(&uci);
	if (ucigo) {
		ucistop = 1;
		uciponder = 0;
	}
	pthread_mutex_unlock(&uci);
}

void search_ponderhit(void) {
	pthread_mutex_lock(&uci);
	uciponder = 0;
	pthread_mutex_unlock(&uci);
}

void *search_thread(void *arg) {
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
	ucistop = 0;
	ucigo = 0;
	uciponder = 0;
	print_bestmove(pos, move[0], move[1]);
	pthread_mutex_unlock(&uci);
	return NULL;
}

void search_start(struct position *pos, int depth, struct timeinfo *ti, struct transpositiontable *tt, struct history *history) {
	struct threadinfo *tdi = malloc(sizeof(*tdi));
	tdi->pos = pos;
	tdi->depth = depth;
	tdi->ti = *ti;
	tdi->tt = tt;
	tdi->history = history;
	pthread_t thread;
	if (pthread_create(&thread, NULL, &search_thread, tdi) || pthread_detach(thread)) {
		fprintf(stderr, "error: failed to create thread\n");
		exit(4);
	}
}

void thread_init(void) {
	pthread_mutex_init(&uci, NULL);
}

void thread_term(void) {
	pthread_mutex_destroy(&uci);
}
