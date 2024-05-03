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

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define SIZE(x) (sizeof(x) / sizeof (*(x)))

#define UNUSED(x) (void)(x)

#define XSTR(x) STR(x)
#define STR(x) #x

#define SEED 1274012836ull

extern const double eps;

uint64_t gxorshift64(void);
uint64_t xorshift64(uint64_t *seed);
int gbernoulli(double p);
int bernoulli(double p, uint64_t *seed);

static inline int max(int a, int b) { return a < b ? b : a; }
static inline int min(int a, int b) { return a < b ? a : b; }
static inline int clamp(int a, int b, int c) { return max(b, min(a, c)); }
static inline double fclamp(double a, double b, double c) { return fmax(b, fmin(a, c)); }

uint64_t power(uint64_t m, uint64_t n);

int find_char(const char *s, char c);

/* Returns 0 if s is not a non negative integer. */
int strint(const char *s);

#endif
