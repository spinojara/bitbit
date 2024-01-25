/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2024 Isak Ellmer
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

static inline score_t evaluate_king_shelter(const struct position *pos, struct evaluationinfo *ei, int us) {
	const int them = other_color(us);

	score_t eval = 0;

	uint64_t ourpawns = pos->piece[us][PAWN] &
		~passed_files(ei->king_square[us], them);
	uint64_t theirpawns = pos->piece[them][PAWN] &
		~passed_files(ei->king_square[us], them);

	uint64_t b;
	int center = clamp(file_of(ei->king_square[us]), 1, 6);
	int ourrank, theirrank;
	for (int f = center - 1; f <= center + 1; f++) {
		b = ourpawns & file(f);

		if (!b && f == file_of(ei->king_square[us])) {
			eval += king_on_open_file;
			if (TRACE) trace.king_on_open_file[us]++;
		}

		ourrank = b ? us ? rank_of(clzm(b)) : rank_of(orient_horizontal(BLACK, ctz(b))) : 0;

		b = theirpawns & file(f);
		theirrank = b ? us ? rank_of(ctz(b)) : rank_of(orient_horizontal(BLACK, clzm(b))) : 0;

		int d = min(f, 7 - f);
		eval += pawn_shelter[d * 7 + ourrank];
		if (TRACE) trace.pawn_shelter[us][d * 7 + ourrank]++;
		if (ourrank && ourrank + 1 == theirrank) {
			eval += blocked_storm[d * 7 + ourrank];
			if (TRACE) trace.blocked_storm[us][d * 7 + ourrank]++;
		}
		else if (ourrank && ourrank < theirrank) {
			eval += unblocked_storm[d * 7 + theirrank];
			if (TRACE) trace.unblocked_storm[us][d * 7 + theirrank]++;
		}
		else {
			eval += unblockable_storm[d * 7 + theirrank];
			if (TRACE) trace.unblockable_storm[us][d * 7 + theirrank]++;
		}
	}

	return eval;
}

