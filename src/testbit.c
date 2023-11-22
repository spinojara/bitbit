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

#include <signal.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

enum {
	LOSS,
	DRAW,
	WIN,
};

struct result {
	unsigned result;
	unsigned done;
};

unsigned long game_number(const char *str) {
	unsigned long r = 0;

	for (const char *c = str; *c != ' '; c++) {
		r = 10 * r + *c - '0';
	}
		
	return r - 1;
}

void update_pentanomial(unsigned long pentanomial[5], struct result *results, unsigned long game) {
	int first;

	/* The pairs are
	 * 0, 1
	 * 2, 3
	 * 4, 5
	 * etc.
	 */
	if (game % 2)
		first = game - 1;
	else
		first = game + 1;

	if (!results[first].done)
		return;

	unsigned a = results[game].result;
	unsigned b = results[first].result;

	pentanomial[a + b]++;
}

void start_task(unsigned long games) {
	games = 400;
	char gamesstr[1024];
	sprintf(gamesstr, "%lu", games);

	int pipefd[2];
	if (pipe(pipefd))
		exit(1);

	pid_t pid = fork();
	if (pid == -1)
		exit(2);

	if (pid == 0) {
		close(pipefd[0]); /* Close unsued read end. */
		dup2(pipefd[1], STDOUT_FILENO); /* Redirect stdout to write end. */

		execlp("cutechess-cli", "cutechess-cli", "-each", "proto=uci", "tc=1+0.03",
				"-games", gamesstr,
				"-concurrency", "8",
				"-repeat",
				"-engine", "cmd=/usr/src/bitbit/bitbit",
				"-engine", "cmd=/usr/src/bitbit/bitbit", NULL);

		perror("error: failed to open cutechess-cli\n");
		kill(getppid(), SIGINT);
		exit(3);
	}

	struct result *results = calloc(games, sizeof(*results));
	unsigned long trinomial[3] = { 0 };
	unsigned long pentanomial[5] = { 0 };

	close(pipefd[1]); /* Close unused write end. */
	FILE *f = fdopen(pipefd[0], "r");
	char line[BUFSIZ];

	while (1) {
		fgets(line, sizeof(line), f);
		if (!strncmp(line, "Finished game", 13)) {
			unsigned long game = game_number(line + 14);
			/* Fixes the problem while parsing that some games
			 * are played as white and some as black.
			 */
			int white = !(game % 2);
			results[game].done = 1;
			if (strstr(line, "1-0"))
				results[game].result = white ? WIN : LOSS;
			else if (strstr(line, "0-1"))
				results[game].result = white ? LOSS : WIN;
			else
				results[game].result = DRAW;

			trinomial[results[game].result]++;
			update_pentanomial(pentanomial, results, game);
		}
		else if (!strncmp(line, "Finished match", 14)) {
			break;
		}
		printf("%s", line);
	}

	printf("TRINOMIAL %ld - %ld - %ld\n", trinomial[WIN], trinomial[LOSS], trinomial[DRAW]);
	printf("PENTANOMIAL %ld - %ld - %ld - %ld - %ld\n", pentanomial[0], pentanomial[1], pentanomial[2], pentanomial[3], pentanomial[4]);

	unsigned long tN = trinomial[0] + trinomial[1] + trinomial[2];
	unsigned long pN = pentanomial[0] + pentanomial[1] + pentanomial[2] + pentanomial[3] + pentanomial[4];

	double t[3];
	double p[5];

	for (int i = 0; i < 3; i++)
		t[i] = (double)trinomial[i] / tN;

	for (int i = 0; i < 5; i++)
		p[i] = (double)pentanomial[i] / pN;

	double tt = 0.0;
	for (int i = 0; i < 3; i++)
		tt += (double)i / 2 * t[i];

	double pt = 0.0;
	for (int i = 0; i < 5; i++)
		pt += (double)i / 4 * p[i];

	double tsigma = 0.0;
	for (int i = 0; i < 3; i++)
		tsigma += ((double)i / 2) * ((double)i / 2) * t[i];
	tsigma = sqrt(tsigma - tt * tt);

	double psigma = 0.0;
	for (int i = 0; i < 5; i++)
		psigma += ((double)i / 4) * ((double)i / 4) * p[i];
	psigma = sqrt(psigma - pt * pt);

	printf("tt: %f\n", tt);
	printf("pt: %f\n", pt);
	printf("ts: %f\n", tsigma);
	printf("ps: %f\n", psigma);

	free(results);
}
