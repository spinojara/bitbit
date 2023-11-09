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

#include "evaluate.h"

#include <string.h>

#include "bitboard.h"
#include "util.h"
#include "attackgen.h"
#include "movegen.h"
#include "pawn.h"
#include "tables.h"
#include "nnue.h"
#include "option.h"
#include "texeltune.h"

CONST score_t king_on_open_file     = S( -8, -4);
CONST score_t knight_outpost        = S( 33, 13);
CONST score_t knight_outpost_attack = S( 17, 10);
CONST score_t bishop_outpost        = S( 32, -4);
CONST score_t bishop_outpost_attack = S( 15,  4);
CONST score_t bishop_long_diagonal  = S( 15, 10);
CONST score_t knight_behind_pawn    = S(  2,  7);
CONST score_t bishop_behind_pawn    = S(  3, -2);
CONST score_t defended_knight       = S(  6,  4);
CONST score_t defended_bishop       = S(  2, 20);
CONST score_t knight_far_from_king  = S( -7, -2);
CONST score_t bishop_far_from_king  = S( -5, -1);
CONST score_t knight_pair           = S( -5, 15);
CONST score_t bishop_pair           = S( 36, 66);
CONST score_t rook_pair             = S( 18,-48);
CONST score_t pawn_blocking_bishop  = S( -4, -7);
CONST score_t rook_open             = S( 40,  0);
CONST score_t rook_semi             = S( 16,  6);
CONST score_t rook_closed           = S(  4, -7);
CONST score_t rook_blocked          = S(-19,-17);
CONST score_t bad_queen             = S(-21, -6);
CONST score_t king_attack_pawn      = S( -5, 40);
CONST score_t king_defend_pawn      = S( -2, 17);
CONST score_t tempo_bonus           = S( 22, 18);

CONST score_t pawn_threat           = S( 66, 49);
CONST score_t push_threat           = S( 17,  7);
CONST score_t minor_threat[7]       = { S(  0,  0), S( -2, 10), S( 17, 36), S( 45, 49), S( 51, 39), S( 41, 58), };
CONST score_t rook_threat[7]        = { S(  0,  0), S(  7, 31), S( 31, 32), S( 36, 33), S( 16, 13), S( 55, 58), };

CONST int weak_squares              = 40;
CONST int enemy_no_queen            = 261;
CONST int knight_attack             = 46;
CONST int bishop_attack             = 35;
CONST int rook_attack               = 36;
CONST int queen_attack              = 30;
CONST int discovery                 = 288;
CONST int checks[12]                = { 0, 0, 0, 0, 165, 192, 81, 198, 164, 347, 111, 215, };

CONST int phase_max                 = 7808;
CONST int phase_min                 = 309;
CONST int phase_knight              = 289;
CONST int phase_bishop              = 366;
CONST int phase_rook                = 570;
CONST int phase_queen               = 1454;

const int material_value[7] = { 0, 100, 300, 300, 500, 1000, 0 };

static inline score_t evaluate_king_shelter(const struct position *pos, struct evaluationinfo *ei, int turn) {
	score_t eval = 0;

	uint64_t ourpawns = pos->piece[turn][pawn] &
		~passed_files(ei->king_square[turn], other_color(turn));
	uint64_t theirpawns = pos->piece[other_color(turn)][pawn] &
		~passed_files(ei->king_square[turn], other_color(turn));

	uint64_t b;
	int center = CLAMP(file_of(ei->king_square[turn]), 1, 6);
	int ourrank, theirrank;
	for (int f = center - 1; f <= center + 1; f++) {
		b = ourpawns & file(f);

		if (!b && f == file_of(ei->king_square[turn])) {
			eval += king_on_open_file;
			if (TRACE) trace.king_on_open_file[turn]++;
		}

		ourrank = b ? turn ? rank_of(clzm(b)) : rank_of(orient_horizontal(black, ctz(b))) : 0;

		b = theirpawns & file(f);
		theirrank = b ? turn ? rank_of(ctz(b)) : rank_of(orient_horizontal(black, clzm(b))) : 0;

		int d = MIN(f, 7 - f);
		eval += pawn_shelter[d * 7 + ourrank];
		if (TRACE) trace.pawn_shelter[turn][d * 7 + ourrank]++;
		if (ourrank && ourrank + 1 == theirrank) {
			eval += blocked_storm[d * 7 + ourrank];
			if (TRACE) trace.blocked_storm[turn][d * 7 + ourrank]++;
		}
		else if (ourrank && ourrank < theirrank) {
			eval += unblocked_storm[d * 7 + theirrank];
			if (TRACE) trace.unblocked_storm[turn][d * 7 + theirrank]++;
		}
		else {
			eval += unblockable_storm[d * 7 + theirrank];
			if (TRACE) trace.unblockable_storm[turn][d * 7 + theirrank]++;
		}
	}

	return eval;
}

