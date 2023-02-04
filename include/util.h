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

#include "move.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

#define ABS(a) (((a) > 0) ? (a) : -(a))

#define SIZE(x) (sizeof(x) / sizeof (*(x)))

#define UNUSED(x) (void)(x)

#define MACRO_NAME(x) #x
#define MACRO_VALUE(x) MACRO_NAME(x)

uint64_t rand_uint64(void);

int rand_int(int i);

uint64_t power(uint64_t m, uint64_t n);

uint64_t log_2(uint64_t m);

int nearint(double f);

int find_char(const char *s, char c);

int str_is_int(const char *s);

int str_to_int(const char *s);

void merge_sort(move *arr, uint64_t *val, unsigned int first, unsigned int last, int increasing);

void util_init(void);

#endif
