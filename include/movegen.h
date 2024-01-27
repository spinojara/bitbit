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

#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "position.h"
#include "move.h"

enum {
	MOVETYPE_QUIET    = 0x1,
	MOVETYPE_NONQUIET = 0x2,
	MOVETYPE_ALL      = MOVETYPE_QUIET | MOVETYPE_NONQUIET,
	MOVETYPE_ESCAPE   = 0x4,
};

move_t *moves(const struct position *pos, const struct pstate *pstate, move_t *move, unsigned type);

move_t *generate_all(const struct position *pos, move_t *move_list);

int move_count(const move_t *m);

#endif
