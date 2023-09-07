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

#ifndef MOVEPICKER_H
#define MOVEPICKER_H

#include "position.h"
#include "move.h"
#include "search.h"
#include "transposition.h"

enum {
	STAGE_TT,
	STAGE_SORT,
	STAGE_GOODCAPTURE,
	STAGE_KILLER1,
	STAGE_KILLER2,
	STAGE_OKCAPTURE,
	STAGE_QUIET,
	STAGE_BADCAPTURE,
	STAGE_NONE,
};

struct movepicker {
	struct position *pos;
	move_t *move_list;
	int64_t evaluation_list[MOVES_MAX];
	int stage;
	int index;

	move_t ttmove;
	move_t killer1, killer2;
	const struct searchinfo *si;
};

move_t next_move(struct movepicker *mp);

void movepicker_init(struct movepicker *mp, struct position *pos, move_t *move_list, move_t ttmove, move_t killer1, move_t killer2, const struct searchinfo *si);

#endif
