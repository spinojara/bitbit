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

#ifndef MOVEPICKER_H
#define MOVEPICKER_H

#include "position.h"
#include "move.h"
#include "search.h"
#include "transposition.h"

enum {
	STAGE_TT,
	STAGE_GENNONQUIET,
	STAGE_SORTNONQUIET,
	STAGE_GOODCAPTURE,
	STAGE_PROMOTION,
	STAGE_KILLER1,
	STAGE_KILLER2,
	STAGE_OKCAPTURE,
	STAGE_GENQUIET,
	STAGE_SORTQUIET,
	STAGE_QUIET,
	STAGE_BAD,
	STAGE_DONE,
};

struct movepicker {
	struct position *pos;
	const struct pstate *pstate;
	move_t movelist[MOVES_MAX], *move, *bad, *end;
	int64_t evallist[MOVES_MAX], *eval;
	int stage;
	int quiescence;

	move_t ttmove;
	move_t killer1, killer2;
	const struct searchinfo *si;
};

move_t next_move(struct movepicker *mp);

void movepicker_init(struct movepicker *mp, int quiescence, struct position *pos, const struct pstate *pstate, move_t ttmove, move_t killer1, move_t killer2, const struct searchinfo *si);

#endif
