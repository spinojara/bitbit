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

#include "movegen.h"

#include "bitboard.h"
#include "attackgen.h"

int move_count(const move_t *moves) {
	const move_t *move;
	for (move = moves; *move; move++);
	return move - moves;
}

static inline move_t *movegen_pawn(const struct position *pos, const struct pstate *pstate, move_t *moves, uint64_t targets, unsigned type) {
	const int us = pos->turn;
	const int them = other_color(us);
	const unsigned down = us ? S : N;
	const int pawn_sign = us ? 8 : -8;

	int square;
	uint64_t pawns = pos->piece[us][PAWN] & ~(us ? RANK_7 : RANK_2);
	uint64_t pawns7 = pos->piece[us][PAWN] ^ pawns;
	uint64_t enemies = pos->piece[them][ALL];
	uint64_t all = all_pieces(pos);

	/* Add en passant square and promotions to
	 * targets.
	 */
	if (type & MOVETYPE_NONQUIET) {
		if (!(type & MOVETYPE_ESCAPE))
			targets |= RANK_1 | RANK_8;
		else
			targets |= (RANK_1 | RANK_8) & pstate->checkray;

		/* It is okay to add this square to targets already. This square
		 * can only be reached by our pawns by capturing, not pushing.
		 */
		if (pos->en_passant && (!pstate->checkers || shift(bitboard(pos->en_passant), down) == pstate->checkers))
			targets |= bitboard(pos->en_passant);
	}

	if (pawns && type & MOVETYPE_QUIET) {
		uint64_t push_targets = targets & ~all;
		uint64_t push = pawns & shift(push_targets, down);
		uint64_t push_twice = pawns & shift_twice(push_targets, down) & (us ? RANK_2 : RANK_7) & ~shift(all, down);

		while (push) {
			square = ctz(push);
			*moves++ = new_move(square, square + pawn_sign, 0, 0);
			push = clear_ls1b(push);
		}

		while (push_twice) {
			square = ctz(push_twice);
			*moves++ = new_move(square, square + 2 * pawn_sign, 0, 0);
			push_twice = clear_ls1b(push_twice);
		}
	}

	if (pawns7 && type & MOVETYPE_NONQUIET) {
		uint64_t push = pawns7 & shift(targets & ~all, down);
		uint64_t capture_e = pawns7 & shift(targets & enemies, down | W);
		uint64_t capture_w = pawns7 & shift(targets & enemies, down | E);

		while (push) {
			square = ctz(push);
			for (int i = 3; i >= 0; i--)
				*moves++ = new_move(square, square + pawn_sign, MOVE_PROMOTION, i);
			push = clear_ls1b(push);
		}

		while (capture_e) {
			square = ctz(capture_e);
			for (int i = 3; i >= 0; i--)
				*moves++ = new_move(square, square + 1 + pawn_sign, MOVE_PROMOTION, i);
			capture_e = clear_ls1b(capture_e);
		}

		while (capture_w) {
			square = ctz(capture_w);
			for (int i = 3; i >= 0; i--)
				*moves++ = new_move(square, square - 1 + pawn_sign, MOVE_PROMOTION, i);
			capture_w = clear_ls1b(capture_w);
		}
	}

	if (pawns && type & MOVETYPE_NONQUIET) {
		uint64_t capture_e = pawns & shift(targets & enemies, down | W);
		uint64_t capture_w = pawns & shift(targets & enemies, down | E);

		while (capture_e) {
			square = ctz(capture_e);
			*moves++ = new_move(square, square + 1 + pawn_sign, 0, 0);
			capture_e = clear_ls1b(capture_e);
		}

		while (capture_w) {
			square = ctz(capture_w);
			*moves++ = new_move(square, square - 1 + pawn_sign, 0, 0);
			capture_w = clear_ls1b(capture_w);
		}

		if (pos->en_passant) {
			uint64_t en_passant = bitboard(pos->en_passant) & targets;
			uint64_t en_passant_e = pawns & shift(en_passant, down | W);
			uint64_t en_passant_w = pawns & shift(en_passant, down | E);
			if (en_passant_e) {
				square = ctz(en_passant_e);
				*moves++ = new_move(square, square + 1 + pawn_sign, MOVE_EN_PASSANT, 0);
			}
			if (en_passant_w) {
				square = ctz(en_passant_w);
				*moves++ = new_move(square, square - 1 + pawn_sign, MOVE_EN_PASSANT, 0);
			}
		}
	}

	return moves;
}

