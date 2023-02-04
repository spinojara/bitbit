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

#ifndef POSITION_H
#define POSITION_H

#include <stdint.h>

struct position {
	uint64_t piece[2][7];
	uint64_t piece_all;

	uint8_t turn;
	int8_t en_passant;
	uint8_t castle;

	uint16_t halfmove;
	uint16_t fullmove;

	uint8_t mailbox[64];

	uint64_t zobrist_key;
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

enum piece { all, pawn, knight, bishop, rook, queen, king };

enum color { black, white };

enum colored_piece { empty, white_pawn, white_knight, white_bishop, white_rook, white_queen, white_king, black_pawn, black_knight, black_bishop, black_rook, black_queen, black_king };

uint64_t generate_checkers(struct position *pos);
uint64_t generate_checkers_white(struct position *pos);
uint64_t generate_checkers_black(struct position *pos);
uint64_t generate_attacked(struct position *pos);
uint64_t generate_attacked_white(struct position *pos);
uint64_t generate_attacked_black(struct position *pos);
uint64_t generate_pinned(struct position *pos);
uint64_t generate_pinned_white(struct position *pos);
uint64_t generate_pinned_black(struct position *pos);

static inline void swap_turn(struct position *pos) {
	pos->turn = 1 - pos->turn;
}

int square(char *algebraic);

char *algebraic(char *str, int square);

char *castle_string(char *str, int castle);

void pos_from_fen(struct position *pos, int argc, char **argv);

void random_pos(struct position *pos, int n);

char *pos_to_fen(char *fen, struct position *pos);

int fen_is_ok(int argc, char **argv);

void print_position(struct position *pos, int flip);

int interactive_setpos(struct position *pos);

void fischer_pos(struct position *pos);

void copy_position(struct position *dest, struct position *src);

int pos_are_equal(struct position *pos1, struct position *pos2);

struct history;

void print_history_pgn(struct history *history);

int has_big_piece(struct position *pos);

#endif