static inline score_t evaluate_king(const struct position *pos, struct evaluationinfo *ei, int us) {
	const int them = other_color(us);
	score_t eval = evaluate_king_shelter(pos, ei, us);

	uint64_t weak = ei->attacked[them][ALL]
		      & ~(ei->attacked2[us])
		      & (~ei->attacked[us][ALL] | ei->attacked[us][KING] | ei->attacked[us][QUEEN]);

	/* All pieces that are not pawns. */
	uint64_t discoveries = ei->pinned[us] & pos->piece[them][ALL] & ~pos->piece[them][PAWN];

	int king_danger = knight_attack * ei->king_attacks[them][KNIGHT]
			+ bishop_attack * ei->king_attacks[them][BISHOP]
			+ rook_attack * ei->king_attacks[them][ROOK]
			+ queen_attack * ei->king_attacks[them][QUEEN]
			+ weak_squares * popcount(weak & ei->king_ring[us])
			+ discovery * popcount(discoveries)
			- enemy_no_queen * !pos->piece[them][QUEEN];

	if (TRACE) trace.knight_attack[us] = ei->king_attacks[them][KNIGHT];
	if (TRACE) trace.bishop_attack[us] = ei->king_attacks[them][BISHOP];
	if (TRACE) trace.rook_attack[us] = ei->king_attacks[them][ROOK];
	if (TRACE) trace.queen_attack[us] = ei->king_attacks[them][QUEEN];
	if (TRACE) trace.weak_squares[us] = popcount(weak & ei->king_ring[us]);
	if (TRACE) trace.discovery[us] = popcount(discoveries);
	if (TRACE) trace.enemy_no_queen[us] = -!pos->piece[them][QUEEN];

	uint64_t possible_knight_checks = knight_attacks(ei->king_square[us], 0);
	uint64_t possible_bishop_checks = bishop_attacks(ei->king_square[us], 0, all_pieces(pos) ^ pos->piece[us][QUEEN]);
	uint64_t possible_rook_checks = rook_attacks(ei->king_square[us], 0, all_pieces(pos) ^ pos->piece[us][QUEEN]);
	uint64_t very_safe = ~ei->attacked[us][ALL];
	uint64_t safe = very_safe
		      | (ei->attacked2[them] & ~ei->attacked2[us] & ~ei->attacked[us][PAWN]);

	uint64_t knight_checks = possible_knight_checks & safe & ei->attacked[them][KNIGHT];
	uint64_t bishop_checks = possible_bishop_checks & safe & ei->attacked[them][BISHOP];
	uint64_t rook_checks = possible_rook_checks & safe & ei->attacked[them][ROOK];
	uint64_t queen_checks = (possible_bishop_checks | possible_rook_checks) & safe & ei->attacked[them][QUEEN];

	if (knight_checks) {
		king_danger += checks[2 * KNIGHT + (clear_ls1b(knight_checks) != 0)];
		if (TRACE) trace.checks[us][2 * KNIGHT + (clear_ls1b(knight_checks) != 0)]++;
	}
	if (bishop_checks) {
		king_danger += checks[2 * BISHOP + (clear_ls1b(bishop_checks) != 0)];
		if (TRACE) trace.checks[us][2 * BISHOP + (clear_ls1b(bishop_checks) != 0)]++;
	}
	if (rook_checks) {
		king_danger += checks[2 * ROOK + (clear_ls1b(rook_checks) != 0)];
		if (TRACE) trace.checks[us][2 * ROOK + (clear_ls1b(rook_checks) != 0)]++;
	}
	if (queen_checks) {
		king_danger += checks[2 * QUEEN + (clear_ls1b(queen_checks) != 0)];
		if (TRACE) trace.checks[us][2 * QUEEN + (clear_ls1b(queen_checks) != 0)]++;
	}

	if (TRACE) trace.king_danger[us] = king_danger;
	if (king_danger > 0)
		eval -= S(king_danger * king_danger / 2048, king_danger / 8);

	if (pos->piece[us][PAWN] & king_attacks(ei->king_square[us], 0)) {
		eval += king_defend_pawn;
		if (TRACE) trace.king_defend_pawn[us]++;
	}

	if (pos->piece[them][PAWN] & king_attacks(ei->king_square[us], 0)) {
		eval += king_attack_pawn;
		if (TRACE) trace.king_attack_pawn[us]++;
	}

	return eval;
}

static inline score_t evaluate_knights(const struct position *pos, struct evaluationinfo *ei, int us) {
	const int them = other_color(us);
	int down = us ? S : N;

	score_t eval = 0;
	uint64_t b = pos->piece[us][KNIGHT];
	/* Knight pair. */
	if (popcount(b) == 2) {
		eval += knight_pair;
		if (TRACE) trace.knight_pair[us]++;
	}

	uint64_t outpost_squares = (us ? (RANK_4 | RANK_5 | RANK_6) : (RANK_3 | RANK_4 | RANK_5))
				 & ~ei->pawn_attack_span[them] & ei->attacked[us][PAWN];

	while (b) {
		ei->material += phase_knight;
		ei->material_value[us] += material_value[KNIGHT];

		int square = ctz(b);
		uint64_t squareb = bitboard(square);
		
		uint64_t attacks = ei->pinned[us] & bitboard(square) ? 0 : knight_attacks(square, 0);

		ei->attacked2[us] |= attacks & ei->attacked[us][ALL];
		ei->attacked[us][KNIGHT] |= attacks;
		ei->attacked[us][ALL] |= attacks;
		/* Mobility (range of popcount is [0, 8]). */
		int m = popcount(attacks & ei->mobility[us]);
		eval += mobility[KNIGHT - 2][m];
		if (TRACE) trace.mobility[us][KNIGHT - 2][m]++;

		/* King attacks. */
		if (ei->king_ring[them] & attacks)
			ei->king_attacks[us][KNIGHT] += popcount(ei->king_ring[them] & attacks) + 1;

		/* Outpost. */
		if (squareb & outpost_squares) {
			eval += knight_outpost;
			if (TRACE) trace.knight_outpost[us]++;
		}
		else if (attacks & outpost_squares) {
			eval += knight_outpost_attack;
			if (TRACE) trace.knight_outpost_attack[us]++;
		}
		
		/* Minor behind pawn. */
		if (squareb & shift(pos->piece[us][PAWN], down)) {
			eval += knight_behind_pawn;
			if (TRACE) trace.knight_behind_pawn[us]++;
		}

		/* Defended minor. */
		if (squareb & ei->attacked[us][PAWN]) {
			eval += defended_knight;
			if (TRACE) trace.defended_knight[us]++;
		}
		
		/* Penalty if piece is far from own king. */
		eval += knight_far_from_king * distance(square, ei->king_square[us]);
		if (TRACE) trace.knight_far_from_king[us] += distance(square, ei->king_square[us]);

		b = clear_ls1b(b);
	}
	return eval;
}

