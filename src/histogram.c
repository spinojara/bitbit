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
#include <stdlib.h>

#include "move.h"
#include "position.h"
#include "nnue.h"
#include "evaluate.h"

void store_information(struct position *pos, uint64_t piece_square[7][64]) {
	for (int color = 0; color < 2; color++) {
		for (int piece = pawn; piece <= king; piece++) {
			uint64_t b = pos->piece[color][piece];
			while (b) {
				int square = ctz(b);
				square = orient(color, square);
				piece_square[piece][square]++;
				b = clear_ls1b(b);
			}
		}
	}
}

void print_information(uint64_t square[64], uint64_t total) {
	for (int y = 0; y < 8; y++) {
		printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n|");
		for (int x = 0; x < 8; x++) {
			int sq = 8 * (7 - y) + x;
			printf(" %5.2f |", 100.f * square[sq] / total);
		}
		printf("\n");
	}
	printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");
	printf("\n");
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("provide a filename\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		printf("could not open %s\n", argv[1]);
		return 2;
	}

	position_init();
	struct position pos;

	uint64_t piece_square[7][64] = { 0 };

	move m;
	int16_t eval;
	uint64_t total = 0;
	size_t count = 0;
	size_t games = 0;
	while (1) {
		count++;
		if (count % 20000 == 0)
			printf("collecting data: %lu\r", count);
		m = 0;
		fread(&m, 2, 1, f);
		if (m) {
			do_move(&pos, &m);
		}
		else {
			fread(&pos, sizeof(struct partialposition), 1, f);
			if (!feof(f))
				games++;
		}

		if (!pos.piece[white][king] || !pos.piece[black][king]) {
			fprintf(stderr, "Missing king in position\n");
			exit(1);
		}
		
		fread(&eval, 2, 1, f);
		if (feof(f))
			break;

		if (eval != VALUE_NONE) {
			store_information(&pos, piece_square);
			total++;
		}
	}

	printf("total positions: %lu\n", total);
	printf("total games: %lu\n", games);
	for (int piece = pawn; piece <= king; piece++)
		print_information(piece_square[piece], total);
}
