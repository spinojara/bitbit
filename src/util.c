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

#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(NDEBUG) && defined(__GNUC__) && defined(__linux__)
#include <execinfo.h>
#endif

#include "bitboard.h"

const double eps = 1.0e-6;

/* <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf> */
uint64_t gseed = SEED;
uint64_t gxorshift64(void) {
	return xorshift64(&gseed);
}
uint64_t xorshift64(uint64_t *seed) {
	*seed ^= *seed >> 12;
	*seed ^= *seed << 25;
	*seed ^= *seed >> 27;
	return *seed * 2685821657736338717ull;
}
int gbernoulli(double p) {
	return bernoulli(p, &gseed);
}
int bernoulli(double p, uint64_t *seed) {
	return uniform(seed) < p;
}
double guniform(void) {
	return uniform(&gseed);
}
double uniform(uint64_t *seed) {
	const uint64_t max = (uint64_t)1 << 32;
	return (double)(xorshift64(seed) % max) / max;
}
int uniformint(uint64_t *seed, int a, int b) {
	return (int)((double)a + uniform(seed) * (b - a));
}

int find_char(const char *s, char c) {
	for (int i = 0; s[i]; i++)
		if (s[i] == c)
			return i;
	return -1;
}

int strint(const char *s) {
	int ret = 0;
	for (int i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9')
			return 0;
		else
			ret = ret * 10 + s[i] - '0';
	}
	return ret;
}

#if !defined(NDEBUG) && defined(__GNUC__) && defined(__linux__)
/* <https://www.gnu.org/software/libc/manual/html_node/Backtraces.html> */
void stacktrace(void) {
	void *array[100];
	char **strings;
	int size, i;

	size = backtrace(array, 10);
	strings = backtrace_symbols(array, size);
	if (strings != NULL) {
		printf("Obtained %d stack frames\n", size);
		for (i = 0; i < size; i++)
			printf("%s\n", strings[i]);
	}

	free(strings);
}
#endif