static inline move_t *movegen_piece(const struct position *pos, move_t *moves, uint64_t targets, int piece) {
	const int us = pos->turn;
	uint64_t own = pos->piece[us][ALL];
	uint64_t all = all_pieces(pos);
	uint64_t attackers = pos->piece[us][piece], attack;

	int source, target;

	while (attackers) {
		source = ctz(attackers);
		attack = attacks(piece, source, own, all) & targets;
		while (attack) {
			target = ctz(attack);
			*moves++ = new_move(source, target, 0, 0);
			attack = clear_ls1b(attack);
		}
		attackers = clear_ls1b(attackers);
	}

	return moves;
}

static inline move_t *movegen_king(const struct position *pos, const struct pstate *pstate, move_t *moves, unsigned type) {
	const int us = pos->turn;
	const int them = other_color(us);
	uint64_t own = pos->piece[us][ALL];
	uint64_t all = all_pieces(pos);

	uint64_t targets = 0;
	if (type & MOVETYPE_NONQUIET)
		targets |= pos->piece[them][ALL];
	if (type & MOVETYPE_QUIET)
		targets |= ~pos->piece[them][ALL];

	int source = ctz(pos->piece[us][KING]), target;

	uint64_t attack = attacks(KING, source, own, all) & targets & ~pstate->attacked[ALL];
	while (attack) {
		target = ctz(attack);
		*moves++ = new_move(source, target, 0, 0);
		attack = clear_ls1b(attack);
	}

	if (type & MOVETYPE_QUIET && !pstate->checkers) {
		if (us) {
			if (pos->castle & 0x1 && !(all & 0x60) &&
					!(pstate->attacked[ALL] & 0x60))
				*moves++ = new_move(e1, g1, MOVE_CASTLE, 0);
			if (pos->castle & 0x2 && !(all & 0xE) &&
					!(pstate->attacked[ALL] & 0xC))
				*moves++ = new_move(e1, c1, MOVE_CASTLE, 0);
		}
		else {
			if (pos->castle & 0x4 && !(all & 0x6000000000000000) &&
					!(pstate->attacked[ALL] & 0x6000000000000000))
				*moves++ = new_move(e8, g8, MOVE_CASTLE, 0);
			if (pos->castle & 0x8 && !(all & 0xE00000000000000) &&
					!(pstate->attacked[ALL] & 0xC00000000000000))
				*moves++ = new_move(e8, c8, MOVE_CASTLE, 0);
		}
	}

	return moves;
}

move_t *movegen(const struct position *pos, const struct pstate *pstate, move_t *moves, unsigned type) {
	assert(type & MOVETYPE_NONQUIET || type & MOVETYPE_QUIET);
	const int us = pos->turn;
	const int them = other_color(us);
	uint64_t targets = 0;
	if (type & MOVETYPE_NONQUIET)
		targets |= pos->piece[them][ALL];
	if (type & MOVETYPE_QUIET)
		targets |= ~pos->piece[them][ALL];
	if (pstate->checkers) {
		type |= MOVETYPE_ESCAPE;
		targets &= pstate->checkray;
	}

	moves = movegen_pawn(pos, pstate, moves, targets, type);
	moves = movegen_piece(pos, moves, targets, KNIGHT);
	moves = movegen_piece(pos, moves, targets, BISHOP);
	moves = movegen_piece(pos, moves, targets, ROOK);
	moves = movegen_piece(pos, moves, targets, QUEEN);
	moves = movegen_king(pos, pstate, moves, type);

	*moves = 0;
	return moves;
}

move_t *movegen_legal(const struct position *pos, move_t *moves, unsigned type) {
	struct pstate pstate;
	pstate_init(pos, &pstate);
	move_t *end = movegen(pos, &pstate, moves, type);
	for (move_t *move = moves; *move; ) {
		if (!legal(pos, &pstate, move)) {
			*move = end[-1];
			*--end = 0;
		}
		else {
			move++;
		}
	}
	return end;
}