static inline score_t evaluate_king(const struct position *pos, struct evaluationinfo *ei, int turn) {
	score_t eval = evaluate_king_shelter(pos, ei, turn);

	uint64_t weak = ei->attacked[other_color(turn)][all]
		      & ~(ei->attacked2[turn])
		      & (~ei->attacked[turn][all] | ei->attacked[turn][king] | ei->attacked[turn][queen]);

	/* All pieces that are not pawns. */
	uint64_t discoveries = ei->pinned[turn] & pos->piece[other_color(turn)][all] & ~pos->piece[other_color(turn)][pawn];

	int king_danger = knight_attack * ei->king_attacks[other_color(turn)][knight]
			+ bishop_attack * ei->king_attacks[other_color(turn)][bishop]
			+ rook_attack * ei->king_attacks[other_color(turn)][rook]
			+ queen_attack * ei->king_attacks[other_color(turn)][queen]
			+ weak_squares * popcount(weak & ei->king_ring[turn])
			+ discovery * popcount(discoveries)
			- enemy_no_queen * !pos->piece[other_color(turn)][queen];

	if (TRACE) trace.knight_attack[turn] = ei->king_attacks[other_color(turn)][knight];
	if (TRACE) trace.bishop_attack[turn] = ei->king_attacks[other_color(turn)][bishop];
	if (TRACE) trace.rook_attack[turn] = ei->king_attacks[other_color(turn)][rook];
	if (TRACE) trace.queen_attack[turn] = ei->king_attacks[other_color(turn)][queen];
	if (TRACE) trace.weak_squares[turn] = popcount(weak & ei->king_ring[turn]);
	if (TRACE) trace.discovery[turn] = popcount(discoveries);
	if (TRACE) trace.enemy_no_queen[turn] = -!pos->piece[other_color(turn)][queen];

	uint64_t possible_knight_checks = knight_attacks(ei->king_square[turn], 0);
	uint64_t possible_bishop_checks = bishop_attacks(ei->king_square[turn], 0, all_pieces(pos) ^ pos->piece[turn][queen]);
	uint64_t possible_rook_checks = rook_attacks(ei->king_square[turn], 0, all_pieces(pos) ^ pos->piece[turn][queen]);
	uint64_t very_safe = ~ei->attacked[turn][all];
	uint64_t safe = very_safe
		      | (ei->attacked2[other_color(turn)] & ~ei->attacked2[turn] & ~ei->attacked[turn][pawn]);

	uint64_t knight_checks = possible_knight_checks & safe & ei->attacked[other_color(turn)][knight];
	uint64_t bishop_checks = possible_bishop_checks & safe & ei->attacked[other_color(turn)][bishop];
	uint64_t rook_checks = possible_rook_checks & safe & ei->attacked[other_color(turn)][rook];
	uint64_t queen_checks = (possible_bishop_checks | possible_rook_checks) & safe & ei->attacked[other_color(turn)][queen];

	if (knight_checks) {
		king_danger += checks[2 * knight + (clear_ls1b(knight_checks) != 0)];
		if (TRACE) trace.checks[turn][2 * knight + (clear_ls1b(knight_checks) != 0)]++;
	}
	if (bishop_checks) {
		king_danger += checks[2 * bishop + (clear_ls1b(bishop_checks) != 0)];
		if (TRACE) trace.checks[turn][2 * bishop + (clear_ls1b(bishop_checks) != 0)]++;
	}
	if (rook_checks) {
		king_danger += checks[2 * rook + (clear_ls1b(rook_checks) != 0)];
		if (TRACE) trace.checks[turn][2 * rook + (clear_ls1b(rook_checks) != 0)]++;
	}
	if (queen_checks) {
		king_danger += checks[2 * queen + (clear_ls1b(queen_checks) != 0)];
		if (TRACE) trace.checks[turn][2 * queen + (clear_ls1b(queen_checks) != 0)]++;
	}

	if (TRACE) trace.king_danger[turn] = king_danger;
	if (king_danger > 0)
		eval -= S(king_danger * king_danger / 2048, king_danger / 8);

	if (pos->piece[turn][pawn] & king_attacks(ei->king_square[turn], 0)) {
		eval += king_defend_pawn;
		if (TRACE) trace.king_defend_pawn[turn]++;
	}

	if (pos->piece[other_color(turn)][pawn] & king_attacks(ei->king_square[turn], 0)) {
		eval += king_attack_pawn;
		if (TRACE) trace.king_attack_pawn[turn]++;
	}

	return eval;
}

