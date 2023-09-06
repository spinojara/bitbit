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

#include "movepicker.h"

#include <assert.h>

#include "moveorder.h"
#include "bitboard.h"
#include "util.h"

static inline int good_capture(struct position *pos, move *m, int threshold) {
	return is_capture(pos, m) && see_geq(pos, m, threshold);
}

static inline int is_quiet(struct position *pos, move *m, int threshold) {
	UNUSED(threshold);
	return !pos->mailbox[move_to(m)];
}

int find_next(int index, struct position *pos, move *move_list, int (*filter)(struct position *, move *, int), int threshold) {
	for (int i = index; move_list[i]; i++) {
		if (filter(pos, &move_list[i], threshold)) {
			move m = move_list[i];
			move_list[i] = move_list[index];
			move_list[index] = m;
			return 1;
		}
	}
	return 0;
}

int place_first(int index, move *move_list, move m) {
	for (int i = index; move_list[i]; i++) {
		if (m == move_list[i]) {
			move_list[i] = move_list[index];
			move_list[index] = m;
			return 1;
		}
	}
	return 0;
}

void sort_moves(struct movepicker *mp) {
	if (!mp->move_list[mp->index])
		return;
	for (int i = 1 + mp->index; mp->move_list[i]; i++) {
		int64_t val = mp->evaluation_list[i];
		move m = mp->move_list[i];
		int j;
		for (j = i - 1; j >= mp->index && mp->evaluation_list[j] < val; j--) {
			mp->evaluation_list[j + 1] = mp->evaluation_list[j];
			mp->move_list[j + 1] = mp->move_list[j];
		}
		mp->evaluation_list[j + 1] = val;
		mp->move_list[j + 1] = m;
	}
}

void evaluate_moves(struct movepicker *mp) {
	for (int i = mp->index; mp->move_list[i]; i++) {
		move *ptr = &mp->move_list[i];
		int square_from = move_from(ptr);
		int square_to = move_to(ptr);
		int attacker = mp->pos->mailbox[square_from];
		int victim = mp->pos->mailbox[square_to];
		if (victim)
			mp->evaluation_list[i] = mvv_lva(attacker, victim);
		else
			mp->evaluation_list[i] = mp->si->history_moves[attacker][square_to];
	}
}

move next_move(struct movepicker *mp) {
	switch (mp->stage) {
	case STAGE_TT:
		mp->stage++;
		if (mp->ttmove && place_first(mp->index, mp->move_list, mp->ttmove))
			return mp->move_list[mp->index++];
		/* fallthrough */
	case STAGE_SORT:
		evaluate_moves(mp);
		sort_moves(mp);
		mp->stage++;
		/* fallthrough */
	case STAGE_GOODCAPTURE:
		if (find_next(mp->index, mp->pos, mp->move_list, &good_capture, 100))
			return mp->move_list[mp->index++];
		mp->stage++;
		/* fallthrough */
	case STAGE_KILLER1:
		mp->stage++;
		if (mp->killer1 && place_first(mp->index, mp->move_list, mp->killer1))
			return mp->move_list[mp->index++];
		/* fallthrough */
	case STAGE_KILLER2:
		mp->stage++;
		if (mp->killer2 && place_first(mp->index, mp->move_list, mp->killer2))
			return mp->move_list[mp->index++];
		/* fallthrough */
	case STAGE_OKCAPTURE:
		if (find_next(mp->index, mp->pos, mp->move_list, &good_capture, 0))
			return mp->move_list[mp->index++];
		mp->stage++;
		/* fallthrough */
	case STAGE_QUIET:
		if (find_next(mp->index, mp->pos, mp->move_list, &is_quiet, 0))
			return mp->move_list[mp->index++];
		mp->stage++;
		/* fallthrough */
	case STAGE_BADCAPTURE:
			return mp->move_list[mp->index++];
		mp->stage++;
		/* fallthrough */
	case STAGE_NONE:
		return 0;
	default:
		assert(0);
		return 0;
	}
}

void movepicker_init(struct movepicker *mp, struct position *pos, move *move_list, move ttmove, move killer1, move killer2, const struct searchinfo *si) {
	mp->pos = pos;
	mp->move_list = move_list;
	mp->si = si;

	mp->ttmove = ttmove;
	mp->killer1 = killer1;
	mp->killer2 = killer2;

	mp->stage = 0;
	mp->index = 0;
}
