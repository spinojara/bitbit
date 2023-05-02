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

#include "material.h"

#include "bitboard.h"

int material_draw(struct position *pos) {
	if (pos->piece[black][pawn] || pos->piece[white][pawn] ||
	    pos->piece[black][rook] || pos->piece[white][rook] ||
	    pos->piece[black][queen] || pos->piece[white][queen])
		return 0;

	int b[2] = { popcount(pos->piece[1 - pos->turn][bishop]), popcount(pos->piece[pos->turn][bishop]) };
	int n[2] = { popcount(pos->piece[1 - pos->turn][knight]), popcount(pos->piece[pos->turn][knight]) };

	/* only white perspective is allowed a draw since it is
	 * white's side to move and otherwise one of white's
	 * piece can be captured.
	 */

	/* if white has enough pieces we don't draw since we might
	 * capture a black piece on this move
	 */
	if (b[white] >= 2 || n[white] >= 3 || (b[white] && n[white]))
		return 0;

	/* white has exactly one bishop */
	if (b[white] == 1 && b[black] + n[black] <= 2)
		return 1;

	if (n[white] == 2 && b[black] <= 1 && n[black] <= 2)
		return 1;

	if (n[white] == 2 && b[black] <= 2 && !n[black])
		return 1;

	if (n[white] == 1 && b[black] <= 1 && n[black] <= 2 && b[black] + n[black] <= 2)
		return 1;

	return 0;
}
