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
#include <signal.h>

#include "move.h"
#include "move_gen.h"
#include "interrupt.h"

uint64_t perft(struct position *pos, int depth, int verbose) {
	if (depth <= 0)
		return 0;
	return pos->turn ? perft_white(pos, depth, verbose) : perft_black(pos, depth, verbose);
}

uint64_t perft_white(struct position *pos, int depth, int verbose) {
	if (interrupt)
		return 0;
	move move_list[256];
	generate_white(pos, move_list);
	uint64_t nodes = 0, count;

	for (move *move_ptr = move_list; *move_ptr; move_ptr++){
		if (depth == 1) {
			count = 1;
			nodes++;
		}
		else {
			do_move_perft(pos, move_ptr);
			count = perft_black(pos, depth - 1, 0);
			undo_move_perft(pos, move_ptr);
			nodes += count;
		}
		if (verbose && !interrupt) {
			print_move(move_ptr);
			printf(": %" PRIu64 "\n", count);
		}
	}
	return nodes;
}

uint64_t perft_black(struct position *pos, int depth, int verbose) {
	if (interrupt)
		return 0;
	move move_list[256];
	generate_black(pos, move_list);
	uint64_t nodes = 0, count;

	for (move *move_ptr = move_list; *move_ptr; move_ptr++){
		if (depth == 1) {
			count = 1;
			nodes++;
		}
		else {
			do_move_perft(pos, move_ptr);
			count = perft_white(pos, depth - 1, 0);
			undo_move_perft(pos, move_ptr);
			nodes += count;
		}
		if (verbose && !interrupt) {
			print_move(move_ptr);
			printf(": %" PRIu64 "\n", count);
		}
	}
	return nodes;
}
