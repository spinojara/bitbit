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

#include "perft.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "move.h"
#include "movegen.h"
#include "interface.h"
#include "interrupt.h"

uint64_t perft(struct position *pos, int depth, int verbose) {
	if (depth <= 0 || interrupt)
		return 0;
	move_t move_list[MOVES_MAX];
	generate_all(pos, move_list);
	uint64_t nodes = 0, count;

	for (move_t *ptr = move_list; *ptr; ptr++){
		if (depth == 1) {
			count = 1;
			nodes++;
		}
		else {
			do_move(pos, ptr);
			count = perft(pos, depth - 1, 0);
			undo_move(pos, ptr);
			nodes += count;
		}
		if (verbose && !interrupt) {
			print_move(ptr);
			printf(": %" PRIu64 "\n", count);
		}
	}
	return nodes;
}
