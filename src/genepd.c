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
#include "transposition.h"
#include "search.h"
#include "tables.h"
#include "moveorder.h"

/* I don't currently want to spend time refactoring the code to allow for
 * chess960 castling. Since castling is important for game outcomes we require
 * that the rooks and king are in their original position. This option is thus
 * actually chess18.
 */
int chess960 = 0;
int moves_max = 16;
int moves_min = 8;
int unique = 0;
int centipawns = 50;
int filter_depth = -1;
int minor_pieces = 0;

void genepd_startpos(struct position *pos, uint64_t *seed) {
	startpos(pos);
	if (!chess960)
		return;
	for (int sq = b1; sq < h1; sq++) {
		if (sq == e1)
			continue;
		pos->mailbox[sq] = EMPTY;
	}
	pos->piece[WHITE][KNIGHT] = pos->piece[WHITE][BISHOP] = pos->piece[WHITE][QUEEN] = 0;
	pos->piece[BLACK][KNIGHT] = pos->piece[BLACK][BISHOP] = pos->piece[BLACK][QUEEN] = 0;

	/* Dark squared bishop. Can be c1 or g1. */
	int db[2] = { c1, g1 };
	pos->mailbox[db[xorshift64(seed) % 2]] = WHITE_BISHOP;
	/* Light squared bishop. Can be b1, d1 or f1. */
	int lb[3] = { b1, d1, f1 };
	pos->mailbox[lb[xorshift64(seed) % 3]] = WHITE_BISHOP;
	
	/* Queen square. One of the remaining 3 squares.
	 * The other two squares are occupied by knights.
	 */
	int qs = xorshift64(seed) % 3;
	for (int sq = b1; sq < h1; sq++) {
		if (!pos->mailbox[sq]) {
			if (!qs)
				pos->mailbox[sq] = WHITE_QUEEN;
			else
				pos->mailbox[sq] = WHITE_KNIGHT;
			qs--;
		}
	}

	for (int sq = b1; sq < h1; sq++) {
		if (sq == e1)
			continue;
		int piece = uncolored_piece(pos->mailbox[sq]);
		int bsq = orient_horizontal(BLACK, sq);
		pos->mailbox[bsq] = colored_piece(piece, BLACK);
		pos->piece[WHITE][piece] |= bitboard(sq);
		pos->piece[BLACK][piece] |= bitboard(bsq);
	}

	for (int color = 0; color < 2; color++) {
		pos->piece[color][ALL] = 0;
		for (int piece = PAWN; piece <= KING; piece++)
			pos->piece[color][ALL] |= pos->piece[color][piece];
	}
}

int already_written(struct position *pos, uint64_t *written_keys, int i) {
	refresh_zobrist_key(pos);
	for (int j = 0; j < i; j++)
		if (written_keys[j] == pos->zobrist_key)
			return 1;
	return 0;
}

int genepd_position(struct position *pos, struct transpositiontable *tt, uint64_t *written_keys, int i, uint64_t *seed) {
	genepd_startpos(pos, seed);

	move_t movelist[MOVES_MAX];
	int moves_num = moves_min;
	moves_num += xorshift64(seed) % (moves_max + 1 - moves_min);
		
	for (int moves = 0; moves < moves_num; moves++) {
		generate_all(pos, movelist);
		int c = move_count(movelist);
		if (!c)
			return 1;

		move_t m = 0;
		for (int j = 0; j < 16 && m == 0; j++) {
			m = movelist[xorshift64(seed) % c];
			int piece = uncolored_piece(pos->mailbox[move_from(&m)]);
			if (minor_pieces && (piece == ROOK || piece == QUEEN || piece == KING))
				m = 0;
		}
		if (!m)
			return 1;
		do_move(pos, &m);
	}

	generate_all(pos, movelist);
	if (!movelist[0] ||
			(unique && already_written(pos, written_keys, i)) ||
			(filter_depth >= 0 && abs(search(pos, filter_depth, 0, 0, 0, NULL, tt, NULL, 1)) > centipawns))
		return 1;
	return 0;
}

int main(int argc, char **argv) {
	int count = 0;
	char *outfilename = "out.epd";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--chess960"))
			chess960 = 1;
		else if (!strcmp(argv[i], "--unique"))
			unique = 1;
		else if (!strcmp(argv[i], "--minor-pieces"))
			minor_pieces = 1;
		else if (!strcmp(argv[i], "--centipawns")) {
			i++;
			if (!(i < argc))
				break;
			centipawns = strint(argv[i]);
		}
		else if (!strcmp(argv[i], "--filter-depth")) {
			i++;
			if (!(i < argc))
				break;
			filter_depth = strint(argv[i]);
		}
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
		else if (!strncmp(argv[i], "--", 2))
			printf("ignoring unknown option: %s\n", argv[i]);
		else if (!count)
			count = strint(argv[i]);
		else
			outfilename = argv[i];
	}

	if (!count) {
		fprintf(stderr, "number of fens to generate needs to be greater than 0\n");
		return 1;
	}
	if (moves_max < moves_min || moves_min < 0) {
		fprintf(stderr, "moves-min must be at least 0 and moves-max cannot be less than moves-min\n");
		return 1;
	}
	FILE *fout = fopen(outfilename, "w");
	if (!fout) {
		fprintf(stderr, "failed to open file \"%s\"\n", outfilename);
		return 1;
	}

	option_nnue = 0;
	option_transposition = 1;
	option_history = 0;
	option_endgame = 0;
	option_damp = 0;

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	tables_init();
	search_init();
	moveorder_init();
	position_init();
	transposition_init();

	struct transpositiontable tt;
	if (filter_depth >= 0)
		transposition_alloc(&tt, 4 * 1024 * 1024);

	uint64_t *written_keys = NULL;
	if (unique)
		written_keys = malloc(count * sizeof(*written_keys));

	struct position pos;

	uint64_t seed = time(NULL);

	for (int i = 0; i < count; i++) {
		char fen[128];
		transposition_clear(&tt);
		if (genepd_position(&pos, &tt, written_keys, i, &seed)) {
			i--;
			continue;
		}
		pos_to_fen(fen, &pos);
		printf("%s\n", fen);
		fprintf(fout, "%s\n", fen);
		if (unique) {
			refresh_zobrist_key(&pos);
			written_keys[i] = pos.zobrist_key;
		}
	}

	if (filter_depth >= 0)
		transposition_free(&tt);
	if (unique)
		free(written_keys);
	fclose(fout);
}