static inline score_t evaluate_knights(const struct position *pos, struct evaluationinfo *ei, int turn) {
	score_t eval = 0;
	uint64_t b = pos->piece[turn][knight];
	/* Knight pair. */
	if (popcount(b) == 2) {
		eval += knight_pair;
		if (TRACE) trace.knight_pair[turn]++;
	}

	uint64_t outpost_squares = (turn ? (RANK_4 | RANK_5 | RANK_6) : (RANK_3 | RANK_4 | RANK_5))
				 & ~ei->pawn_attack_span[other_color(turn)] & ei->attacked[turn][pawn];

	while (b) {
		ei->material += phase_knight;
		ei->material_value[turn] += material_value[knight];

		int square = ctz(b);
		uint64_t squareb = bitboard(square);
		
		uint64_t attacks = ei->pinned[turn] & bitboard(square) ? 0 : knight_attacks(square, 0);

		ei->attacked2[turn] |= attacks & ei->attacked[turn][all];
		ei->attacked[turn][knight] |= attacks;
		ei->attacked[turn][all] |= attacks;
		/* Mobility (range of popcount is [0, 8]). */
		int m = popcount(attacks & ei->mobility[turn]);
		eval += mobility[knight - 2][m];
		if (TRACE) trace.mobility[turn][knight - 2][m]++;

		/* King attacks. */
		if (ei->king_ring[other_color(turn)] & attacks)
			ei->king_attacks[turn][knight] += popcount(ei->king_ring[other_color(turn)] & attacks) + 1;

		/* Outpost. */
		if (squareb & outpost_squares) {
			eval += knight_outpost;
			if (TRACE) trace.knight_outpost[turn]++;
		}
		else if (attacks & outpost_squares) {
			eval += knight_outpost_attack;
			if (TRACE) trace.knight_outpost_attack[turn]++;
		}
		
		/* Minor behind pawn. */
		if (squareb & shift_color(pos->piece[turn][pawn], other_color(turn))) {
			eval += knight_behind_pawn;
			if (TRACE) trace.knight_behind_pawn[turn]++;
		}

		/* Defended minor. */
		if (squareb & ei->attacked[turn][pawn]) {
			eval += defended_knight;
			if (TRACE) trace.defended_knight[turn]++;
		}
		
		/* Penalty if piece is far from own king. */
		eval += knight_far_from_king * distance(square, ei->king_square[turn]);
		if (TRACE) trace.knight_far_from_king[turn] += distance(square, ei->king_square[turn]);

		b = clear_ls1b(b);
	}
	return eval;
}

