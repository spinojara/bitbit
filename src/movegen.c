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

int move_count(const move_t *m) {
	const move_t *n;
	for (n = m; *n; n++);
	return n - m;
}

static inline move_t *pawn_moves(const struct position *pos, const struct pstate *pstate, move_t *move, uint64_t targets, unsigned type) {
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
			*move++ = new_move(square, square + pawn_sign, 0, 0);
			push = clear_ls1b(push);
		}

		while (push_twice) {
			square = ctz(push_twice);
			*move++ = new_move(square, square + 2 * pawn_sign, 0, 0);
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
				*move++ = new_move(square, square + pawn_sign, MOVE_PROMOTION, i);
			push = clear_ls1b(push);
		}

		while (capture_e) {
			square = ctz(capture_e);
			for (int i = 3; i >= 0; i--)
				*move++ = new_move(square, square + 1 + pawn_sign, MOVE_PROMOTION, i);
			capture_e = clear_ls1b(capture_e);
		}

		while (capture_w) {
			square = ctz(capture_w);
			for (int i = 3; i >= 0; i--)
				*move++ = new_move(square, square - 1 + pawn_sign, MOVE_PROMOTION, i);
			capture_w = clear_ls1b(capture_w);
		}
	}

	if (pawns && type & MOVETYPE_NONQUIET) {
		uint64_t capture_e = pawns & shift(targets & enemies, down | W);
		uint64_t capture_w = pawns & shift(targets & enemies, down | E);

		while (capture_e) {
			square = ctz(capture_e);
			*move++ = new_move(square, square + 1 + pawn_sign, 0, 0);
			capture_e = clear_ls1b(capture_e);
		}

		while (capture_w) {
			square = ctz(capture_w);
			*move++ = new_move(square, square - 1 + pawn_sign, 0, 0);
			capture_w = clear_ls1b(capture_w);
		}

		if (pos->en_passant) {
			uint64_t en_passant = bitboard(pos->en_passant) & targets;
			uint64_t en_passant_e = pawns & shift(en_passant, down | W);
			uint64_t en_passant_w = pawns & shift(en_passant, down | E);
			if (en_passant_e) {
				square = ctz(en_passant_e);
				*move++ = new_move(square, square + 1 + pawn_sign, MOVE_EN_PASSANT, 0);
			}
			if (en_passant_w) {
				square = ctz(en_passant_w);
				*move++ = new_move(square, square - 1 + pawn_sign, MOVE_EN_PASSANT, 0);
			}
		}
	}

	return move;
}

static inline move_t *piece_moves(const struct position *pos, move_t *move, uint64_t targets, int piece) {
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
			*move++ = new_move(source, target, 0, 0);
			attack = clear_ls1b(attack);
		}
		attackers = clear_ls1b(attackers);
	}

	return move;
}

static inline move_t *king_moves(const struct position *pos, const struct pstate *pstate, move_t *move, unsigned type) {
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

	uint64_t attack = attacks(KING, source, own, all) & targets & ~pstate->attacked;
	while (attack) {
		target = ctz(attack);
		*move++ = new_move(source, target, 0, 0);
		attack = clear_ls1b(attack);
	}

	if (type & MOVETYPE_QUIET && !pstate->checkers) {
		if (us) {
			if (pos->castle & 0x1 && !(all & 0x60) &&
					!(pstate->attacked & 0x60))
				*move++ = new_move(e1, g1, MOVE_CASTLE, 0);
			if (pos->castle & 0x2 && !(all & 0xE) &&
					!(pstate->attacked & 0xC))
				*move++ = new_move(e1, c1, MOVE_CASTLE, 0);
		}
		else {
			if (pos->castle & 0x4 && !(all & 0x6000000000000000) &&
					!(pstate->attacked & 0x6000000000000000))
				*move++ = new_move(e8, g8, MOVE_CASTLE, 0);
			if (pos->castle & 0x8 && !(all & 0xE00000000000000) &&
					!(pstate->attacked & 0xC00000000000000))
				*move++ = new_move(e8, c8, MOVE_CASTLE, 0);
		}
	}

	return move;
}

