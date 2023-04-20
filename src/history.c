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

void history_next(struct position *pos, struct history *h, move m) {
	h->zobrist_key[h->index] = pos->zobrist_key;
	h->move[h->index] = m;
	do_move(pos, h->move + h->index);
	h->index++;
}

void history_previous(struct position *pos, struct history *h) {
	h->index--;
	undo_move(pos, h->move + h->index);
}

move *history_get_move(struct history *h) {
	return h->index ? h->move + (h->index - 1) : NULL;
}

void history_reset(struct position *pos, struct history *h) {
	h->index = 0;
	copy_position(&h->start, pos);
}
