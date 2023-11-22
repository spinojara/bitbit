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

#include <math.h>
#include <stdio.h>

#define eps 1.0e-6

#define ALPHA(i) ((double)i / 4)

double sigmoid(double x) {
	return 1.0 / (1.0 + exp(-x * log(10) / 400));
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
	printf("[%lf, %lf]\n", a, b);
	while (1) {
		double c = (a + b) / 2;
		double f = f_calc(c, n, C);
		if (fabs(f) < eps)
			return c;
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

/* Calculates L(theta, x). */
double loglikelihood(const double p[5], const unsigned long N[5]) {
	double sum = 0.0;
	for (int i = 0; i < 5; i++)
		/* Avoid 0 * log(0) which has limit 0
		 * but C standard says nan. */
		if (N[i])
			sum += N[i] * log(p[i]);
	return sum;
}

int main(void) {
	/* Careful with too large C which are rounded to 1.
	 * CLAMP(C, eps, 1.0 - eps);
	 */
	double alpha = 0.05;
	double beta = 0.05;
	double mu;
	const unsigned long N[5] = { 0, 17, 23, 16, 0 };
	double n[5];
	double p[5];
	double sum = 0.0;
	for (int i = 0; i < 5; i++) {
		n[i] = (double)N[i] / (N[0] + N[1] + N[2] + N[3] + N[4]);
#if 1
		if (i == 0 || i == 4)
			n[i] = fmax(n[i], eps);
#endif
		sum += n[i];
	}
	for (int i = 0; i < 5; i++)
		n[i] /= sum;
	printf("%lf - %lf - %lf - %lf - %lf\n", n[0], n[1], n[2], n[3], n[4]);

	double elo1 = 0.0;
	double elo2 = 10.0;
	double C1 = sigmoid(elo1);
	printf("C1: %lf\n", C1);
	double C2 = sigmoid(elo2);
	printf("C2: %lf\n", C2);

	mu = mu_bisect(n, C1);
	printf("mu: %lf\n", mu);
	p_calc(p, mu, n, C1);
	double l1 = loglikelihood(p, N);
	printf("%lf - %lf - %lf - %lf - %lf\n", p[0], p[1], p[2], p[3], p[4]);
	printf("%lf\n", l1);

	mu = mu_bisect(n, C2);
	printf("mu: %lf\n", mu);
	p_calc(p, mu, n, C2);
	double l2 = loglikelihood(p, N);
	printf("%lf - %lf - %lf - %lf - %lf\n", p[0], p[1], p[2], p[3], p[4]);

	double a = log(beta / (1 - alpha));
	double b = log((1 - beta) / alpha);


	printf("%lf <= %lf <= %lf\n", a, l1 - l2, b);
}
