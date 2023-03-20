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

#include "move.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

#define CLAMP(a, b, c) MAX((b), MIN((a), (c)))

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

/* returns 0 if s is not a non negative integer */
int strint(const char *s);

char *appendstr(char *dest, const char *src);

void merge_sort(move *arr, uint64_t *val, unsigned int first, unsigned int last, int increasing);

int variance(int16_t *arr, int len);

void printdigits(int d);

void util_init(void);

static inline uint8_t read_le_uint8(FILE *f) {
	uint8_t buf[1];
	if (!fread(buf, sizeof(buf), 1, f))
		return 0;
	return buf[0];
}

static inline uint16_t read_le_uint16(FILE *f) {
	uint8_t buf[2];
	if (!fread(buf, sizeof(buf), 1, f))
		return 0;
	return buf[0] | (buf[1] << 8);
}

static inline uint32_t read_le_uint32(FILE *f) {
	uint8_t buf[4];
	if (!fread(buf, sizeof(buf), 1, f))
		return 0;
	return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static inline void write_le_uint8(FILE *f, uint8_t t) {
	uint8_t buf[1];
	buf[0] = t;
	fwrite(buf, sizeof(buf), 1, f);
}

static inline void write_le_uint16(FILE *f, uint16_t t) {
	uint8_t buf[2];
	buf[0] = ((uint8_t *)(&t))[0];
	buf[1] = ((uint8_t *)(&t))[1];
	fwrite(buf, sizeof(buf), 1, f);
}

static inline void write_le_uint32(FILE *f, uint32_t t) {
	uint8_t buf[4];
	buf[0] = ((uint8_t *)(&t))[0];
	buf[1] = ((uint8_t *)(&t))[1];
	buf[2] = ((uint8_t *)(&t))[2];
	buf[3] = ((uint8_t *)(&t))[3];
	fwrite(buf, sizeof(buf), 1, f);
}

static inline int reset_file_pointer(FILE *f) {
	if (feof(f)) {
		fseek(f, 0, SEEK_SET);
		return 1;
	}
	return 0;
}

#endif
