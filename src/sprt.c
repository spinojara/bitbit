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

#define _POSIX_C_SOURCE 1
#include "sprt.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>

#include "util.h"
#include "testbitshared.h"

#define eps 1.0e-6

#define ALPHA(i) ((double)i / 4)

double sigmoid(double x) {
	return 1.0 / (1.0 + exp(-x * log(10.0) / 400.0));
}

double dsigmoiddx(double x) {
	double e = exp(-x * log(10.0) / 400.0);
	return log(10) / 400.0 * e / ((1 + e) * (1 + e));
}

double sigmoidinv(double y) {
	return -400.0 * log(1.0 / y - 1.0) / log(10.0);
}

double f_calc(const double mu, const double n[5], double C) {
	double sum = 0.0;
	for (int i = 0; i < 5; i++) {
		double num = (ALPHA(i) - C) * n[i];
		if (num)
			sum += num / (1.0 + (ALPHA(i) - C) * mu);
	}
	return sum;
}

double mu_bisect(const double n[5], double C) {
	double a = -1.0 / (1.0 - C);
	double b = 1.0 / C;
	while (1) {
		double c = (a + b) / 2;
		double f = f_calc(c, n, C);
		if (fabs(f) < eps) {
			return c;
		}
		if (f > 0)
			a = c;
		else
			b = c;
	}
}

void p_calc(double p[5], double mu, const double n[5], double C) {
	for (int i = 0; i < 5; i++)
		if (n[i] > 0)
			p[i] = n[i] / (1.0 + (ALPHA(i) - C) * mu);
}

/* Calculates L(theta, x) but instead takes mu as argument. */
double loglikelihood(double mu, double C, const double n[5]) {
	double p, sum = 0.0;
	for (int j = 0; j < 5; j++) {
		p = n[j] / (1.0 + (ALPHA(j) - C) * mu);
		/* Avoid 0 * log(0) which has limit 0 but standard says nan. */
		if (n[j])
			sum += n[j] * log(p);
	}

	return sum;
}

/* The perspective is from player 1 according to <doc/mle_pentanomial.pdf>.
 * If E1 and E2 are the Elos of player 1 and 2 respectively, then the hypotheses are
 * H0: E1 - E2 <= elo0.
 * H1: E1 - E2 >= elo1.
 */
int sprt_check(const unsigned long N[5], double alpha, double beta, double elo0, double elo1, double *llh) {
	unsigned long N_total = 0;
	for (int j = 0; j < 5; j++)
		N_total += N[j];

	double C[2];
	int use_score[2];
	double n[5];
	double mu;

	double A = log(beta / (1 - alpha));
	double B = log((1 - beta) / alpha);

	double llh2[2];

	double sum = 0.0;
	for (int j = 0; j < 5; j++) {
		n[j] = (double)N[j] / N_total;
		if (j == 0 || j == 4)
			n[j] = fmax(n[j], eps);

		sum += n[j];
	}
	for (int j = 0; j < 5; j++)
		n[j] /= sum;

	double score = 0.0;
	for (int j = 0; j < 5; j++)
		score += ALPHA(j) * n[j];

	C[0] = sigmoid(elo0);
	/* H0: E1 - E2 <= elo0. */
	use_score[0] = C[0] >= score;
	/* H1: E1 - E2 >= elo1. */
	C[1] = sigmoid(elo1);
	use_score[1] = C[1] <= score;
	
	for (int i = 0; i < 2; i++) {
		if (use_score[i]) {
			mu = 0.0;
		}
		else {
			C[i] = CLAMP(C[i], eps, 1.0 - eps);
			mu = mu_bisect(n, C[i]);
		}
		llh2[i] = N_total * loglikelihood(mu, C[i], n);
	}

	*llh = llh2[1] - llh2[0];

	if (*llh > B)
		return H1;
	else if (*llh < A)
		return H0;
	else
		return HNONE;
}