move_t *moves(const struct position *pos, const struct pstate *pstate, move_t *move, unsigned type) {
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

	move = pawn_moves(pos, pstate, move, targets, type);
	move = piece_moves(pos, move, targets, KNIGHT);
	move = piece_moves(pos, move, targets, BISHOP);
	move = piece_moves(pos, move, targets, ROOK);
	move = piece_moves(pos, move, targets, QUEEN);
	move = king_moves(pos, pstate, move, type);

	*move = 0;
	return move;
}

move_t *generate_all(const struct position *pos, move_t *move_list) {
	const int us = pos->turn;
	const int them = other_color(us);
	const unsigned up = us ? N : S;
	const unsigned down = us ? S : N;

	move_t *move_ptr = move_list;
	int i;

	uint64_t temp;
	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;
	uint64_t all_pieces = pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL];

	uint64_t checkers = generate_checkers(pos, us);
	uint64_t attacked = generate_attacked(pos, them);
	uint64_t pinned = generate_pinned(pos, us);

	int target_square;
	int source_square;
	int king_square;

	king_square = ctz(pos->piece[us][KING]);

	int pawn_sign = 2 * us - 1;

	if (!checkers) {
		piece = pawn_push(pos->piece[us][PAWN], all_pieces, us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_push(pos->piece[us][PAWN], all_pieces, us) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (file_of(source_square) == file_of(king_square)) {
				*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_double_push(pos->piece[us][PAWN], all_pieces, us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_double_push(pos->piece[us][PAWN], all_pieces, us) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (file_of(source_square) == file_of(king_square)) {
				*move_ptr++ = new_move(source_square, source_square + 16 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[us][PAWN], pos->piece[them][ALL], us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[us][PAWN], pos->piece[them][ALL], us) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (file_of(source_square) > file_of(king_square) && pawn_sign * rank_of(source_square) > pawn_sign * rank_of(king_square)) {
				if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7) {
					for (i = 0; i < 4; i++) {
						*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 2, i);
					}
				}
				else {
					*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[us][PAWN], pos->piece[them][ALL], us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[us][PAWN], pos->piece[them][ALL], us) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (file_of(source_square) < file_of(king_square) && pawn_sign * rank_of(source_square) > pawn_sign * rank_of(king_square)) {
				if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7) {
					for (i = 0; i < 4; i++) {
						*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 2, i);
					}
				}
				else {
					*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			uint64_t target_bitboard = bitboard(target_square);

			piece = pawn_capture_e(pos->piece[us][PAWN], target_bitboard, us) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				temp = (all_pieces) ^ (target_bitboard | shift(target_bitboard, down) | shift(target_bitboard, down | W));

				if (!(rook_attacks(king_square, 0, temp) & (pos->piece[them][ROOK] | pos->piece[them][QUEEN])) && !(bishop_attacks(king_square, 0, temp) & (pos->piece[them][BISHOP] | pos->piece[them][QUEEN]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

			}

			piece = pawn_capture_e(pos->piece[us][PAWN], target_bitboard, us) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}

			piece = pawn_capture_w(pos->piece[us][PAWN], target_bitboard, us) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				temp = (all_pieces) ^ (target_bitboard | shift(target_bitboard, down) | shift(target_bitboard, down | E));

				if (!(rook_attacks(king_square, 0, temp) & (pos->piece[them][ROOK] | pos->piece[them][QUEEN])) && !(bishop_attacks(king_square, 0, temp) & (pos->piece[them][BISHOP] | pos->piece[them][QUEEN]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}

			piece = pawn_capture_w(pos->piece[us][PAWN], target_bitboard, us) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}
		}

		piece = pos->piece[us][KNIGHT] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[us][ALL]);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][BISHOP] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[us][ALL], all_pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][BISHOP] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[us][ALL], all_pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][ROOK] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[us][ALL], all_pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][ROOK] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[us][ALL], all_pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][QUEEN] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[us][ALL], all_pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][QUEEN] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[us][ALL], all_pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		if (us) {
			if (pos->castle & 0x1) {
				if (!(all_pieces & 0x60)) {
					if (!(attacked & 0x60)) {
						*move_ptr++ = new_move(e1, g1, 3, 0);
					}
				}
			}
			if (pos->castle & 0x2) {
				if (!(all_pieces & 0xE)) {
					if (!(attacked & 0xC)) {
						*move_ptr++ = new_move(e1, c1, 3, 0);
					}
				}
			}
		}
		else {
			if (pos->castle & 0x4) {
				if (!(all_pieces & 0x6000000000000000)) {
					if (!(attacked & 0x6000000000000000)) {
						*move_ptr++ = new_move(e8, g8, 3, 0);
					}
				}

			}
			if (pos->castle & 0x8) {
				if (!(all_pieces & 0xE00000000000000)) {
					if (!(attacked & 0xC00000000000000)) {
						*move_ptr++ = new_move(e8, c8, 3, 0);
					}
				}
			}
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = pawn_push(pos->piece[us][PAWN], all_pieces, us) & shift(pinned_squares, down) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_double_push(pos->piece[us][PAWN], all_pieces, us) & shift_twice(pinned_squares, down) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[us][PAWN], checkers, us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[us][PAWN], checkers, us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			piece = pawn_capture_e(pos->piece[us][PAWN], shift(checkers, up) & bitboard(target_square), us) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}

			piece = pawn_capture_w(pos->piece[us][PAWN], shift(checkers, up) & bitboard(target_square), us) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}
		}

		piece = pos->piece[us][KNIGHT] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[us][ALL]) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);
				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][BISHOP] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[us][ALL], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][ROOK] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[us][ALL], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][QUEEN] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[us][ALL], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}
	}

	attacks = king_attacks(king_square, pos->piece[us][ALL]) & ~attacked;
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}


	/* Set the terminating move. */
	*move_ptr = 0;
	return move_ptr;
}

