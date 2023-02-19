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

#include <stdlib.h>

void move_next(struct position **p, struct history **h, move m) {
	struct history *t = *h;
	(*h) = malloc(sizeof(struct history));
	(*h)->previous = t;
	(*h)->move = malloc(sizeof(move));
	(*h)->pos = malloc(sizeof(struct position));
	copy_position((*h)->pos, *p);
	*((*h)->move) = m;
	do_move(*p, (*h)->move);
}

void move_previous(struct position **p, struct history **h) {
	if (!(*h))
		return;
	undo_move(*p, (*h)->move);
	struct history *t = *h;
	(*h) = (*h)->previous;
	free(t->move);
	free(t->pos);
	free(t);
}

void delete_history(struct history **h) {
	while (*h) {
		struct history *t = *h;
		(*h) = (*h)->previous;
		free(t->move);
		free(t->pos);
		free(t);
	}
}

