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

uint64_t rand_uint64() {
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

int power(int m, int n) {
	if (n == 0)
		return 1;
	return m * power(m, n - 1);
}

int find_char(char *s, char c) {
	for (long unsigned int i = 0; i < strlen(s); i++)
		if (s[i] == c)
			return i;
	return -1;
}

int string_is_int(char *s) {
	for (long unsigned int i = 0; i < strlen(s); i++)
		if (find_char("0123456789", s[i]) == -1)
			return 0;
	return 1;
}

void merge_sort(move *arr, int16_t *val, unsigned int first, unsigned int last, int increasing) {
	if (first < last) {
		unsigned int middle = (first + last) / 2;
		merge_sort(arr, val, first, middle, increasing);
		merge_sort(arr, val, middle + 1, last, increasing);
		merge(arr, val, first, last, increasing);
	}
}

void merge(move *arr, int16_t *val, unsigned int first, unsigned int last, int increasing) {
	if (!(first < last))
		return;
	unsigned int middle = (first + last) / 2;
	unsigned int length = last - first + 1;
	move *temp_arr = malloc(length * sizeof(move));
	int16_t *temp_val = malloc(length * sizeof(int16_t));

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

	free(temp_arr);
	free(temp_val);
}

void util_init() {
	srand(0);
	init_status("setting seed");
}
