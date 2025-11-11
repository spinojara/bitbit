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

#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <stdio.h>

#include "move.h"
#include "position.h"
#include "transposition.h"

enum {
	RESULT_LOSS = -1,
	RESULT_DRAW,
	RESULT_WIN,
	RESULT_UNKNOWN,
};

enum {
	FLAG_SKIP = 0x1,
};

int write_uintx(FILE *f, uint64_t p, size_t x);
int read_uintx(FILE *f, void *p, size_t x);

int write_position(FILE *f, const struct position *pos);
int read_position(FILE *f, struct position *pos);
int read_position_mem(const unsigned char *data, struct position *pos, size_t *index, size_t size);

int write_move(FILE *f, move_t move);
int read_move(FILE *f, move_t *move);
int read_move_mem(const unsigned char *data, move_t *move, size_t *index, size_t size);

int write_eval(FILE *f, int32_t eval);
int read_eval(FILE *f, int32_t *eval);
int read_eval_mem(const unsigned char *data, int32_t *eval, size_t *index, size_t size);

int write_result(FILE *f, char result);
int read_result(FILE *f, char *result);
int read_result_mem(const unsigned char *data, char *result, size_t *index, size_t size);

int write_flag(FILE *f, unsigned char flag);
int read_flag(FILE *f, unsigned char *flag);
int read_flag_mem(const unsigned char *data, unsigned char *flag, size_t *index, size_t size);

int write_tt(FILE *f, const struct transpositiontable *tt);
int read_tt(FILE *f, struct transpositiontable *tt);

#endif
