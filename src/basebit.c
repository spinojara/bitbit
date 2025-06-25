/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2025 Isak Ellmer
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
#include <time.h>
#include <ctype.h>

#include "bitbase.h"
#include "position.h"
#include "bitboard.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "util.h"
#include "movegen.h"

#define BITBASE_KXKX_INDEX_MAX (2l * 64 * 6 * 64 * 64 * 6 * 64)
#define BITBASE_KXKX_BITS_PER_POSITION (2)
#define BITBASE_KXKX_BITS_MASK ((1 << BITBASE_KXKX_BITS_PER_POSITION) - 1)
#define BITBASE_KXKX_BITS_PER_ENTRY (8 * sizeof(*bitbase_KXKX))
#define BITBASE_KXKX_POSITIONS_PER_ENTRY (BITBASE_KXKX_BITS_PER_ENTRY / BITBASE_KXKX_BITS_PER_POSITION)
#define BITBASE_KXKX_TABLE_SIZE (BITBASE_KXKX_INDEX_MAX * BITBASE_KXKX_BITS_PER_POSITION / BITBASE_KXKX_BITS_PER_ENTRY)

uint32_t *bitbase_KXKX;
uint32_t *invalid_KXKX;
uint32_t bitbase_KPK[BITBASE_KPK_TABLE_SIZE] = { 0 };
uint32_t bitbase_KPKP[BITBASE_KPKP_TABLE_SIZE] = { 0 };
uint32_t bitbase_KRKP[BITBASE_KRKP_TABLE_SIZE] = { 0 };

void write_bitbase(char *name, uint32_t *bitbase, size_t table_size);

long bitbase_KXKX_index_by_square(int turn, int king_white, int piece_white, int square_white, int king_black, int piece_black, int square_black) {
	return 64 * 6 * 64 * 64 * 6 * 64 * turn
		  + 6 * 64 * 64 * 6 * 64 * king_white
		      + 64 * 64 * 6 * 64 * piece_white
		           + 64 * 6 * 64 * square_white
			        + 6 * 64 * king_black
				    + 64 * piece_black
				         + square_black;
}

long bitbase_KXKX_index(const struct position *pos) {
	int king_white = ctz(pos->piece[WHITE][KING]);
	int king_black = ctz(pos->piece[BLACK][KING]);
	int piece_white = 0;
	int square_white = 0;
	int piece_black = 0;
	int square_black = 0;
	for (int piece = PAWN; piece < KING; piece++) {
		if (pos->piece[WHITE][piece]) {
			piece_white = piece;
			square_white = ctz(pos->piece[WHITE][piece]);
		}
		if (pos->piece[BLACK][piece]) {
			piece_black = piece;
			square_black = ctz(pos->piece[BLACK][piece]);
		}
	}
	return bitbase_KXKX_index_by_square(pos->turn, king_white, piece_white, square_white, king_black, piece_black, square_black);
}

unsigned bitbase_KXKX_probe_by_index(long index) {
	long lookup_index = index / BITBASE_KXKX_POSITIONS_PER_ENTRY;
	long bit_index = BITBASE_KXKX_BITS_PER_POSITION * (index % BITBASE_KXKX_POSITIONS_PER_ENTRY);
	return (bitbase_KXKX[lookup_index] >> bit_index) & BITBASE_KXKX_BITS_MASK;
}

unsigned bitbase_KXKX_probe(const struct position *pos) {
	return bitbase_KXKX_probe_by_index(bitbase_KXKX_index(pos));
}

void bitbase_KXKX_store_by_index(long index, unsigned eval) {
	long lookup_index = index / BITBASE_KXKX_POSITIONS_PER_ENTRY;
	long bit_index = BITBASE_KXKX_BITS_PER_POSITION * (index % BITBASE_KXKX_POSITIONS_PER_ENTRY);
	bitbase_KXKX[lookup_index] &= ~(BITBASE_KXKX_BITS_MASK << bit_index);
	bitbase_KXKX[lookup_index] |= (eval << bit_index);
}

