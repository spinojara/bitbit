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

#ifndef MOVE_H
#define MOVE_H

#include <stdint.h>
#include <assert.h>

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
typedef uint64_t move_t;

static inline int move_from(const move_t *m) { return *m & 0x3F; }
static inline int move_to(const move_t *m) { return (*m >> 6) & 0x3F; }
static inline int move_flag(const move_t *m) { return (*m >> 12) & 0x3; }
static inline int move_promote(const move_t *m) { return (*m >> 14) & 0x3; }
static inline int move_capture(const move_t *m) { return (*m >> 16) & 0x7; }
static inline int move_castle(const move_t *m) { return (*m >> 19) & 0xF; }
static inline int move_en_passant(const move_t *m) { return (*m >> 24) & 0x3F; }
static inline int move_halfmove(const move_t *m) { return (*m >> 30) & 0x7F; }
static inline void move_set_captured(move_t *m, uint64_t i) { *m |= (i << 0x10); }
static inline void move_set_castle(move_t *m, uint64_t i) { *m |= (i << 0x13); }
static inline void move_set_en_passant(move_t *m, uint64_t i) { *m |= (i << 0x18); }
static inline void move_set_halfmove(move_t *m, uint64_t i) { *m |= (i << 0x1E); }

#define MOVES_MAX (256)
#define MOVE_EN_PASSANT (1)
#define MOVE_PROMOTION (2)
#define MOVE_CASTLE (3)

#define M(source_square, target_square, flag, promotion) ((source_square) | ((target_square) << 6) | ((flag) << 12) | ((promotion) << 14))

void do_move(struct position *pos, move_t *m);

void undo_move(struct position *pos, const move_t *m);

static inline move_t new_move(int source_square, int target_square, int flag, int promotion) {
	assert(0 <= source_square && source_square < 64);
	assert(0 <= target_square && target_square < 64);
	assert(0 <= flag && flag <= 3);
	assert(0 <= promotion && promotion <= 3);
	return source_square | (target_square << 6) | (flag << 12) | (promotion << 14);
}

static inline int is_capture(const struct position *pos, const move_t *m) {
	return pos->mailbox[move_to(m)];
}

void print_move(const move_t *m);

char *move_str_pgn(char *str, const struct position *pos, const move_t *m);
char *move_str_algebraic(char *str, const move_t *m);

move_t string_to_move(const struct position *pos, const char *str);

int is_legal(const struct position *pos, const move_t *m);

void do_null_move(struct position *pos, int en_passant);

#endif
