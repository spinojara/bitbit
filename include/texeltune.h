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

#ifndef TEXELTUNE_H
#define TEXELTUNE_H

#include "evaluate.h"

struct trace {
	int mobility[2][4][28];
	int pawn_shelter[2][28];
	int unblocked_storm[2][28];
	int unblockable_storm[2][28];
	int blocked_storm[2][28];

	int king_on_open_file[2];
	int knight_outpost[2];
	int knight_outpost_attack[2];
	int bishop_outpost[2];
	int bishop_outpost_attack[2];
	int bishop_long_diagonal[2];
	int knight_behind_pawn[2];
	int bishop_behind_pawn[2];
	int defended_knight[2];
	int defended_bishop[2];
	int knight_far_from_king[2];
	int bishop_far_from_king[2];
	int knight_pair[2];
	int bishop_pair[2];
	int rook_pair[2];
	int pawn_blocking_bishop[2];
	int rook_open[2];
	int rook_semi[2];
	int rook_closed[2];
	int rook_blocked[2];
	int bad_queen[2];
	int king_attack_pawn[2];
	int king_defend_pawn[2];

	int pawn_threat[2];
	int push_threat[2];
	int minor_threat[2][7];
	int rook_threat[2][7];

	int weak_squares[2];
	int knight_attack[2];
	int bishop_attack[2];
	int rook_attack[2];
	int queen_attack[2];
	int discovery[2];
	int checks[2][12];
	int enemy_no_queen[2];

	int supported_pawn[2];
	int backward_pawn[2][4];
	int isolated_pawn[2][4];
	int doubled_pawn[2][4];
	int connected_pawn[2][7];
	int passed_pawn[2][7];
	int passed_blocked[2][7];
	int passed_file[2][4];
	int distance_us[2][7];
	int distance_them[2][7];

	int king_danger[2];
	int material;

	int eval;

	int p;
	int s;
};

extern struct trace trace;

extern mevalue king_on_open_file;
extern mevalue knight_outpost;
extern mevalue knight_outpost_attack;
extern mevalue bishop_outpost;
extern mevalue bishop_outpost_attack;
extern mevalue bishop_long_diagonal;
extern mevalue knight_behind_pawn;
extern mevalue bishop_behind_pawn;
extern mevalue defended_knight;
extern mevalue defended_bishop;
extern mevalue knight_far_from_king;
extern mevalue bishop_far_from_king;
extern mevalue knight_pair;
extern mevalue bishop_pair;
extern mevalue rook_pair;
extern mevalue pawn_blocking_bishop;
extern mevalue rook_open;
extern mevalue rook_semi;
extern mevalue rook_closed;
extern mevalue rook_blocked;
extern mevalue bad_queen;
extern mevalue king_attack_pawn;
extern mevalue king_defend_pawn;
extern mevalue tempo_bonus;

extern mevalue pawn_threat;
extern mevalue push_threat;
extern mevalue minor_threat[7];
extern mevalue rook_threat[7];

extern int weak_squares;
extern int enemy_no_queen;
extern int knight_attack;
extern int bishop_attack;
extern int rook_attack;
extern int queen_attack;
extern int discovery;
extern int checks[12];

extern int phase_max;
extern int phase_min;
extern int phase_knight;
extern int phase_bishop;
extern int phase_rook;
extern int phase_queen;

extern mevalue supported_pawn;
extern mevalue backward_pawn[4];
extern mevalue isolated_pawn[4];
extern mevalue doubled_pawn[4];
extern mevalue connected_pawn[7];
extern mevalue passed_pawn[7];
extern mevalue passed_blocked[7];
extern mevalue passed_file[4];
extern mevalue distance_us[7];
extern mevalue distance_them[7];

#ifdef TRACE
#undef TRACE
#define TRACE 1
#else
#define TRACE 0
#endif

#endif
