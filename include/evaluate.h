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

#ifndef EVALUATE_H
#define EVALUATE_H

#include <stdint.h>

#include "position.h"

typedef int32_t mevalue;

struct evaluationinfo {
	mevalue mobility[2];

	uint64_t mobility_squares[2];
	uint64_t pawn_attack_span[2];

	uint64_t attacked_squares[2][7];
	uint64_t attacked2_squares[2];

	uint64_t king_ring[2];
	int king_attack_units[2];
};

enum {
	pawn_mg   =   75, pawn_eg   =  125,
	knight_mg =  400, knight_eg =  425,
	bishop_mg =  425, bishop_eg =  450,
	rook_mg   =  650, rook_eg   =  700,
	queen_mg  = 1300, queen_eg  = 1350,
};

#define S(a, b) ((mevalue)((a) + ((uint32_t)(b) << 16)))

static inline int16_t mevalue_mg(mevalue eval) { return (int16_t)((uint32_t)eval & 0xFFFF); }
static inline int16_t mevalue_eg(mevalue eval) { return (int16_t)((uint32_t)(eval + 0x8000) >> 16); }

static inline mevalue new_mevalue(int16_t mgeval, int16_t egeval) {
	return (mevalue)(mgeval + ((uint32_t)egeval << 16));
}

static inline int16_t mevalue_evaluation(mevalue eval, double phase) {
	return phase * mevalue_mg(eval) + (1 - phase) * mevalue_eg(eval);
}

int16_t evaluate_classical(const struct position *pos);

void evaluate_print(const struct position *pos);

void evaluate_init(void);

#endif
