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

#ifndef ENDGAME_H
#define ENDGAME_H

#include <stdint.h>

#include "position.h"
#include "bitboard.h"
#include "move.h"
#include "evaluate.h"
#include "option.h"

#define ENDGAMEBITS (8)
#define ENDGAMESIZE (1 << ENDGAMEBITS)
#define ENDGAMEINDEX (ENDGAMESIZE - 1)

struct endgame {
	uint64_t endgame_key;
	int32_t (*evaluate)(const struct position *pos, int strong_side);
	uint8_t strong_side;
};

extern struct endgame endgame_table[ENDGAMESIZE];
extern struct endgame endgame_KXK[2];

int is_KXK(const struct position *pos, int color);

static inline struct endgame *endgame_get(const struct position *pos) {
	return &endgame_table[pos->endgame_key & ENDGAMEINDEX];
}

static inline struct endgame *endgame_probe(const struct position *pos) {
	if (!option_endgame)
		return NULL;
	/* Check if pos qualifies for KXK. */
	for (int color = 0; color < 2; color++)
		if (is_KXK(pos, color))
			return &endgame_KXK[color];

	struct endgame *e = endgame_get(pos);
	return e->endgame_key == pos->endgame_key ? e : NULL;
}

/* If pos->halfmove is too large we should probably return 0
 * because we don't have enough time to force a checkmate.
 */
static inline int32_t endgame_evaluate(const struct endgame *e, const struct position *pos) {
	int32_t eval = e->evaluate(pos, e->strong_side);
	return eval == VALUE_NONE ? VALUE_NONE : pos->turn == e->strong_side ? eval : -eval;
}

void refresh_endgame_key(struct position *pos);

void do_endgame_key(struct position *pos, const move_t *m);
void undo_endgame_key(struct position *pos, const move_t *m);

void endgame_init(void);

#endif