static inline score_t evaluate_bishops(const struct position *pos, struct evaluationinfo *ei, int turn) {
	score_t eval = 0;
	uint64_t b = pos->piece[turn][bishop];
	/* Bishop pair. */
	if (popcount(b) == 2) {
		eval += bishop_pair;
		if (TRACE) trace.bishop_pair[turn]++;
	}

	uint64_t outpost_squares = (turn ? (RANK_4 | RANK_5 | RANK_6) : (RANK_3 | RANK_4 | RANK_5))
				 & ~ei->pawn_attack_span[other_color(turn)] & ei->attacked[turn][pawn];

	uint64_t long_diagonal_squares = 0x1818000000;

	while (b) {
		ei->material += phase_bishop;
		ei->material_value[turn] += material_value[bishop];

		int square = ctz(b);
		uint64_t squareb = bitboard(square);
		
		/* Attacks, including x-rays through both sides' queens and own bishops. */
		uint64_t attacks = bishop_attacks(square, 0, all_pieces(pos) ^ pos->piece[white][queen] ^ pos->piece[black][queen] ^ pos->piece[turn][bishop]);
		if (ei->pinned[turn] & bitboard(square))
			attacks &= line(square, ei->king_square[turn]);

		ei->attacked2[turn] |= attacks & ei->attacked[turn][all];
		ei->attacked[turn][bishop] |= attacks;
		ei->attacked[turn][all] |= attacks;
		/* Mobility (range of popcount is [0, 13]). */
		int m = popcount(attacks & ei->mobility[turn]);
		eval += mobility[bishop - 2][m];
		if (TRACE) trace.mobility[turn][bishop - 2][m]++;

		/* King attacks. */
		if (ei->king_ring[other_color(turn)] & attacks)
			ei->king_attacks[turn][bishop] += popcount(ei->king_ring[other_color(turn)] & attacks) + 1;

		/* Outpost. */
		if (squareb & outpost_squares) {
			eval += bishop_outpost;
			if (TRACE) trace.bishop_outpost[turn]++;
		}
		else if (attacks & outpost_squares) {
			eval += bishop_outpost_attack;
			if (TRACE) trace.bishop_outpost_attack[turn]++;
		}

		/* Minor behind pawn. */
		if (squareb & shift_color(pos->piece[turn][pawn], other_color(turn))) {
			eval += bishop_behind_pawn;
			if (TRACE) trace.bishop_behind_pawn[turn]++;
		}

		/* Defended minor. */
		if (squareb & ei->attacked[turn][pawn]) {
			eval += defended_bishop;
			if (TRACE) trace.defended_bishop[turn]++;
		}

		/* Long diagonal. */
		if (clear_ls1b(attacks & long_diagonal_squares)) {
			eval += bishop_long_diagonal;
			if (TRACE) trace.bishop_long_diagonal[turn]++;
		}
		
		/* Penalty if piece is far from own king. */
		eval += bishop_far_from_king * distance(square, ei->king_square[turn]);
		if (TRACE) trace.bishop_far_from_king[turn] += distance(square, ei->king_square[turn]);

		/* Penalty for own pawns on same squares as bishop. */
		uint64_t pawns = same_colored_squares(square) & pos->piece[turn][pawn];
		uint64_t blocked_pawns = pawns & shift_color(all_pieces(pos), other_color(turn));
		eval += pawn_blocking_bishop * (popcount(pawns) + popcount(blocked_pawns));
		if (TRACE) trace.pawn_blocking_bishop[turn] += popcount(pawns) + popcount(blocked_pawns);

		b = clear_ls1b(b);
	}
	return eval;
}

