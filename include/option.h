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

#ifndef OPTION_H
#define OPTION_H

#include "transposition.h"

extern int option_nnue;
extern int option_pure_nnue;
extern int option_transposition;
extern int option_history;
extern int option_endgame;
extern int option_damp;
extern int option_ponder;
extern int option_deterministic;

void print_options(void);

void setoption(int argc, char **argv, struct transpositiontable *tt);

#endif
