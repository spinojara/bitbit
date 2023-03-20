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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "position.h"
#include "move.h"
#include "move_gen.h"
#include "util.h"
#include "magic_bitboard.h"
#include "bitboard.h"
#include "transposition_table.h"
#include "attack_gen.h"
#include "nnue.h"
#include "search.h"
#include "evaluate.h"
#include "pawn.h"
#include "perft.h"

int main(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);

	util_init();
	magic_bitboard_init();
	attack_gen_init();
	bitboard_init();
	evaluate_init();
	search_init();
	transposition_table_init();
	position_init();
	pawn_init();

	FILE *f = fopen("train.bin", "wb");
	
	struct position pos[1];
	startpos(pos);
	move move_list[MOVES_MAX];
	move m;
	int16_t eval;
	//srand(time(NULL));
	srand(0);

	clock_t start_time = clock();
	int i = 1;
	int fens = 10000;
	while (i <= fens) {
		m = 0;
		eval = evaluate(pos, 3, 0, 0, 0, &m, NULL, 0);
		write_le_uint16(f, eval);
		if (!m || pos->fullmove >= 50 || i == fens) {
			write_le_uint16(f, 0);
			startpos(pos);
		}
		else if (rand() % 5 == 0) {
			generate_all(pos, move_list);
			m = move_list[rand() % move_count(move_list)];
			write_le_uint16(f, (uint16_t)m);
			do_move(pos, &m);
		}
		else {
			write_le_uint16(f, (uint16_t)m);
			do_move(pos, &m);
		}

		i++;
	}
	clock_t end_time = clock() - start_time;
	printf("time: %li\n", end_time / CLOCKS_PER_SEC);
	if (end_time)
		printf("fens per second: %li\n", fens * CLOCKS_PER_SEC / end_time);

	pawn_init();
	transposition_table_term();
	position_term();
	fclose(f);
}