void invalid_KXKX_store_by_index(long index) {
	long lookup_index = index / BITBASE_KXKX_BITS_PER_ENTRY;
	long bit_index = index % BITBASE_KXKX_BITS_PER_ENTRY;
	invalid_KXKX[lookup_index] |= (1 << bit_index);
}

unsigned invalid_KXKX_probe_by_index(long index) {
	long lookup_index = index / BITBASE_KXKX_BITS_PER_ENTRY;
	long bit_index = index % BITBASE_KXKX_BITS_PER_ENTRY;
	return (invalid_KXKX[lookup_index] >> bit_index) & 0x1;
}

static inline int legal_position(const struct position *pos, int king_white, int piece_white, int square_white, int king_black, int piece_black, int square_black) {
	UNUSED(king_white);
	UNUSED(king_black);
	if ((!piece_white && square_white) || (!piece_black && square_black))
		return 0;

	/* No pawns on rank 1 and 8. */
	if ((piece_white == PAWN && (square_white < 8 || square_white > 56)) || (piece_black == PAWN && (square_black < 8 || square_black > 56)))
		return 0;

	/* Makes sure that all 2 to 4 squares are distinct. */
	if (popcount(pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL]) != 2ull + (piece_white != 0) + (piece_black != 0))
		return 0;

	/* Not legal if we can capture enemy king. */
	if (generate_checkers(pos, !pos->turn) || distance(king_white, king_black) <= 1)
		return 0;

	return 1;
}

int mate(const struct position *pos) {
	move_t moves[MOVES_MAX];
	struct pstate pstate;
	pstate_init(pos, &pstate);
	movegen(pos, &pstate, moves, MOVETYPE_ALL);

	if (moves[0])
		return 0;

	return pstate.checkers != 0ull ? 2 : 1;
}

