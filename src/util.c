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

uint64_t rand_uint64(void) {
	uint64_t ret = 0;
	for (int i = 0; i < 4; i++) {
		ret ^= (uint64_t)(rand() & 0xFFFF) << 16 * i;
	}
	return ret;
}

/* results not completely uniform */
int rand_int(int i) {
	return rand() % i;
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

int str_is_int(const char *s) {
	for (int i = 0; s[i]; i++)
		if (s[i] < '0' || s[i] > '9')
			return 0;
	return 1;
}

int str_to_int(const char *s) {
	int ret = 0;
	for (int i = 0; s[i]; i++)
		ret = ret * 10 + s[i] - '0';
	return ret;
}

char *appendstr(char *dest, const char *src) {
	size_t i, n;
	for (n = 0; dest[n]; n++);
	for (i = 0; i <= strlen(src); i++)
		dest[n + i] = src[i];
	return dest;
}

void merge(move *arr, uint64_t *val, unsigned int first, unsigned int last, int increasing) {
	if (!(first < last))
		return;
	unsigned int middle = (first + last) / 2;
	move temp_arr[MOVES_MAX];
	uint64_t temp_val[MOVES_MAX];

	unsigned int i = first, j = middle + 1, k = 0;

	while (i <= middle && j <= last) {
		/* xnor */
		if ((val[i] <= val[j]) == increasing) {
			temp_arr[k] = arr[i];
			temp_val[k] = val[i];
			i++;
		}
		else {
			temp_arr[k] = arr[j];
			temp_val[k] = val[j];
			j++;
		}
		k++;
	}

	while (i <= middle) {
		temp_arr[k] = arr[i];
		temp_val[k] = val[i];
		i++;
		k++;
	}

	while (j <= last) {
		temp_arr[k] = arr[j];
		temp_val[k] = val[j];
		j++;
		k++;
	}

	for (i = 0; i < k; i++) {
		arr[i + first] = temp_arr[i];
		val[i + first] = temp_val[i];
	}
}

void merge_sort(move *arr, uint64_t *val, unsigned int first, unsigned int last, int increasing) {
	if (first < last) {
		unsigned int middle = (first + last) / 2;
		merge_sort(arr, val, first, middle, increasing);
		merge_sort(arr, val, middle + 1, last, increasing);
		merge(arr, val, first, last, increasing);
	}
}

int variance(int16_t *arr, int len) {
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

void util_init(void) {
	srand(0);
	init_status("setting seed");
}