static inline score_t evaluate_bishops(const struct position *pos, struct evaluationinfo *ei, int us) {
	const int them = other_color(us);
	int down = us ? S : N;

	score_t eval = 0;
	uint64_t b = pos->piece[us][BISHOP];
	/* Bishop pair. */
	if (popcount(b) == 2) {
		eval += bishop_pair;
		if (TRACE) trace.bishop_pair[us]++;
	}

	uint64_t outpost_squares = (us ? (RANK_4 | RANK_5 | RANK_6) : (RANK_3 | RANK_4 | RANK_5))
				 & ~ei->pawn_attack_span[them] & ei->attacked[us][PAWN];

	uint64_t long_diagonal_squares = 0x1818000000;

	while (b) {
		ei->material += phase_bishop;
		ei->material_value[us] += material_value[BISHOP];

		int square = ctz(b);
		uint64_t squareb = bitboard(square);
		
		/* Attacks, including x-rays through both sides' queens and own bishops. */
		uint64_t attacks = bishop_attacks(square, 0, all_pieces(pos) ^ pos->piece[WHITE][QUEEN] ^ pos->piece[BLACK][QUEEN] ^ pos->piece[us][BISHOP]);
		if (ei->pinned[us] & bitboard(square))
			attacks &= line(square, ei->king_square[us]);

		ei->attacked2[us] |= attacks & ei->attacked[us][ALL];
		ei->attacked[us][BISHOP] |= attacks;
		ei->attacked[us][ALL] |= attacks;
		/* Mobility (range of popcount is [0, 13]). */
		int m = popcount(attacks & ei->mobility[us]);
		eval += mobility[BISHOP - 2][m];
		if (TRACE) trace.mobility[us][BISHOP - 2][m]++;

		/* King attacks. */
		if (ei->king_ring[them] & attacks)
			ei->king_attacks[us][BISHOP] += popcount(ei->king_ring[them] & attacks) + 1;

		/* Outpost. */
		if (squareb & outpost_squares) {
			eval += bishop_outpost;
			if (TRACE) trace.bishop_outpost[us]++;
		}
		else if (attacks & outpost_squares) {
			eval += bishop_outpost_attack;
			if (TRACE) trace.bishop_outpost_attack[us]++;
		}

		/* Minor behind pawn. */
		if (squareb & shift(pos->piece[us][PAWN], down)) {
			eval += bishop_behind_pawn;
			if (TRACE) trace.bishop_behind_pawn[us]++;
		}

		/* Defended minor. */
		if (squareb & ei->attacked[us][PAWN]) {
			eval += defended_bishop;
			if (TRACE) trace.defended_bishop[us]++;
		}

		/* Long diagonal. */
		if (clear_ls1b(attacks & long_diagonal_squares)) {
			eval += bishop_long_diagonal;
			if (TRACE) trace.bishop_long_diagonal[us]++;
		}
		
		/* Penalty if piece is far from own king. */
		eval += bishop_far_from_king * distance(square, ei->king_square[us]);
		if (TRACE) trace.bishop_far_from_king[us] += distance(square, ei->king_square[us]);

		/* Penalty for own pawns on same squares as bishop. */
		uint64_t pawns = same_colored_squares(square) & pos->piece[us][PAWN];
		uint64_t blocked_pawns = pawns & shift(all_pieces(pos), down);
		eval += pawn_blocking_bishop * (popcount(pawns) + popcount(blocked_pawns));
		if (TRACE) trace.pawn_blocking_bishop[us] += popcount(pawns) + popcount(blocked_pawns);

		b = clear_ls1b(b);
	}
	return eval;
}