static inline score_t evaluate_rooks(const struct position *pos, struct evaluationinfo *ei, int turn) {
	score_t eval = 0;
	uint64_t b = pos->piece[turn][rook];
	/* Rook pair. */
	if (popcount(b) == 2) {
		eval += rook_pair;
		if (TRACE) trace.rook_pair[turn]++;
	}

	while (b) {
		ei->material += phase_rook;
		ei->material_value[turn] += material_value[rook];

		int square = ctz(b);
		
		/* Attacks, including x-rays through both sides' queens and own rooks. */
		uint64_t attacks = rook_attacks(square, 0, all_pieces(pos) ^ pos->piece[white][queen] ^ pos->piece[black][queen] ^ pos->piece[turn][rook]);
		if (ei->pinned[turn] & bitboard(square))
			attacks &= line(square, ei->king_square[turn]);

		ei->attacked2[turn] |= attacks & ei->attacked[turn][all];
		ei->attacked[turn][rook] |= attacks;
		ei->attacked[turn][all] |= attacks;
		/* Mobility (range of popcount is [0, 14]). */
		int m = popcount(attacks & ei->mobility[turn]);
		eval += mobility[rook - 2][m];
		if (TRACE) trace.mobility[turn][rook - 2][m]++;

		/* King attacks. */
		if (ei->king_ring[other_color(turn)] & attacks)
			ei->king_attacks[turn][rook] += popcount(ei->king_ring[other_color(turn)] & attacks) + 1;

		/* Bonus on open files. */
		if (!((pos->piece[black][pawn] | pos->piece[white][pawn]) & file(square))) {
			eval += rook_open;
			if (TRACE) trace.rook_open[turn]++;
		}
		else if (!(pos->piece[turn][pawn] & file(square))) {
			eval += rook_semi;
			if (TRACE) trace.rook_semi[turn]++;
		}
		/* Penalty if blocked by uncastled king. */
		else {
			/* Behind blocked pawns. */
			if (shift_color(all_pieces(pos), other_color(turn)) & pos->piece[turn][pawn] & file(square)) {
				eval += rook_closed;
				if (TRACE) trace.rook_closed[turn]++;
			}

			if (m <= 3) {
				int kf = file_of(ei->king_square[turn]);
				if ((kf < e1) == (file_of(square) < kf)) {
					eval += rook_blocked * (1 + !(pos->castle & (turn ? 0x3 : 0xC)));
					if (TRACE) trace.rook_blocked[turn] += 1 + !(pos->castle & (turn ? 0x3 : 0xC));
				}
			}
		}
		/* We don't need a rook on seventh parameters since this is included
		 * in the psqt part.
		 */

		b = clear_ls1b(b);
	}
	return eval;
}

static inline score_t evaluate_queens(const struct position *pos, struct evaluationinfo *ei, int turn) {
	score_t eval = 0;
	uint64_t b = pos->piece[turn][queen];
	/* Only add the material value of 1 queen per side. */
	while (b) {
		ei->material += phase_queen;
		ei->material_value[turn] += material_value[queen];

		int square = ctz(b);
		
		uint64_t attacks = queen_attacks(square, 0, all_pieces(pos));
		if (ei->pinned[turn] & bitboard(square))
			attacks &= line(square, ei->king_square[turn]);
		ei->attacked2[turn] |= attacks & ei->attacked[turn][all];
		ei->attacked[turn][queen] |= attacks;
		ei->attacked[turn][all] |= attacks;
		/* Mobility (range of popcount is [0, 27]). */
		int m = popcount(attacks & ei->mobility[turn]);
		eval += mobility[queen - 2][m];
		if (TRACE) trace.mobility[turn][queen - 2][m]++;

		/* King attacks. */
		if (ei->king_ring[other_color(turn)] & attacks)
			ei->king_attacks[turn][queen] += popcount(ei->king_ring[other_color(turn)] & attacks) + 1;

		if (generate_blockers(pos, pos->piece[other_color(turn)][bishop] | pos->piece[other_color(turn)][rook], square)) {
			eval += bad_queen;
			if (TRACE) trace.bad_queen[turn]++;
		}

		b = clear_ls1b(b);
	}
	return eval;
}

static inline score_t evaluate_threats(const struct position *pos, struct evaluationinfo *ei, int turn) {
	score_t eval = 0;
	uint64_t ourpawns = pos->piece[turn][pawn];
	uint64_t theirnonpawns = pos->piece[other_color(turn)][all] ^ pos->piece[other_color(turn)][pawn];
	uint64_t b;
	/* Pawn threats. */
	b = ei->attacked[turn][pawn] & theirnonpawns;
	eval += popcount(b) * pawn_threat;
	if (TRACE) trace.pawn_threat[turn] += popcount(b);

	/* Pawn push threats. */
	b = shift_color(ourpawns, turn) & ~all_pieces(pos) & ~ei->attacked[other_color(turn)][pawn];
	b = shift_color(shift_west(b) | shift_east(b), turn) & (pos->piece[other_color(turn)][all] ^ pos->piece[other_color(turn)][pawn]);
	eval += popcount(b) * push_threat;
	if (TRACE) trace.push_threat[turn] += popcount(b);

	b = (ei->attacked[turn][knight] | ei->attacked[turn][bishop]) & pos->piece[other_color(turn)][all];
	while (b) {
		int square = ctz(b);
		eval += minor_threat[uncolored_piece(pos->mailbox[square])];
		if (TRACE) trace.minor_threat[turn][uncolored_piece(pos->mailbox[square])]++;
		b = clear_ls1b(b);
	}

	b = ei->attacked[turn][rook] & pos->piece[other_color(turn)][all]
	  & ~(ei->attacked[other_color(turn)][pawn] | (ei->attacked2[other_color(turn)] & ~ei->attacked2[turn]));
	while (b) {
		int square = ctz(b);
		eval += rook_threat[uncolored_piece(pos->mailbox[square])];
		if (TRACE) trace.rook_threat[turn][uncolored_piece(pos->mailbox[square])]++;
		b = clear_ls1b(b);
	}

	return eval;
}

