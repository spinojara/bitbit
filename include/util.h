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

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

#define CLAMP(a, b, c) MAX((b), MIN((a), (c)))

#define ABS(a) (((a) > 0) ? (a) : -(a))

#define SIZE(x) (sizeof(x) / sizeof (*(x)))

#define UNUSED(x) (void)(x)

#define MACRO_NAME(x) #x
#define MACRO_VALUE(x) MACRO_NAME(x)

uint64_t xorshift64();

uint64_t power(uint64_t m, uint64_t n);

uint64_t log_2(uint64_t m);

int nearint(double f);

int find_char(const char *s, char c);

/* returns 0 if s is not a non negative integer */
int strint(const char *s);

char *appendstr(char *dest, const char *src);

int variance(int16_t *arr, int len);

void printdigits(int d);

void printbinary(uint64_t d, int l);

uint32_t read_le_uint(FILE *f, int bytes);

void write_le_uint(FILE *f, uint32_t t, int bytes);

#endif
