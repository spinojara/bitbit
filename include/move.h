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

#ifndef MOVE_H
#define MOVE_H

#include <stdint.h>

#include "position.h"

/* 0-5 source square.
 * 6-11 target square.
 * 12-13 flag, 0: none, 1: en passant, 2: promotion, 3: castle.
 * 14-15 promotion piece, 0: knight, 1: bishop, 2: rook, 3: queen.
 * 16-18 capture, 0: no piece, 1: pawn, 2: knight, 3: bishop, 4: rook, 5: queen.
 * 19-23 available castles before move. First bit, 0: K, 1: Q, 2: k, 3: q.
 * 24-29 en passant square before move.
 * 30-36 halfmove.
 */
typedef uint64_t move;

static inline uint8_t move_from(const move *m) { return *m & 0x3F; }
static inline uint8_t move_to(const move *m) { return (*m >> 0x6) & 0x3F; }
static inline uint8_t move_flag(const move *m) { return (*m >> 0xC) & 0x3; }
static inline uint8_t move_promote(const move *m) { return (*m >> 0xE) & 0x3; }
static inline uint8_t move_capture(const move *m) { return (*m >> 0x10) & 0x7; }
static inline uint8_t move_castle(const move *m) { return (*m >> 0x13) & 0xF; }
static inline uint8_t move_en_passant(const move *m) { return (*m >> 0x18) & 0x3F; }
static inline uint16_t move_halfmove(const move *m) { return (*m >> 0x1E) & 0x7F; }
static inline void move_set_captured(move *m, uint64_t i) { *m |= (i << 0x10); }
static inline void move_set_castle(move *m, uint64_t i) { *m |= (i << 0x13); }
static inline void move_set_en_passant(move *m, uint64_t i) { *m |= (i << 0x18); }
static inline void move_set_halfmove(move *m, uint64_t i) { *m |= (i << 0x1E); }

#define MOVES_MAX 256

#define M(source_square, target_square, flag, promotion) ((source_square) | ((target_square) << 0x6) | ((flag) << 0xC) | ((promotion) << 0xE))

void do_move(struct position *pos, move *m);

void undo_move(struct position *pos, const move *m);

static inline move new_move(uint8_t source_square, uint8_t target_square, uint8_t flag, uint8_t promotion) {
	return source_square | (target_square << 0x6) | (flag << 0xC) | (promotion << 0xE);
}

static inline int is_capture(const struct position *pos, const move *m) {
	return pos->mailbox[move_to(m)];
}

void print_move(const move *m);

char *move_str_pgn(char *str, const struct position *pos, const move *m);
char *move_str_algebraic(char *str, const move *m);

move string_to_move(const struct position *pos, const char *str);

int is_legal(const struct position *pos, const move *m);

void do_null_move(struct position *pos, int en_passant);

#endif