int main(void) {
	magicbitboard_init();
	attackgen_init();
	bitboard_init();

	struct position pos;
	bitbase_KXKX = calloc(BITBASE_KXKX_TABLE_SIZE, sizeof(*bitbase_KXKX));
	invalid_KXKX = calloc(BITBASE_KXKX_TABLE_SIZE / BITBASE_KXKX_BITS_PER_POSITION, sizeof(*bitbase_KXKX));

	long total = 0;
	long counter = 0;
	for (long index = 0; index < BITBASE_KXKX_INDEX_MAX; index++) {
		bitbase_KXKX_store_by_index(index, BITBASE_UNKNOWN);
		int turn = index / (64l * 6 * 64 * 64 * 6 * 64);
		int king_white = (index % (64 * 6 * 64 * 64 * 6 * 64)) / (6 * 64 * 64 * 6 * 64);
		int piece_white = (index % (6 * 64 * 64 * 6 * 64)) / (64 * 64 * 6 * 64);
		int square_white = (index % (64 * 64 * 6 * 64)) / (64 * 6 * 64);
		int king_black = (index % (64 * 6 * 64)) / (6 * 64);
		int piece_black = (index % (6 * 64)) / 64;
		int square_black = (index % (64)) / 1;

		memset(&pos, 0, sizeof(pos));
		pos.turn = turn;
		pos.piece[WHITE][KING] = bitboard(king_white);
		pos.mailbox[king_white] = WHITE_KING;
		pos.piece[BLACK][KING] = bitboard(king_black);
		pos.mailbox[king_black] = BLACK_KING;
		if (piece_white) {
			pos.piece[WHITE][piece_white] = bitboard(square_white);
			pos.mailbox[square_white] = piece_white;
		}
		if (piece_black) {
			pos.piece[BLACK][piece_black] = bitboard(square_black);
			pos.mailbox[square_black] = piece_black + 6;
		}
		pos.piece[WHITE][ALL] = pos.piece[WHITE][KING] | pos.piece[WHITE][piece_white];
		pos.piece[BLACK][ALL] = pos.piece[BLACK][KING] | pos.piece[BLACK][piece_black];

		if (!legal_position(&pos, king_white, piece_white, square_white, king_black, piece_black, square_black)) {
			total++;
			invalid_KXKX_store_by_index(index);
			continue;
		}

		int m = mate(&pos);
		if (m == 2 && pos.turn == BLACK) {
			counter++;
			bitbase_KXKX_store_by_index(index, BITBASE_WIN);
		}
		else if (m == 2 && pos.turn == WHITE) {
			counter++;
			bitbase_KXKX_store_by_index(index, BITBASE_LOSE);
		}
		else if (m == 1) {
			counter++;
			bitbase_KXKX_store_by_index(index, BITBASE_DRAW);
		}

	}
	printf("There are %ld total legal positions.\n", total);
	printf("There are %ld positions where a checkmate or stalemate occured.\n", counter);

	int iteration = 1;
	int changed = 1;
	while (changed) {
		changed = 0;
		total = 0;
		clock_t t = clock();
		for (long index = 0; index < BITBASE_KXKX_INDEX_MAX; index++) {
			if (invalid_KXKX_probe_by_index(index) || bitbase_KXKX_probe_by_index(index) != BITBASE_UNKNOWN)
				continue;

			int turn = index / (64l * 6 * 64 * 64 * 6 * 64);
			int king_white = (index % (64 * 6 * 64 * 64 * 6 * 64)) / (6 * 64 * 64 * 6 * 64);
			int piece_white = (index % (6 * 64 * 64 * 6 * 64)) / (64 * 64 * 6 * 64);
			int square_white = (index % (64 * 64 * 6 * 64)) / (64 * 6 * 64);
			int king_black = (index % (64 * 6 * 64)) / (6 * 64);
			int piece_black = (index % (6 * 64)) / 64;
			int square_black = (index % (64)) / 1;

			memset(&pos, 0, sizeof(pos));
			pos.turn = turn;
			pos.piece[WHITE][KING] = bitboard(king_white);
			pos.mailbox[king_white] = WHITE_KING;
			pos.piece[BLACK][KING] = bitboard(king_black);
			pos.mailbox[king_black] = BLACK_KING;
			if (piece_white) {
				pos.piece[WHITE][piece_white] = bitboard(square_white);
				pos.mailbox[square_white] = piece_white;
			}
			if (piece_black) {
				pos.piece[BLACK][piece_black] = bitboard(square_black);
				pos.mailbox[square_black] = piece_black + 6;
			}
			pos.piece[WHITE][ALL] = pos.piece[WHITE][KING] | pos.piece[WHITE][piece_white];
			pos.piece[BLACK][ALL] = pos.piece[BLACK][KING] | pos.piece[BLACK][piece_black];

			move_t moves[MOVES_MAX];
			movegen_legal(&pos, moves, MOVETYPE_ALL);

			int exists[4] = { 0 };
			for (move_t *ptr = moves; *ptr; ptr++) {
				do_move(&pos, ptr);
				unsigned p = bitbase_KXKX_probe(&pos);
				undo_move(&pos, ptr);
				exists[p] = 1;
				if (p == BITBASE_WIN && pos.turn == WHITE)
					break;
				else if (p == BITBASE_LOSE && pos.turn == BLACK)
					break;
			}

			if (exists[BITBASE_WIN] && pos.turn == WHITE) {
				bitbase_KXKX_store_by_index(index, BITBASE_WIN);
				counter++;
				total++;
				changed = 1;
			}
			else if (exists[BITBASE_LOSE] && pos.turn == BLACK) {
				bitbase_KXKX_store_by_index(index, BITBASE_LOSE);
				counter++;
				total++;
				changed = 1;
			}
			else if (!exists[BITBASE_UNKNOWN] && exists[BITBASE_DRAW]) {
				bitbase_KXKX_store_by_index(index, BITBASE_DRAW);
				counter++;
				total++;
				changed = 1;
			}
			else if (!exists[BITBASE_UNKNOWN]) {
				bitbase_KXKX_store_by_index(index, pos.turn == WHITE ? BITBASE_LOSE : BITBASE_WIN);
				counter++;
				total++;
				changed = 1;
			}
		}
		printf("Iteration %d took %ld seconds.\n", iteration++, (clock() - t) / CLOCKS_PER_SEC);
		printf("Stored %ld positions.\n", total);
		printf("There are now %ld stored positions.\n", counter);
	}

	/* Clean up. */
	for (long index = 0; index < BITBASE_KXKX_INDEX_MAX; index++)
		if (!invalid_KXKX_probe_by_index(index) && bitbase_KXKX_probe_by_index(index) == BITBASE_UNKNOWN)
			bitbase_KXKX_store_by_index(index, BITBASE_DRAW);

	/* KPK bitbase. */
	for (int turn = 0; turn < 2; turn++) {
		for (int king_white = 0; king_white < 64; king_white++) {
			for (int pawn_white = 8; pawn_white < 56; pawn_white++) {
				for (int king_black = 0; king_black < 64; king_black++) {
					memset(&pos, 0, sizeof(pos));
					pos.turn = turn;
					pos.piece[WHITE][KING] = bitboard(king_white);
					pos.piece[WHITE][PAWN] = bitboard(pawn_white);
					pos.piece[BLACK][KING] = bitboard(king_black);
					unsigned p = bitbase_KXKX_probe(&pos);
					bitbase_KPK_store(&pos, p);
				}
			}
		}
	}

	/* KPKP bitbase. */
	for (int turn = 0; turn < 2; turn++) {
		for (int king_white = 0; king_white < 64; king_white++) {
			for (int pawn_white = 8; pawn_white < 56; pawn_white++) {
				for (int king_black = 0; king_black < 64; king_black++) {
					for (int pawn_black = 8; pawn_black < 56; pawn_black++) {
						memset(&pos, 0, sizeof(pos));
						pos.turn = turn;
						pos.piece[WHITE][KING] = bitboard(king_white);
						pos.piece[WHITE][PAWN] = bitboard(pawn_white);
						pos.piece[BLACK][KING] = bitboard(king_black);
						pos.piece[BLACK][PAWN] = bitboard(pawn_black);
						unsigned p = bitbase_KXKX_probe(&pos);
						bitbase_KPKP_store(&pos, p);
					}
				}
			}
		}
	}

	/* KRKP bitbase. */
	for (int turn = 0; turn < 2; turn++) {
		for (int king_white = 0; king_white < 64; king_white++) {
			for (int rook_white = 0; rook_white < 64; rook_white++) {
				for (int king_black = 0; king_black < 64; king_black++) {
					for (int pawn_black = 8; pawn_black < 56; pawn_black++) {
						memset(&pos, 0, sizeof(pos));
						pos.turn = turn;
						pos.piece[WHITE][KING] = bitboard(king_white);
						pos.piece[WHITE][ROOK] = bitboard(rook_white);
						pos.piece[BLACK][KING] = bitboard(king_black);
						pos.piece[BLACK][PAWN] = bitboard(pawn_black);
						unsigned p = bitbase_KXKX_probe(&pos);
						bitbase_KRKP_store(&pos, p);
					}
				}
			}
		}
	}
	write_bitbase("kpk", bitbase_KPK, BITBASE_KPK_TABLE_SIZE);
	write_bitbase("kpkp", bitbase_KPKP, BITBASE_KPKP_TABLE_SIZE);
	write_bitbase("krkp", bitbase_KRKP, BITBASE_KRKP_TABLE_SIZE);

	free(bitbase_KXKX);
	free(invalid_KXKX);
}

void write_bitbase(char *name, uint32_t *bitbase, size_t table_size) {
	char path[BUFSIZ] = "files/";
	strcat(path, name);
	strcat(path, ".bin");

	FILE *f = fopen(path, "w");
	if (!f)
		fprintf(stderr, "Failed to open file %s.", path);

	for (size_t i = 0; i < table_size; i++) {
		if (i % 8)
			fprintf(f, " ");
		else if (i)
			fprintf(f, "\n");
		fprintf(f, "0x%08X,", bitbase[i]);
	}
	fprintf(f, "\n");

	fclose(f);
}
