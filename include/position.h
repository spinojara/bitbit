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

#ifndef POSITION_H
#define POSITION_H

#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <assert.h>

#define K_HALF_DIMENSIONS (256)

struct position {
	uint64_t piece[2][7];

	int turn;
	int en_passant;
	/* KQkq. */
	unsigned castle;

	int halfmove;
	int fullmove;

	int mailbox[64];

	uint64_t zobrist_key;
	uint64_t endgame_key;

	int16_t accumulation[2][K_HALF_DIMENSIONS];
	int32_t psqtaccumulation[2];
};

struct pstate {
	uint64_t checkers;
	uint64_t attacked[7];
	/* Set if checkers contains exactly one bit.
	 * checkray = between(ctz(checkers), king_square) | checkers;
	 * If checkers is not exactly one bit it is set to 0.
	 */
	uint64_t checkray;
	uint64_t pinned;
	uint64_t check_threats[7];
};

struct partialposition {
	uint64_t piece[2][7];

	int turn;
	int en_passant;
	/* KQkq. */
	unsigned castle;

	int halfmove;
	int fullmove;

	int mailbox[64];
};

enum square {
	a1, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8,
};

enum uncolored_piece { ALL, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

enum color { BLACK, WHITE };

enum colored_piece { EMPTY, WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING, BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING };

void pstate_init(const struct position *pos, struct pstate *pstate);

uint64_t generate_checkers(const struct position *pos, int color);
uint64_t generate_attackers(const struct position *pos, int square, int color);
void generate_attacked(const struct position *pos, int color, uint64_t attacked[7]);
uint64_t generate_pinned(const struct position *pos, int color);
uint64_t generate_blockers(const struct position *pos, uint64_t pinners, int king_square);
uint64_t generate_pinners(const struct position *pos, uint64_t pinned, int color);
void generate_check_threats(const struct position *pos, int color, uint64_t check_threats[7]);

static inline int other_color(int color) {
	return color ^ WHITE ^ BLACK;
}

static inline int orient_horizontal(int turn, int square) {
	return square ^ (turn ? 0x0 : 0x38);
}

static inline int orient_vertical(int orient, int square) {
	return square ^ (orient ? 0x7 : 0x0);
}

static inline uint64_t all_pieces(const struct position *pos) {
	return pos->piece[BLACK][ALL] | pos->piece[WHITE][ALL];
}

static inline int colored_piece(int piece, int color) {
	return piece + other_color(color) * KING;
}

static inline int uncolored_piece(int piece) {
	return piece - WHITE_KING * (piece > WHITE_KING);
}

static inline int color_of_piece(int piece) {
	return piece <= WHITE_KING;
}

static inline int rank_of(int square) {
	assert(0 <= square && square < 64);
	return square >> 3;
}

static inline int file_of(int square) {
	assert(0 <= square && square < 64);
	return square & 0x7;
}

static inline int make_square(int file, int rank) {
	return file + 8 * rank;
}

int square(const char *algebraic);

char *algebraic(char *str, int square);

char *castle_string(char *str, int castle);

void startpos(struct position *pos);
void pos_from_fen(struct position *pos, int argc, char **argv);

void mirror_position(struct position *pos);

void random_pos(struct position *pos, int n);

char *pos_to_fen(char *fen, const struct position *pos);

int fen_is_ok(int argc, char **argv);

void print_position(const struct position *pos);

int pos_are_equal(const struct position *pos1, const struct position *pos2);

struct history;

void print_history_pgn(const struct history *history);
void print_history_algebraic(const struct history *history, FILE *file);

int has_sliding_piece(const struct position *pos);

int is_repetition(const struct position *pos, const struct history *h, int ply, int count);

void position_init(void);

#endif
