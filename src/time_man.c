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

#include "time_man.h"

#include <stdint.h>
#include <stdio.h>

#include "util.h"

int time_man(int etime, int16_t saved_evaluation[256], uint8_t depth) {
	int var = 0;
	if (depth > 2)
		var = variance(saved_evaluation + depth - 3, 3);
	int a = (double)etime / 20 * (1 + (double)var / 4096);
	return a;
}