static inline score_t evaluate_tempo(const struct position *pos, struct evaluationinfo *ei, int turn) {
	UNUSED(ei);
	return (pos->turn == turn) * tempo_bonus;
}

void evaluationinfo_init(const struct position *pos, struct evaluationinfo *ei) {
	ei->attacked[white][pawn] = shift_north_west(pos->piece[white][pawn]) | shift_north_east(pos->piece[white][pawn]);
	ei->attacked[black][pawn] = shift_south_west(pos->piece[black][pawn]) | shift_south_east(pos->piece[black][pawn]);

	ei->pawn_attack_span[white] = fill_north(ei->attacked[white][pawn]);
	ei->pawn_attack_span[black] = fill_south(ei->attacked[black][pawn]);

	for (int turn = 0; turn < 2; turn++) {
		int king_square = ctz(pos->piece[turn][king]);
		ei->king_square[turn] = king_square;
		int f = CLAMP(file_of(king_square), 1, 6);
		int r = CLAMP(rank_of(king_square), 1, 6);
		king_square = 8 * r + f;
		/* King ring... */
		ei->king_ring[turn] = king_attacks(king_square, 0) | bitboard(king_square);
		/* but not defended by two own pawns,
		 * if it is only defended by one pawn it could pinned.
		 */
		ei->king_ring[turn] &= ~(shift_color_west(pos->piece[turn][pawn], turn) & shift_color_east(pos->piece[turn][pawn], turn));
		ei->attacked[turn][king] = king_attacks(ctz(pos->piece[turn][king]), 0);
		ei->attacked[turn][all] = ei->attacked[turn][king] | ei->attacked[turn][pawn];
		ei->attacked2[turn] = ei->attacked[turn][king] & ei->attacked[turn][pawn];

		ei->pinned[turn] = generate_pinned(pos, turn);

		ei->mobility[turn] = ~pos->piece[turn][king]
		                           & ~ei->attacked[other_color(turn)][pawn]
		                           & ~(shift_color(all_pieces(pos), other_color(turn)) & pos->piece[turn][pawn])
					   & ~(ei->pinned[turn] & pos->piece[turn][all]);
	}
}

static inline score_t evaluate_psqtable(const struct position *pos, struct evaluationinfo *ei, int turn) {
	UNUSED(ei);
	score_t eval = 0;
	uint64_t bitboard;
	int square;
	for (int piece = pawn; piece <= king; piece++) {
		bitboard = pos->piece[turn][piece];
		while (bitboard) {
			square = ctz(bitboard);
			eval += psqtable[turn][piece][square];
			bitboard = clear_ls1b(bitboard);
		}
	}

	return eval;
}

static inline int32_t phase(const struct evaluationinfo *ei) {
	int32_t phase = CLAMP(ei->material, phase_min, phase_max);
	phase = (phase - phase_min) * PHASE / (phase_max - phase_min);
	return phase;
}

