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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "util.h"
#include "option.h"
#include "position.h"
#include "bitboard.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "movegen.h"

/* I don't currently want to spend time refactoring the code to allow for
 * chess960 castling. Since castling is important for game outcomes we require
 * that the rooks and king are in their original position. This option is thus
 * actually chess18.
 */
int chess960 = 0;
int moves_max = 16;
int moves_min = 8;
int trim_fens = 0;

void genepd_startpos(struct position *pos, uint64_t *seed) {
	startpos(pos);
	if (!chess960)
		return;
	for (int sq = b1; sq < h1; sq++) {
		if (sq == e1)
			continue;
		pos->mailbox[sq] = empty;
	}
	pos->piece[white][knight] = pos->piece[white][bishop] = pos->piece[white][queen] = 0;
	pos->piece[black][knight] = pos->piece[black][bishop] = pos->piece[black][queen] = 0;

	/* Dark squared bishop. Can be c1 or g1. */
	int db[2] = { c1, g1 };
	pos->mailbox[db[xorshift64(seed) % 2]] = white_bishop;
	/* Light squared bishop. Can be b1, d1 or f1. */
	int lb[3] = { b1, d1, f1 };
	pos->mailbox[lb[xorshift64(seed) % 3]] = white_bishop;
	
	/* Queen square. One of the remaining 3 squares.
	 * The other two squares are occupied by knights.
	 */
	int qs = xorshift64(seed) % 3;
	for (int sq = b1; sq < h1; sq++) {
		if (!pos->mailbox[sq]) {
			if (!qs)
				pos->mailbox[sq] = white_queen;
			else
				pos->mailbox[sq] = white_knight;
			qs--;
		}
	}

	for (int sq = b1; sq < h1; sq++) {
		if (sq == e1)
			continue;
		int piece = uncolored_piece(pos->mailbox[sq]);
		int bsq = orient_horizontal(black, sq);
		pos->mailbox[bsq] = colored_piece(piece, black);
		pos->piece[white][piece] |= bitboard(sq);
		pos->piece[black][piece] |= bitboard(bsq);
	}

	for (int color = 0; color < 2; color++) {
		pos->piece[color][all] = 0;
		for (int piece = pawn; piece <= king; piece++)
			pos->piece[color][all] |= pos->piece[color][piece];
	}
}

int genepd_position(struct position *pos, uint64_t *seed) {
	genepd_startpos(pos, seed);

	move movelist[MOVES_MAX];
	int moves_num = moves_min;
	if (moves_max > moves_min)
		moves_num += xorshift64(seed) % (moves_max - moves_min);
		
	for (int moves = 0; moves < moves_num; moves++) {
		generate_all(pos, movelist);
		int c = move_count(movelist);
		if (!c)
			return 1;

		move m = movelist[xorshift64(seed) % c];
		do_move(pos, &m);
	}

	generate_all(pos, movelist);
	if (!movelist[0])
		return 1;
	return 0;
}

int main(int argc, char **argv) {
	int count = 0;
	char *outfilename = "out.epd";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--chess960"))
			chess960 = 1;
		else if (!strcmp(argv[i], "--trim-fens"))
			trim_fens = 1;
		else if (!strcmp(argv[i], "--moves-min")) {
			i++;
			if (!(i < argc))
				break;
			moves_min = strint(argv[i]);
		}
		else if (!strcmp(argv[i], "--moves-max")) {
			i++;
			if (!(i < argc))
				break;
			moves_max = strint(argv[i]);
		}
		else if (strncmp(argv[i], "--", 2)) {
			if (!count)
				count = strint(argv[i]);
			else
				outfilename = argv[i];
		}
		else {
			printf("ignoring unknown option: %s\n", argv[i]);
		}
	}

	if (!count) {
		fprintf(stderr, "number of fens to generate needs to be greater than 0\n");
		exit(1);
	}
	if (moves_max < moves_min || moves_min < 0) {
		fprintf(stderr, "moves-min must be at least 0 and moves-max cannot be less than moves-min\n");
		exit(1);
	}
	FILE *fout = fopen(outfilename, "w");
	if (!fout) {
		fprintf(stderr, "failed to open file \"%s\"\n", outfilename);
		exit(3);
	}

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	position_init();

	struct position pos;

	uint64_t seed = time(NULL);

	for (int i = 0; i < count; i++) {
		char fen[128];
		if (genepd_position(&pos, &seed)) {
			i--;
			continue;
		}
		pos_to_fen(fen, &pos);
		printf("%s\n", fen);
		fprintf(fout, "%s\n", fen);
	}

	fclose(fout);
}