static inline score_t evaluate_rooks(const struct position *pos, struct evaluationinfo *ei, int us) {
	const int them = other_color(us);
	int down = us ? S : N;

	score_t eval = 0;
	uint64_t b = pos->piece[us][ROOK];
	/* Rook pair. */
	if (popcount(b) == 2) {
		eval += rook_pair;
		if (TRACE) trace.rook_pair[us]++;
	}

	while (b) {
		ei->material += phase_rook;
		ei->material_value[us] += material_value[ROOK];

		int square = ctz(b);
		
		/* Attacks, including x-rays through both sides' queens and own rooks. */
		uint64_t attacks = rook_attacks(square, 0, all_pieces(pos) ^ pos->piece[WHITE][QUEEN] ^ pos->piece[BLACK][QUEEN] ^ pos->piece[us][ROOK]);
		if (ei->pinned[us] & bitboard(square))
			attacks &= line(square, ei->king_square[us]);

		ei->attacked2[us] |= attacks & ei->attacked[us][ALL];
		ei->attacked[us][ROOK] |= attacks;
		ei->attacked[us][ALL] |= attacks;
		/* Mobility (range of popcount is [0, 14]). */
		int m = popcount(attacks & ei->mobility[us]);
		eval += mobility[ROOK - 2][m];
		if (TRACE) trace.mobility[us][ROOK - 2][m]++;

		/* King attacks. */
		if (ei->king_ring[them] & attacks)
			ei->king_attacks[us][ROOK] += popcount(ei->king_ring[them] & attacks) + 1;

		/* Bonus on open files. */
		if (!((pos->piece[BLACK][PAWN] | pos->piece[WHITE][PAWN]) & file(square))) {
			eval += rook_open;
			if (TRACE) trace.rook_open[us]++;
		}
		else if (!(pos->piece[us][PAWN] & file(square))) {
			eval += rook_semi;
			if (TRACE) trace.rook_semi[us]++;
		}
		/* Penalty if blocked by uncastled king. */
		else {
			/* Behind blocked pawns. */
			if (shift(all_pieces(pos), down) & pos->piece[us][PAWN] & file(square)) {
				eval += rook_closed;
				if (TRACE) trace.rook_closed[us]++;
			}

			if (m <= 3) {
				int kf = file_of(ei->king_square[us]);
				if ((kf < e1) == (file_of(square) < kf)) {
					eval += rook_blocked * (1 + !(pos->castle & (us ? 0x3 : 0xC)));
					if (TRACE) trace.rook_blocked[us] += 1 + !(pos->castle & (us ? 0x3 : 0xC));
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

static inline score_t evaluate_queens(const struct position *pos, struct evaluationinfo *ei, int us) {
	const int them = other_color(us);

	score_t eval = 0;
	uint64_t b = pos->piece[us][QUEEN];
	/* Only add the material value of 1 queen per side. */
	while (b) {
		ei->material += phase_queen;
		ei->material_value[us] += material_value[QUEEN];

		int square = ctz(b);
		
		uint64_t attacks = queen_attacks(square, 0, all_pieces(pos));
		if (ei->pinned[us] & bitboard(square))
			attacks &= line(square, ei->king_square[us]);
		ei->attacked2[us] |= attacks & ei->attacked[us][ALL];
		ei->attacked[us][QUEEN] |= attacks;
		ei->attacked[us][ALL] |= attacks;
		/* Mobility (range of popcount is [0, 27]). */
		int m = popcount(attacks & ei->mobility[us]);
		eval += mobility[QUEEN - 2][m];
		if (TRACE) trace.mobility[us][QUEEN - 2][m]++;

		/* King attacks. */
		if (ei->king_ring[them] & attacks)
			ei->king_attacks[us][QUEEN] += popcount(ei->king_ring[them] & attacks) + 1;

		if (generate_blockers(pos, pos->piece[them][BISHOP] | pos->piece[them][ROOK], square)) {
			eval += bad_queen;
			if (TRACE) trace.bad_queen[us]++;
		}

		b = clear_ls1b(b);
	}
	return eval;
}

static inline score_t evaluate_threats(const struct position *pos, struct evaluationinfo *ei, int us) {
	const int them = other_color(us);
	int up = us ? N : S;

	score_t eval = 0;
	uint64_t ourpawns = pos->piece[us][PAWN];
	uint64_t theirnonpawns = pos->piece[them][ALL] ^ pos->piece[them][PAWN];
	uint64_t b;
	/* Pawn threats. */
	b = ei->attacked[us][PAWN] & theirnonpawns;
	eval += popcount(b) * pawn_threat;
	if (TRACE) trace.pawn_threat[us] += popcount(b);

	/* Pawn push threats. */
	b = shift(ourpawns, up) & ~all_pieces(pos) & ~ei->attacked[them][PAWN];
	b = shift(shift(b, E) | shift(b, W), up) & (pos->piece[them][ALL] ^ pos->piece[them][PAWN]);
	eval += popcount(b) * push_threat;
	if (TRACE) trace.push_threat[us] += popcount(b);

	b = (ei->attacked[us][KNIGHT] | ei->attacked[us][BISHOP]) & pos->piece[them][ALL];
	while (b) {
		int square = ctz(b);
		eval += minor_threat[uncolored_piece(pos->mailbox[square])];
		if (TRACE) trace.minor_threat[us][uncolored_piece(pos->mailbox[square])]++;
		b = clear_ls1b(b);
	}

	b = ei->attacked[us][ROOK] & pos->piece[them][ALL]
	  & ~(ei->attacked[them][PAWN] | (ei->attacked2[them] & ~ei->attacked2[us]));
	while (b) {
		int square = ctz(b);
		eval += rook_threat[uncolored_piece(pos->mailbox[square])];
		if (TRACE) trace.rook_threat[us][uncolored_piece(pos->mailbox[square])]++;
		b = clear_ls1b(b);
	}

	return eval;
}

static inline score_t evaluate_tempo(const struct position *pos, struct evaluationinfo *ei, int us) {
	UNUSED(ei);
	return (pos->turn == us) * tempo_bonus;
}

void evaluationinfo_init(const struct position *pos, struct evaluationinfo *ei) {
	ei->attacked[WHITE][PAWN] = shift(pos->piece[WHITE][PAWN], N | E) | shift(pos->piece[WHITE][PAWN], N | W);
	ei->attacked[BLACK][PAWN] = shift(pos->piece[BLACK][PAWN], S | E) | shift(pos->piece[BLACK][PAWN], S | W);

	ei->pawn_attack_span[WHITE] = fill(ei->attacked[WHITE][PAWN], N);
	ei->pawn_attack_span[BLACK] = fill(ei->attacked[BLACK][PAWN], S);

	for (int us = 0; us < 2; us++) {
		int up = us ? N : S;
		int down = us ? S : N;
		const int them = other_color(us);
		int king_square = ctz(pos->piece[us][KING]);
		ei->king_square[us] = king_square;
		int f = clamp(file_of(king_square), 1, 6);
		int r = clamp(rank_of(king_square), 1, 6);
		king_square = 8 * r + f;
		/* King ring... */
		ei->king_ring[us] = king_attacks(king_square, 0) | bitboard(king_square);
		/* but not defended by two own pawns,
		 * if it is only defended by one pawn it could pinned.
		 */
		ei->king_ring[us] &= ~(shift(pos->piece[us][PAWN], up | E) & shift(pos->piece[us][PAWN], up | W));
		ei->attacked[us][KING] = king_attacks(ctz(pos->piece[us][KING]), 0);
		ei->attacked[us][ALL] = ei->attacked[us][KING] | ei->attacked[us][PAWN];
		ei->attacked2[us] = ei->attacked[us][KING] & ei->attacked[us][PAWN];

		ei->pinned[us] = generate_pinned(pos, us);

		ei->mobility[us] = ~pos->piece[us][KING]
		                           & ~ei->attacked[them][PAWN]
		                           & ~(shift(all_pieces(pos), down) & pos->piece[us][PAWN])
					   & ~(ei->pinned[us] & pos->piece[us][ALL]);
	}
}

static inline score_t evaluate_psqtable(const struct position *pos, struct evaluationinfo *ei, int us) {
	UNUSED(ei);
	score_t eval = 0;
	uint64_t bitboard;
	int square;
	for (int piece = PAWN; piece <= KING; piece++) {
		bitboard = pos->piece[us][piece];
		while (bitboard) {
			square = ctz(bitboard);
			eval += psqtable[us][piece][square];
			bitboard = clear_ls1b(bitboard);
		}
	}

	return eval;
}

static inline int32_t phase(const struct evaluationinfo *ei) {
	int32_t phase = clamp(ei->material, phase_min, phase_max);
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
	if (!pos->piece[strong_side][PAWN] && strong_material - weak_material <= material_value[BISHOP]) {
		scale = (strong_material <= material_value[BISHOP]) ? 0 : (weak_material < material_value[ROOK]) ? 16 : 32;
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

	ei.eval  = evaluate_psqtable(pos, &ei, WHITE) - evaluate_psqtable(pos, &ei, BLACK);
	ei.eval += evaluate_pawns(pos, &ei, WHITE)    - evaluate_pawns(pos, &ei, BLACK);
	ei.eval += evaluate_knights(pos, &ei, WHITE)  - evaluate_knights(pos, &ei, BLACK);
	ei.eval += evaluate_bishops(pos, &ei, WHITE)  - evaluate_bishops(pos, &ei, BLACK);
	ei.eval += evaluate_rooks(pos, &ei, WHITE)    - evaluate_rooks(pos, &ei, BLACK);
	ei.eval += evaluate_queens(pos, &ei, WHITE)   - evaluate_queens(pos, &ei, BLACK);
	ei.eval += evaluate_king(pos, &ei, WHITE)     - evaluate_king(pos, &ei, BLACK);
	ei.eval += evaluate_threats(pos, &ei, WHITE)  - evaluate_threats(pos, &ei, BLACK);
	ei.eval += evaluate_tempo(pos, &ei, WHITE)    - evaluate_tempo(pos, &ei, BLACK);

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
			if (uncolored_piece(piece) && uncolored_piece(piece) != KING) {
				int16_t oldeval = psqtaccumulation[WHITE] - psqtaccumulation[BLACK];
				for (int color = 0; color < 2; color++) {
					int king_square = ei.king_square[color];
					king_square = orient_horizontal(color, king_square);
					int index = make_index(color, square, piece, king_square);
					add_index_slow(index, accumulation, psqtaccumulation, color);
				}
				int16_t neweval = psqtaccumulation[WHITE] - psqtaccumulation[BLACK] - oldeval;
				
				if (abs(neweval) >= 2000)
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
	printf("Psqt: %+.2f\n", (double)(psqtaccumulation[WHITE] - psqtaccumulation[BLACK]) / 200);
	printf("Positional %+.2f\n", (double)((2 * pos->turn - 1) * evaluate_nnue(pos) - (psqtaccumulation[WHITE] - psqtaccumulation[BLACK]) / 2) / 100);
	printf("\n");
	printf("Classical Evaluation: %+.2f\n", (double)(2 * pos->turn - 1) * evaluate_classical(pos) / 100);
	printf("NNUE Evaluation: %+.2f\n", (double)(2 * pos->turn - 1) * evaluate_nnue(pos) / 100);
	option_nnue = old_option_nnue;
}

void print_score_t(score_t eval) {
	int m = score_mg(eval);
	int e = score_eg(eval);
	if (abs(m) >= 10000)
		printf("%+.0f ", (double)m / 100);
	else if (abs(m) >= 1000)
		printf("%+.1f", (double)m / 100);
	else
		printf("%+.2f", (double)m / 100);
	printf(" ");
	if (abs(e) >= 10000)
		printf("%+.0f ", (double)e / 100);
	else if (abs(e) >= 1000)
		printf("%+.1f", (double)e / 100);
	else
		printf("%+.2f", (double)e / 100);
}

score_t evaluate_print_x(const char *name, const struct position *pos, struct evaluationinfo *ei,
		score_t (*evaluate_x)(const struct position *, struct evaluationinfo *ei, int)) {
	score_t w = evaluate_x(pos, ei, WHITE);
	score_t b = evaluate_x(pos, ei, BLACK);
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
