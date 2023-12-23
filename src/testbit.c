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
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/wait.h>

#include "sprt.h"

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

int update_pentanomial(unsigned long pentanomial[5], struct result *results, unsigned long game) {
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
		return 0;

	unsigned a = results[game].result;
	unsigned b = results[first].result;

	pentanomial[a + b]++;
	return 1;
}

int sprt(unsigned long games, double alpha, double beta, double elo0, double elo1) {
	//games = 10;
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
		close(STDOUT_FILENO);
		
		dup2(pipefd[1], STDOUT_FILENO); /* Redirect stdout to write end. */

		/* We strongly prefer if the pipe from c-chess-cli to testbit
		 * is unbuffered. This is achieved by the library call
		 * setbuf(stdout, NULL);
		 *
		 * A fork with this simple change is retained at
		 * <https://github.com/spinosarus123/c-chess-cli/tree/unbuffered>.
		 */
		execlp("c-chess-cli", "c-chess-cli", "-each", "tc=1+0.03",
				"-games", gamesstr,
				"-concurrency", "8",
				"-repeat",
				"-engine", "cmd=/usr/src/bitbit/bitbitold", "name=old",
				"-engine", "cmd=/usr/src/bitbit/bitbit", NULL);

		perror("error: failed to open c-chess-cli\n");
		exit(3);
	}

	struct result *results = calloc(games, sizeof(*results));
	unsigned long trinomial[3] = { 0 };
	unsigned long pentanomial[5] = { 0 };

	close(pipefd[1]); /* Close unused write end. */
	FILE *f = fdopen(pipefd[0], "r");
	char *str, line[BUFSIZ];

	int H = NONE;
	while (1) {
		fgets(line, sizeof(line), f);
		if ((str = strstr(line, "Finished game"))) {
			unsigned long game = game_number(str + 14);
			/* Fixes the problem while parsing that some games
			 * are played as white and some as black.
			 */
			int white = !((game + 1) % 2);
			results[game].done = 1;

			str = strrchr(line, '{') - 2;

			switch (*str) {
			/* 1-0 */
			case '0':
				results[game].result = white ? WIN : LOSS;
				break;
			/* 0-1 */
			case '1':
				results[game].result = white ? LOSS : WIN;
				break;
			/* 1/2-1/2 */
			case '2':
				results[game].result = DRAW;
				break;
			default:
				assert(0);
			}
			trinomial[results[game].result]++;

			if (update_pentanomial(pentanomial, results, game)) {
				unsigned long N = 0;
				for (int j = 0; j < 5; j++)
					N += pentanomial[j];

				if (N % 8 == 0) {
					double plusminus;
					double elo = sprt_elo(pentanomial, &plusminus);
					printf("Elo: %lf +- %lf\n", elo, plusminus);
				}
				/* We only check every 8 games. */
				if (N % 8 == 0 && (H = sprt_check(pentanomial, alpha, beta, elo0, elo1))) {
					break;
				}
			}
		}
		//printf("%s", line);
	}

	kill(pid, SIGINT);
	waitpid(pid, NULL, 0);
	free(results);
	if (H == H0)
		printf("H0 accepted\n");
	else if (H == H1)
		printf("H1 accepted\n");
	else
		printf("None accepted\n");
	return H;
}

int main(void) {
	sprt(400, 0.05, 0.05, 0.0, 10.0);
	return 0;
}
