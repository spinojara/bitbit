/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2025 Isak Ellmer
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

#ifndef BITBASE_H
#define BITBASE_H

enum {
	BITBASE_DRAW    = 0,
	BITBASE_WIN     = 1,
	BITBASE_LOSE    = 2,
	BITBASE_UNKNOWN = 3,
	BITBASE_INVALID = 3,
};

static inline unsigned orient_bitbase_eval(int orient, unsigned p) {
	return orient ? ((p & 1) << 1) | ((p & 2) >> 1) : p;
}

#include "kpk.h"
#include "kpkp.h"
#include "krkp.h"

#endif
