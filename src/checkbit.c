/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2024 Isak Ellmer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

#include "position.h"
#include "move.h"
#include "io.h"
#include "evaluate.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "bitboard.h"
#include "position.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "error: no argument\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		fprintf(stderr, "error: failed to open file '%s'\n", argv[1]);
		return 2;
	}

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	position_init();

	struct position pos = { 0 };
	char result;
	int32_t eval;
	move_t move;
	unsigned char flag;

	int first = 1;

	while (1) {
		int r;
		if ((r = read_move(f, &move)))
			return r == 2 && feof(f) ? 0 : 3;

		if (move) {
			if (first)
				return 9;
			struct pstate ps;
			pstate_init(&pos, &ps);
			char movestr[16];
			char fen[128];
			if (!pseudo_legal(&pos, &ps, &move) || !legal(&pos, &ps, &move)) {
				fprintf(stderr, "error: illegal move %s for position %s\n", move_str_algebraic(movestr, &move), pos_to_fen(fen, &pos));
				return 4;
			}
			do_move(&pos, &move);
		}
		else {
			if (read_position(f, &pos))
				return 5;
			if (!pos_is_ok(&pos))
				return 6;
			if (read_result(f, &result) || (result != RESULT_LOSS && result != RESULT_DRAW && result != RESULT_WIN && result != RESULT_UNKNOWN))
				return 7;
		}
		first = 0;

		if (read_eval(f, &eval) || (eval != VALUE_NONE && (eval < -VALUE_INFINITE || eval > VALUE_INFINITE)))
			return 8;
		if (read_flag(f, &flag))
			return 10;
	}
}