/* 0.95 grade confidence interval for Elo difference. */
double sprt_elo(const unsigned long N[5], double *plusminus) {
	unsigned long N_total = 0;
	for (int j = 0; j < 5; j++)
		N_total += N[j];

	double n[5];
	for (int j = 0; j < 5; j++)
		n[j] = (double)N[j] / N_total;

	double score = 0.0;
	for (int j = 0; j < 5; j++)
		score += ALPHA(j) * n[j];

	score = CLAMP(score, eps, 1.0 - eps);
	double elo = sigmoidinv(score);

	if (plusminus) {
		double sigma = - score * score;
		for (int j = 0; j < 5; j++)
			sigma += ALPHA(j) * ALPHA(j) * n[j];
		sigma = sqrt(sigma);

		/* 0.025 quantile of normal distribution. */
		double lambda = 1.96;

		double dSdx = dsigmoiddx(elo);

		*plusminus = lambda * sigma / (sqrt(N_total) * dSdx);
	}

	return elo;
}

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

int update_nomials(unsigned long trinomial[3], unsigned long pentanomial[5], struct result *results, unsigned long game) {
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
	trinomial[a]++;
	trinomial[b]++;
	return 1;
}

int sprt(unsigned long games, uint64_t trinomial[3], uint64_t pentanomial[5], double alpha, double beta, double maintime, double increment, double elo0, double elo1, double *llh, int threads, int sockfd) {
	char gamesstr[1024];
	char concurrencystr[1024];
	char timestr[1024];
	char threadstr[1024];
	int concurrency = 5;
	sprintf(gamesstr, "%lu", games);
	sprintf(concurrencystr, "%d", concurrency);
	sprintf(timestr, "tc=%lg+%lg", maintime, increment);
	sprintf(threadstr, "%d", threads);

	memset(trinomial, 0, sizeof(3 * *trinomial));
	memset(pentanomial, 0, sizeof(5 * *pentanomial));

	int pipefd[2];
	if (pipe(pipefd))
		exit(1);

	pid_t pid = fork();
	if (pid == -1)
		exit(2);

	if (pid == 0) {
		close(pipefd[0]);
		close(STDOUT_FILENO);
		
		dup2(pipefd[1], STDOUT_FILENO);

		/* We strongly prefer if the pipe from c-chess-cli to testbit
		 * is unbuffered. This is achieved by the library call
		 * setbuf(stdout, NULL);
		 *
		 * A fork with this simple change is retained at
		 * <https://github.com/Spinojara/c-chess-cli/tree/unbuffered>.
		 */
		execlp("c-chess-cli-unbuffered", "c-chess-cli-unbuffered", "-each", timestr,
				"-games", gamesstr,
				"-concurrency", concurrencystr,
				"-openings", "file=etc/book/5d6m100k.epd", "order=sequential",
				"-repeat",
				"-engine", "cmd=./bitbitold", "name=bitbitold",
				"-engine", "cmd=./bitbit", (char *)NULL);

		fprintf(stderr, "error: exec c-chess-cli-unbuffered\n");
		exit(3);
	}

	close(pipefd[1]);
	FILE *f = fdopen(pipefd[0], "r");
	if (!f) {
		kill(pid, SIGINT);
		return HERROR;
	}

	struct result *results = calloc(games, sizeof(*results));
	char *str, line[BUFSIZ];

	int H = HNONE;
	while (fgets(line, sizeof(line), f)) {
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
				fclose(f);
				kill(pid, SIGINT);
				free(results);
				return HERROR;
			}

			if (update_nomials(trinomial, pentanomial, results, game)) {
				unsigned long N = 0;
				for (int j = 0; j < 5; j++)
					N += pentanomial[j];

				/* We only check every 8 games. */
				if (N % 8 == 0) {
					if ((H = sprt_check(pentanomial, alpha, beta, elo0, elo1, llh)) != HNONE) {
						break;
					}
					else {
						/* Send update to sockfd. */
						char status = TESTRUNNING;
						if (sendall(sockfd, &status, 1) ||
								sendall(sockfd, (char *)trinomial, 3 * sizeof(*trinomial)) ||
								sendall(sockfd, (char *)pentanomial, 5 * sizeof(*pentanomial)) ||
								sendall(sockfd, (char *)llh, 8) ||
								sendall(sockfd, &status, 1))
							break;
						char mystatus;
						recvexact(sockfd, &mystatus, 1);
						if (mystatus == CANCELLED) {
							H = HCANCEL;
							break;
						}
					}
				}
			}
		}
		printf("%s", line);
	}

	fclose(f);
	kill(pid, SIGINT);
	waitpid(pid, NULL, 0);
	free(results);
	if (trinomial[0] + trinomial[1] + trinomial[2] != games && H == HNONE)
		H = HERROR;
	return H;
}
