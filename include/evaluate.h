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

#define VALUE_INFINITE (0x7FFF)
#define VALUE_MATE (0x7F00)
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

extern int weak_squares_danger;
extern int enemy_no_queen_bonus;
extern int knight_king_attack_danger;
extern int bishop_king_attack_danger;
extern int rook_king_attack_danger;
extern int queen_king_attack_danger;

extern int tempo_bonus;

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
	pawn_mg   =   80, pawn_eg   =  118,
	knight_mg =  405, knight_eg =  370,
	bishop_mg =  436, bishop_eg =  383,
	rook_mg   =  565, rook_eg   =  649,
	queen_mg  = 1289, queen_eg  = 1254,
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

#endif
