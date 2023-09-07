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

#ifndef HISTORY_H
#define HISTORY_H

#include "move.h"

#define POSITIONS_MAX 8192

struct history {
	move_t move[POSITIONS_MAX];
	int irreversible[POSITIONS_MAX];
	uint64_t zobrist_key[POSITIONS_MAX];
	struct position start;
	int ply;
};

void history_reset(const struct position *pos, struct history *h);

void history_next(struct position *pos, struct history *h, move_t m);

void history_previous(struct position *pos, struct history *h);

int seldepth(const struct history *h);

void reset_seldepth(struct history *h);

#endif
