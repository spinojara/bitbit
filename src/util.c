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

uint64_t log_2(uint64_t m) {
	if (m <= 2)
		return 1;
	return 1 + log_2(m / 2);
}

int find_char(char *s, char c) {
	for (int i = 0; s[i]; i++)
		if (s[i] == c)
			return i;
	return -1;
}

int str_is_int(char *s) {
	for (int i = 0; s[i]; i++)
		if (s[i] < '0' || s[i] > '9')
			return 0;
	return 1;
}

int str_to_int(char *s) {
	int ret = 0;
	for (int i = 0; s[i]; i++)
		ret = ret * 10 + s[i] - '0';
	return ret;
}

void util_init() {
	srand(0);
	init_status("setting seed");
}
