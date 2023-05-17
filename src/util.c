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

#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "init.h"
#include "bitboard.h"

/* <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf> */
uint64_t seed = 1274012836ull;
uint64_t xorshift64() {
	seed ^= seed >> 12;
	seed ^= seed << 25;
	seed ^= seed >> 27;
	return seed * 2685821657736338717ull;
}

uint64_t log_2(uint64_t m) {
	if (m <= 2)
		return 1;
	return 1 + log_2(m / 2);
}

int nearint(double f) {
	if (f < 0)
		return -nearint(-f);
	int ret = (int)f;
	return ret + (f - ret >= 0.5);
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

char *appendstr(char *dest, const char *src) {
	size_t i, n;
	for (n = 0; dest[n]; n++);
	for (i = 0; i <= strlen(src); i++)
		dest[n + i] = src[i];
	return dest;
}

int variance(int16_t *arr, int len) {
	if (len < 2)
		return -1;
	int variance = 0;
	int mean = 0;
	for (int i = 0; i < len; i++)
		mean += arr[i];
	mean = mean / len;
	for (int i = 0; i < len; i++)
		variance += (arr[i] - mean) * (arr[i] - mean);
	variance /= len - 1;
	return variance;
}

void printdigits(int d) {
	double f = (double)d / 100;
	if (d >= 1000 || d <= -1000)
		printf("%+.1f", f);
	else if (d == 0)
		printf("+0.00");
	else
		printf("%+.2f", f);
}

void printbinary(uint64_t b, int l) {
	for (int i = l - 1; i >= 0; i--) {
		printf("%d", get_bit(b, i) ? 1 : 0);
	}
}

uint32_t read_le_uint(FILE *f, int bytes) {
	uint8_t buf[4] = { 0 };
	if (!fread(buf, bytes, 1, f))
		return 0;
	return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

void write_le_uint(FILE *f, uint32_t t, int bytes) {
	uint8_t buf[4] = { 0 };
	memcpy(buf, &t, 4);
	fwrite(buf, bytes, 1, f);
}