/* <https://ulthiel.com/math/other/endgames> */
static inline int32_t scale(const struct position *pos, const struct evaluationinfo *ei, int strong_side) {
	int weak_side = other_color(strong_side);
	int32_t scale = NORMAL_SCALE;
	int32_t strong_material = ei->material_value[strong_side];
	int32_t weak_material = ei->material_value[weak_side];
	/* Scores also KBBKN as a draw which is ok by the 50 move rule. */
	if (!pos->piece[strong_side][pawn] && strong_material - weak_material <= material_value[bishop]) {
		scale = (strong_material <= material_value[bishop]) ? 0 : (weak_material < material_value[rook]) ? 16 : 32;
	}
	return scale;
}

static inline int32_t evaluate_tapered(const struct position *pos, const struct evaluationinfo *ei) {
	if (TRACE) trace.eval = ei->eval;
	int32_t p = phase(ei);
	if (TRACE) trace.material = ei->material;
	if (TRACE) trace.p = p;
	int strong_side = score_eg(ei->eval) > 0;
	int32_t s = scale(pos, ei, strong_side);
	if (TRACE) trace.s = s;
	int32_t ret = p * score_mg(ei->eval) + (PHASE - p) * s * score_eg(ei->eval) / NORMAL_SCALE;
	return ret / PHASE;
}

int32_t evaluate_classical(const struct position *pos) {
	struct evaluationinfo ei = { 0 };
	evaluationinfo_init(pos, &ei);

	ei.eval  = evaluate_psqtable(pos, &ei, white) - evaluate_psqtable(pos, &ei, black);
	ei.eval += evaluate_pawns(pos, &ei, white)    - evaluate_pawns(pos, &ei, black);
	ei.eval += evaluate_knights(pos, &ei, white)  - evaluate_knights(pos, &ei, black);
	ei.eval += evaluate_bishops(pos, &ei, white)  - evaluate_bishops(pos, &ei, black);
	ei.eval += evaluate_rooks(pos, &ei, white)    - evaluate_rooks(pos, &ei, black);
	ei.eval += evaluate_queens(pos, &ei, white)   - evaluate_queens(pos, &ei, black);
	ei.eval += evaluate_king(pos, &ei, white)     - evaluate_king(pos, &ei, black);
	ei.eval += evaluate_threats(pos, &ei, white)  - evaluate_threats(pos, &ei, black);
	ei.eval += evaluate_tempo(pos, &ei, white)    - evaluate_tempo(pos, &ei, black);

	int32_t ret = evaluate_tapered(pos, &ei);
	return pos->turn ? ret : -ret;
}

void print_score_t(score_t eval);
score_t evaluate_print_x(const char *name, const struct position *pos, struct evaluationinfo *ei,
		score_t (*evaluate_x)(const struct position *, struct evaluationinfo *ei, int));

