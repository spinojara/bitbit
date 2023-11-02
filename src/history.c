/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022 Isak Ellmer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * * This program is distributed in the hope that it will be useful, * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "history.h"

#include <string.h>

#include "transposition.h"

void history_next(struct position *pos, struct history *h, move_t m) {
	h->zobrist_key[h->ply] = pos->zobrist_key;
	h->move[h->ply] = m;
	do_zobrist_key(pos, h->move + h->ply);
	do_move(pos, h->move + h->ply);
	h->ply++;
}

void history_previous(struct position *pos, struct history *h) {
	h->ply--;
	undo_zobrist_key(pos, h->move + h->ply);
	undo_move(pos, h->move + h->ply);
}

void history_reset(const struct position *pos, struct history *h) {
	memset(h, 0, sizeof(*h));
	memcpy(&h->start, pos, sizeof(h->start));
}

int seldepth(const struct history *h) {
	int ply;
	for (ply = h->ply; ply < POSITIONS_MAX; ply++) {
		if (!h->zobrist_key[ply])
			break;
	}
	return ply - h->ply;
}

void reset_seldepth(struct history *h) {
	for (int ply = h->ply; ply < POSITIONS_MAX; ply++)
		h->zobrist_key[ply] = 0;
}
