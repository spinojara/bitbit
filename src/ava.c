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
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include "util.h"
#include "magic_bitboard.h"
#include "attack_gen.h"
#include "bitboard.h"
#include "position.h"
#include "transposition_table.h"
#include "move_gen.h"
#include "interface.h"
#include "evaluate.h"

pthread_mutex_t lock;

struct engine {
	FILE *in, *out;
	pid_t pid;
	char name[BUFSIZ];
};

struct parseinfo {
	char engine[2][BUFSIZ];
	char option[2][16][BUFSIZ];

	int wins[3];

	int wtime;
	int btime;
	int winc;
	int binc;
	int movetime;

	int games;
	int played;
	int playing;
	int white;
	int threads;
};

long timems(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (((long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

int play_game(struct engine *engine[2], int engine_white, struct parseinfo *info) {
	struct position *pos = malloc(sizeof(struct position));
	struct history *history = NULL;
	
	char *start_fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
	char fen[512], buf[BUFSIZ], move_str[16];
	move m;
	pos_from_fen(pos, SIZE(start_fen), start_fen);
	

	long t[3];
	t[white] = info->wtime;
	t[black] = info->btime;

	for (int i = 0; i < 2; i++) {
		fprintf(engine[i]->in, "ucinewgame\n");
		fprintf(engine[i]->in, "isready\n");
	}

	for (int i = 0; i < 2; i++) {
		while (1) {
			if (!fgets(buf, sizeof(buf), engine[i]->out))
				printf("fgets ERROR\n");
			if (strstr(buf, "readyok"))
				break;
		}
	}

	int turn = engine_white;
	int ret = 0;

	while (1) {
		pos_to_fen(fen, pos);
		fprintf(engine[turn]->in, "position fen %s\n", fen);
		//printf("position fen %s\n", fen);
		fprintf(engine[turn]->in, "go wtime %ld btime %ld winc %d binc %d\n", t[white], t[black], info->winc, info->binc);
		//printf("go wtime %ld btime %ld winc %d binc %d\n", t[white], t[black], info->winc, info->binc);
		
		t[3] = timems();
		while (1) {
			if (!fgets(buf, sizeof(buf), engine[turn]->out))
				printf("fgets ERROR\n");
			//printf("%s", buf);
			if (strstr(buf, "bestmove ")) {
				snprintf(move_str, SIZE(move_str), buf + strlen("bestmove "));
				for (unsigned long i = 0; i < SIZE(move_str) - 1; i++)
					if (move_str[i] == ' ')
						move_str[i + 1] = '\0';
				break;
			}
		}
		t[turn] -= timems() - t[3];

		if (t[turn] <= 0) {
			ret = 1 - turn;
			break;
		}
		/* increment 1000 ms */
		t[turn] += (turn == engine_white) ? info->winc : info->binc;

		/* remove \n character */
		move_str[strlen(move_str) - 1] = '\0';
		m = string_to_move(pos, move_str);
		move_next(&pos, &history, m);

		if (mate(pos) == 2) {
			/* 1 - turn won */
			ret = turn;
			break;
		}
		if (mate(pos) == 1 || pos->halfmove >= 100 || is_threefold(pos, history)) {
			ret = 2;
			break;
		}

		turn = 1 - turn;
	}

	/* fix early returns */
	free(pos);
	return ret;
}

struct engine *eopen(struct parseinfo *info, int n) {
	pid_t pid = 0;

	int ipipe[2];
	int opipe[2];
	if (pipe(ipipe))
		return NULL;
	if (pipe(opipe))
		return NULL;

	struct engine *engine = malloc(sizeof(struct engine));
	pid = fork();
	if (pid == 0) {
		close(ipipe[1]);
		dup2(ipipe[0], STDIN_FILENO);
		close(opipe[0]);
		dup2(opipe[1], STDOUT_FILENO);

		execl(info->engine[n], info->engine[n], (char *)NULL);
		printf("failed to open engine: %s\n", info->engine[n]);
		kill(getppid(), SIGINT);
		exit(1);
	}

	engine->pid = pid;

	engine->out = fdopen(opipe[0], "r");
	engine->in = fdopen(ipipe[1], "w");
	setbuf(engine->out, NULL);
	setbuf(engine->in, NULL);

	char buf[BUFSIZ];

	fprintf(engine->in, "uci\n");

	while (1) {
		if (!fgets(buf, sizeof(buf), engine->out))
			printf("fgets ERROR\n");
		//printf("%s", buf);
		if (strstr(buf, "uciok"))
			break;
		if (strstr(buf, "id name "))
			snprintf(engine->name, sizeof(engine->name), buf + strlen("id name "));
	}
	engine->name[sizeof(engine->name) - 1] = '\0';

	for (int i = 0; i < 16; i++)
		fprintf(engine->in, "%s\n", info->option[n][i]);

	return engine;
}

int eclose(struct engine *engine) {
	if (!engine)
		return 1;
	fclose(engine->in);
	fclose(engine->out);
	kill(engine->pid, SIGTERM);
	free(engine);
	return 0;
}

void print_eta(struct parseinfo *info) {
	time_t rawcurrent, raweta;
	struct tm current, eta, *t;
	raweta = rawcurrent = time(NULL);
	raweta += (info->games - info->played) * MAX(info->wtime + info->btime + 40 * (info->winc + info->binc), 2 * 40 * info->movetime) / (1000 * info->threads);
	t = localtime(&rawcurrent);
	memcpy(&current, t, sizeof(*t));
	t = localtime(&raweta);
	memcpy(&eta, t, sizeof(*t));
	printf("%d/%d time: %02d:%02d, eta: %02d:%02d\n", info->played + info->playing,
			info->games, current.tm_hour, current.tm_min,
			eta.tm_hour, eta.tm_min);
}

void *thread_play(void *ptr) {
	struct parseinfo *info = (struct parseinfo *)ptr;
	struct engine *engine[2];

	engine[0] = eopen(info, 0);
	engine[1] = eopen(info, 1);

	int ret;
	while (1) {
		pthread_mutex_lock(&lock);
		if (info->played + info->playing < info->games) {
			info->playing++;
			ret = info->white;
			info->white = 1 - info->white;
			print_eta(info);
			pthread_mutex_unlock(&lock);

			ret = play_game(engine, white, info);

			pthread_mutex_lock(&lock);
			info->playing--;
			info->played++;
			info->wins[ret]++;
			pthread_mutex_unlock(&lock);
		}
		else {
			pthread_mutex_unlock(&lock);
			break;
		}
	}

	eclose(engine[0]);
	eclose(engine[1]);

	return NULL;
}

void parseinfo(struct parseinfo *info, int argc, char **argv) {
	memset(info, 0, sizeof(struct parseinfo));
	info->games = 1;
	info->threads = 1;
	info->wtime = info->btime = -1;
	info->winc = info->binc = 0;

	int engine_n = -1;
	int option_n = -1;
	void *ptr = NULL;

	int as_str = 1;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "engine")) {
			as_str = 1;
			engine_n++;
			option_n = -1;
			ptr = info->engine[engine_n];
		}
		else if (!strcmp(argv[i], "option")) {
			as_str = 1;
			option_n++;
			ptr = info->option[engine_n][option_n];
		}
		else if (!strcmp(argv[i], "games")) {
			as_str = 0;
			ptr = &info->games;
		}
		else if (!strcmp(argv[i], "threads")) {
			as_str = 0;
			ptr = &info->threads;
		}
		else if (!strcmp(argv[i], "wtime")) {
			as_str = 0;
			ptr = &info->wtime;
		}
		else if (!strcmp(argv[i], "btime")) {
			as_str = 0;
			ptr = &info->btime;
		}
		else if (!strcmp(argv[i], "winc")) {
			as_str = 0;
			ptr = &info->winc;
		}
		else if (!strcmp(argv[i], "binc")) {
			as_str = 0;
			ptr = &info->binc;
		}
		else if (!strcmp(argv[i], "movetime")) {
			as_str = 0;
			ptr = &info->movetime;
		}
		else if (as_str && ptr) {
			if (*(char *)ptr)
				appendstr(ptr, " ");
			appendstr(ptr, argv[i]);
		}
		else if (!as_str && ptr) {
			*(int *)ptr = strint(argv[i]);
			ptr = NULL;
		}
	}

	/* if not set */
	if (info->wtime == -1 && info->btime == -1)
		info->winc = info->binc = 1000;
	if (info->wtime == -1)
		info->wtime = 120000;
	if (info->btime == -1)
		info->btime = 120000;

	info->threads = MAX(1, info->threads);
	info->games = MAX(1, info->games);
	info->wtime = MAX(0, info->wtime);
	info->btime = MAX(0, info->btime);
	info->winc = MAX(0, info->winc);
	info->binc = MAX(0, info->binc);
	info->movetime = MAX(0, info->movetime);

	for (int i = 0; i < 2; i++) {
		printf("%s\n", info->engine[i]);
		for (int j = 0; j < 16; j++)
			if (info->option[i][j][0])
				printf("%s\n", info->option[i][j]);
	}
	printf("games: %i\n", info->games);
	printf("threads: %i\n", info->threads);
	if (info->wtime)
		printf("wtime: %i\n", info->wtime);
	if (info->btime)
		printf("btime: %i\n", info->btime);
	if (info->winc)
		printf("winc: %i\n", info->winc);
	if (info->binc)
		printf("binc: %i\n", info->binc);
	if (info->movetime)
		printf("movetime: %i\n", info->movetime);
}

int main(int argc, char **argv) {
	magic_bitboard_init();
	attack_gen_init();
	bitboard_init();
	transposition_table_init();

	struct parseinfo info;
	parseinfo(&info, argc, argv);

	pthread_t *thread = malloc(info.threads * sizeof(pthread_t));
	int *ret = malloc(info.threads * sizeof(int));

	pthread_mutex_init(&lock, NULL);

	setbuf(stdin, NULL);
	setbuf(stdout, NULL);

	for (int i = 0; i < info.threads; i++)
		ret[i] = pthread_create(thread + i, NULL, thread_play, (void *)&info);

	for (int i = 0; i < info.threads; i++)
		pthread_join(thread[i], NULL);

	pthread_mutex_destroy(&lock);

	printf("\n%s: %d\n%s: %d\nstalemates: %d\n", info.engine[0], info.wins[0], info.engine[1], info.wins[1], info.wins[2]);

	transposition_table_term();
	return 0;
}