void evaluate_print(struct position *pos) {
	int old_option_nnue = option_nnue;
	option_nnue = 1;

	struct evaluationinfo ei = { 0 };
	evaluationinfo_init(pos, &ei);

	printf("+-------------+-------------+-------------+-------------+\n"
	       "| Term        |    White    |    Black    |    Total    |\n"
	       "|             |   MG    EG  |   MG    EG  |   MG    EG  |\n"
	       "+-------------+-------------+-------------+-------------+\n");
	ei.eval += evaluate_print_x("PSQT", pos, &ei, &evaluate_psqtable);
	ei.eval += evaluate_print_x("Knights", pos, &ei, &evaluate_knights);
	ei.eval += evaluate_print_x("Bishops", pos, &ei, &evaluate_bishops);
	ei.eval += evaluate_print_x("Rooks", pos, &ei, &evaluate_rooks);
	ei.eval += evaluate_print_x("Queens", pos, &ei, &evaluate_queens);
	ei.eval += evaluate_print_x("Pawns", pos, &ei, &evaluate_pawns);
	ei.eval += evaluate_print_x("King", pos, &ei, &evaluate_king);
	ei.eval += evaluate_print_x("Tempo", pos, &ei, &evaluate_tempo);
	printf("+-------------+-------------+-------------+-------------+\n");
	printf("| Total                                   | ");
	print_score_t(ei.eval);
	printf(" |\n");

	int32_t p = phase(&ei);
	int strong_side = score_mg(ei.eval) > 0;
	int32_t s = scale(pos, &ei, strong_side);

	int32_t scaledmg = score_mg(ei.eval);
	int32_t scaledeg = s * score_eg(ei.eval) / NORMAL_SCALE;
	printf("| Scaled  Where s = %.2f                  | ", (double)s / NORMAL_SCALE);
	print_score_t(S(scaledmg, scaledeg));
	printf(" |\n");
	printf("| Tapered Where p = %.2f                  | ", (double)p / PHASE);
	print_score_t(S(p * scaledmg / PHASE, (PHASE - p) * scaledeg / PHASE));
	printf(" |\n");
	printf("| Final                                   |       %+.2f |\n", (double)evaluate_tapered(pos, &ei) / 100);
	printf("+-------------+-------------+-------------+-------------+\n");
	
	char pieces[] = " PNBRQKpnbrqk";

	alignas(64) int16_t accumulation[2][K_HALF_DIMENSIONS];
	alignas(64) int32_t psqtaccumulation[2] = { 0 };
	printf("\n+-------+-------+-------+-------+-------+-------+-------+-------+\n");
	for (int r = 7; r >= 0; r--) {
		printf("|");
		for (int f = 0; f < 8; f++) {
			printf("   %c   |", pieces[pos->mailbox[make_square(f, r)]]);
		}
		printf("\n|");
		for (int f = 0; f < 8; f++) {
			int square = make_square(f, r);
			int piece = pos->mailbox[square];
			if (uncolored_piece(piece) && uncolored_piece(piece) != king) {
				int16_t oldeval = psqtaccumulation[white] - psqtaccumulation[black];
				for (int color = 0; color < 2; color++) {
					int king_square = ei.king_square[color];
					king_square = orient_horizontal(color, king_square);
					int index = make_index(color, square, piece, king_square);
					add_index_slow(index, accumulation, psqtaccumulation, color);
				}
				int16_t neweval = psqtaccumulation[white] - psqtaccumulation[black] - oldeval;
				
				if (ABS(neweval) >= 2000)
					printf(" %+.1f |", (double)neweval / 200);
				else
					printf(" %+.2f |", (double)neweval / 200);
			}
			else {
				printf("       |");
			}
		}
		printf("\n+-------+-------+-------+-------+-------+-------+-------+-------+\n");
	}
	printf("Psqt: %+.2f\n", (double)(psqtaccumulation[white] - psqtaccumulation[black]) / 200);
	printf("Positional %+.2f\n", (double)((2 * pos->turn - 1) * evaluate_nnue(pos) - (psqtaccumulation[white] - psqtaccumulation[black]) / 2) / 100);
	printf("\n");
	printf("Classical Evaluation: %+.2f\n", (double)(2 * pos->turn - 1) * evaluate_classical(pos) / 100);
	printf("NNUE Evaluation: %+.2f\n", (double)(2 * pos->turn - 1) * evaluate_nnue(pos) / 100);
	option_nnue = old_option_nnue;
}

void print_score_t(score_t eval) {
	int m = score_mg(eval);
	int e = score_eg(eval);
	if (ABS(m) >= 10000)
		printf("%+.0f ", (double)m / 100);
	else if (ABS(m) >= 1000)
		printf("%+.1f", (double)m / 100);
	else
		printf("%+.2f", (double)m / 100);
	printf(" ");
	if (ABS(e) >= 10000)
		printf("%+.0f ", (double)e / 100);
	else if (ABS(e) >= 1000)
		printf("%+.1f", (double)e / 100);
	else
		printf("%+.2f", (double)e / 100);
}

score_t evaluate_print_x(const char *name, const struct position *pos, struct evaluationinfo *ei,
		score_t (*evaluate_x)(const struct position *, struct evaluationinfo *ei, int)) {
	score_t w = evaluate_x(pos, ei, white);
	score_t b = evaluate_x(pos, ei, black);
	char namepadded[32];
	strcpy(namepadded, name);
	int len = strlen(name);
	int i;
	for (i = 0; i < 11 - len; i++)
		namepadded[len + i] = ' ';
	namepadded[len + i] = '\0';
	printf("| %s | ", namepadded);
	print_score_t(w);
	printf(" | ");
	print_score_t(b);
	printf(" | ");
	print_score_t(w - b);
	printf(" |\n");
	return w - b;
}
