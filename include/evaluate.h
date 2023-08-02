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

#define VALUE_NONE (0x7FFF)
#define VALUE_INFINITE (0x7FFE)
#define VALUE_MATE (0x7F00)
#define VALUE_WIN (0x1000)
#define VALUE_MATE_IN_MAX_PLY (VALUE_MATE - 128)

enum { mg, eg };

typedef int32_t mevalue;

extern mevalue king_on_open_file;
extern mevalue outpost_bonus;
extern mevalue outpost_attack;
extern mevalue minor_behind_pawn;
extern mevalue knight_far_from_king;
extern mevalue bishop_far_from_king;
extern mevalue bishop_pair;
extern mevalue pawn_on_bishop_square;
extern mevalue rook_on_open_file;
extern mevalue blocked_rook;
extern mevalue undeveloped_piece;
extern mevalue defended_minor;
extern mevalue tempo_bonus;

extern int weak_squares_danger;
extern int enemy_no_queen_bonus;
extern int knight_king_attack_danger;
extern int bishop_king_attack_danger;
extern int rook_king_attack_danger;
extern int queen_king_attack_danger;

extern int phase_max_material;
extern int phase_min_material;

extern const int material_value[7];

#define PHASE (256)
#define NORMAL_SCALE (256)

struct evaluationinfo {
	mevalue mobility[2];

	uint64_t mobility_squares[2];
	uint64_t pawn_attack_span[2];

	uint64_t attacked_squares[2][7];
	uint64_t attacked2_squares[2];

	uint64_t king_ring[2];
	int king_attack_units[2];

	int32_t material;
	int32_t material_value[2];
};

enum {
	pawn_mg   =   90, pawn_eg   =  125,
	knight_mg =  420, knight_eg =  351,
	bishop_mg =  450, bishop_eg =  352,
	rook_mg   =  576, rook_eg   =  597,
	queen_mg  = 1237, queen_eg  = 1077,
};


#define S(m, e) ((int32_t)((m) + ((uint32_t)(e) << 16)))

static inline int32_t mevalue_mg(mevalue eval) {
	union {
		uint16_t u;
		int16_t v;
	} ret = { .u = (uint16_t)eval };
	return (int32_t)ret.v;
}

static inline int32_t mevalue_eg(mevalue eval) {
	union {
		uint16_t u;
		int16_t v;
	} ret = { .u = (uint32_t)(eval + 0x8000) >> 16 };
	return (int32_t)ret.v;
}

int32_t evaluate_classical(const struct position *pos);

void evaluate_print(struct position *pos);

#endif