move_t *generate_quiescence(const struct position *pos, move_t *move_list) {
	const int us = pos->turn;
	const int them = other_color(us);
	const unsigned down = us ? S : N;

	move_t *move_ptr = move_list;

	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;
	uint64_t all_pieces = pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL];

	uint64_t checkers = generate_checkers(pos, us);
	uint64_t attacked = generate_attacked(pos, them);
	uint64_t pinned = generate_pinned(pos, us);

	int target_square;
	int source_square;
	int king_square;

	king_square = ctz(pos->piece[us][KING]);

	int pawn_sign = 2 * us - 1;

	if (!checkers) {
		piece = pawn_push(pos->piece[us][PAWN] & (us ? RANK_7 : RANK_2), all_pieces, us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 2, 3);
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[us][PAWN], pos->piece[them][ALL], us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7)
				*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 2, 3);
			else
				*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[us][PAWN], pos->piece[them][ALL], us) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (file_of(source_square) > file_of(king_square) && pawn_sign * rank_of(source_square) > pawn_sign * rank_of(king_square)) {
				if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7)
					*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 2, 3);
				else
					*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[us][PAWN], pos->piece[them][ALL], us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7)
				*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 2, 3);
			else
				*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[us][PAWN], pos->piece[them][ALL], us) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (file_of(source_square) < file_of(king_square) && pawn_sign * rank_of(source_square) > pawn_sign * rank_of(king_square)) {
				if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7)
					*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 2, 3);
				else
					*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][KNIGHT] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[us][ALL]) & pos->piece[them][ALL];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][BISHOP] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[us][ALL], all_pieces) & pos->piece[them][ALL];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][BISHOP] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[us][ALL], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[them][ALL];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][ROOK] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[us][ALL], all_pieces) & pos->piece[them][ALL];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][ROOK] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[us][ALL], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[them][ALL];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][QUEEN] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[us][ALL], all_pieces) & pos->piece[them][ALL];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][QUEEN] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[us][ALL], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[them][ALL];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;

		piece = pawn_push(pos->piece[us][PAWN], all_pieces, us) & shift(pinned_squares, down) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7)
					*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 2, 3);
			else
				*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_double_push(pos->piece[us][PAWN], all_pieces, us) & shift_twice(pinned_squares, down) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[us][PAWN], checkers, us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7)
				*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 2, 3);
			else
				*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[us][PAWN], checkers, us) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign <= 7)
				*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 2, 3);
			else
				*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][KNIGHT] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[us][ALL]) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);
				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][BISHOP] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[us][ALL], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][ROOK] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[us][ALL], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[us][QUEEN] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[us][ALL], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}
	}

	attacks = king_attacks(king_square, pos->piece[us][ALL]) & ~attacked;
	if (!checkers)
		attacks &= pos->piece[them][ALL];
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}

	/* Set the terminating move. */
	*move_ptr = 0;
	return move_ptr;
}
