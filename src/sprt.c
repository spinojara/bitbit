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

#include "sprt.h"

#include <math.h>
#include <stdio.h>

#include "util.h"

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
	printf("SPRT LLH: ");
	double p, sum = 0.0;
	for (int j = 0; j < 5; j++) {
		p = n[j] / (1.0 + (ALPHA(j) - C) * mu);
		/* Avoid 0 * log(0) which has limit 0 but standard says nan. */
		printf("%lf, ", p);
		if (n[j])
			sum += n[j] * log(p);
	}
	printf(" gives %lf\n", sum);

	return sum;
}

/* The perspective is from player 1 according to <doc/mle_pentanomial.pdf>.
 * If E1 and E2 are the Elos of player 1 and 2 respectively, then the hypotheses are
 * H0: E1 - E2 <= elo0.
 * H1: E1 - E2 >= elo1.
 */
int sprt_check(const unsigned long N[5], double alpha, double beta, double elo0, double elo1) {
	printf("SPRT: %lu, %lu, %lu, %lu, %lu\n", N[0], N[1], N[2], N[3], N[4]);
	unsigned long N_total = 0;
	for (int j = 0; j < 5; j++)
		N_total += N[j];

	double C[2];
	int use_score[2];
	double n[5];
	double mu;

	double A = log(beta / (1 - alpha));
	double B = log((1 - beta) / alpha);

	double llh[2];

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
		llh[i] = N_total * loglikelihood(mu, C[i], n);
	}

	double t = llh[0] - llh[1];

	printf("SPRT: %lf<%lf<%lf\n", A, t, B);
	printf("SPRT: %lf, %lf\n", C[0], C[1]);
	printf("SPRT: %lf, %lf\n", sigmoidinv(C[0]), sigmoidinv(C[1]));
	if (t > B)
		return H0;
	else if (t < A)
		return H1;
	else
		return NONE;
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
