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

#include "move_gen.h"

#include "bitboard.h"
#include "attack_gen.h"

int move_count(const move *m) {
	const move *n;
	for (n = m; *n; n++);
	return n - m;
}

move *generate_all(const struct position *pos, move *move_list) {
	move *move_ptr = move_list;
	uint8_t i;

	uint64_t temp;
	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;
	uint64_t all_pieces = pos->piece[white][all] | pos->piece[black][all];

	uint64_t checkers = generate_checkers(pos, pos->turn);
	uint64_t attacked = generate_attacked(pos, pos->turn);
	uint64_t pinned = generate_pinned(pos, pos->turn);

	uint8_t target_square;
	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[pos->turn][king]);

	int pawn_sign = 2 * pos->turn - 1;

	if (!checkers) {
		piece = pawn_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign < 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_double_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_double_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square + 16 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[pos->turn][pawn], pos->piece[1 - pos->turn][all], pos->turn) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign < 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[pos->turn][pawn], pos->piece[1 - pos->turn][all], pos->turn) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && pawn_sign * source_square / 8 > pawn_sign * king_square / 8) {
				if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign < 7) {
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

		piece = pawn_capture_w(pos->piece[pos->turn][pawn], pos->piece[1 - pos->turn][all], pos->turn) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign < 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[pos->turn][pawn], pos->piece[1 - pos->turn][all], pos->turn) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && pawn_sign * source_square / 8 > pawn_sign * king_square / 8) {
				if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign < 7) {
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

			piece = pawn_capture_e(pos->piece[pos->turn][pawn], target_bitboard, pos->turn) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				temp = (all_pieces) ^ (target_bitboard | shift_color(target_bitboard, 1 - pos->turn) | shift_color_west(target_bitboard, 1 - pos->turn));

				if (!(rook_attacks(king_square, 0, temp) & (pos->piece[1 - pos->turn][rook] | pos->piece[1 - pos->turn][queen])) && !(bishop_attacks(king_square, 0, temp) & (pos->piece[1 - pos->turn][bishop] | pos->piece[1 - pos->turn][queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

			}

			piece = pawn_capture_e(pos->piece[pos->turn][pawn], target_bitboard, pos->turn) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}

			piece = pawn_capture_w(pos->piece[pos->turn][pawn], target_bitboard, pos->turn) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				temp = (all_pieces) ^ (target_bitboard | shift_color(target_bitboard, 1 - pos->turn) | shift_color_east(target_bitboard, 1 - pos->turn));

				if (!(rook_attacks(king_square, 0, temp) & (pos->piece[1 - pos->turn][rook] | pos->piece[1 - pos->turn][queen])) && !(bishop_attacks(king_square, 0, temp) & (pos->piece[1 - pos->turn][bishop] | pos->piece[1 - pos->turn][queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}

			piece = pawn_capture_w(pos->piece[pos->turn][pawn], target_bitboard, pos->turn) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}
		}

		piece = pos->piece[pos->turn][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[pos->turn][all]);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[pos->turn][all], all_pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[pos->turn][all], all_pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[pos->turn][all], all_pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		if (pos->turn) {
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
		piece = pawn_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & shift_color(pinned_squares, 1 - pos->turn) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign < 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_double_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & shift_color2(pinned_squares, 1 - pos->turn) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16 * pawn_sign, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[pos->turn][pawn], checkers, pos->turn) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign < 7) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 1 + 8 * pawn_sign, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[pos->turn][pawn], checkers, pos->turn) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (56 <= source_square + 8 * pawn_sign || source_square + 8 * pawn_sign < 7) {
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

			piece = pawn_capture_e(pos->piece[pos->turn][pawn], shift_color(checkers, pos->turn) & bitboard(target_square), pos->turn) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}

			piece = pawn_capture_w(pos->piece[pos->turn][pawn], shift_color(checkers, pos->turn) & bitboard(target_square), pos->turn) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}
		}

		piece = pos->piece[pos->turn][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[pos->turn][all]) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);
				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}
	}

	attacks = king_attacks(king_square, pos->piece[pos->turn][all]) & ~attacked;
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}


	/* set the terminating move */
	*move_ptr = 0;
	return move_ptr;
}

int mate(const struct position *pos) {
	uint64_t temp;
	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;
	uint64_t all_pieces = pos->piece[white][all] | pos->piece[black][all];

	uint64_t checkers = generate_checkers(pos, pos->turn);
	uint64_t attacked = generate_attacked(pos, pos->turn);
	uint64_t pinned = generate_pinned(pos, pos->turn);

	uint8_t target_square;
	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[pos->turn][king]);

	int pawn_sign = 2 * pos->turn - 1;

	attacks = king_attacks(king_square, pos->piece[pos->turn][all]) & ~attacked;
	if (attacks)
		return 0;

	if (!checkers) {
		piece = pawn_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & ~pinned;
		if (piece)
			return 0;

		piece = pawn_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pawn_double_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & ~pinned;
		if (piece)
			return 0;

		piece = pawn_double_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_e(pos->piece[pos->turn][pawn], pos->piece[1 - pos->turn][all], pos->turn) & ~pinned;
		if (piece)
			return 0;

		piece = pawn_capture_e(pos->piece[pos->turn][pawn], pos->piece[1 - pos->turn][all], pos->turn) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && pawn_sign * source_square / 8 > pawn_sign * king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pawn_capture_w(pos->piece[pos->turn][pawn], pos->piece[1 - pos->turn][all], pos->turn) & ~pinned;
		if (piece)
			return 0;

		piece = pawn_capture_w(pos->piece[pos->turn][pawn], pos->piece[1 - pos->turn][all], pos->turn) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && pawn_sign * source_square / 8 > pawn_sign * king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			uint64_t target_bitboard = bitboard(target_square);

			piece = pawn_capture_e(pos->piece[pos->turn][pawn], target_bitboard, pos->turn) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				temp = (all_pieces) ^ (target_bitboard | shift_color(target_bitboard, 1 - pos->turn) | shift_color_west(target_bitboard, 1 - pos->turn));

				if (!(rook_attacks(king_square, 0, temp) & (pos->piece[1 - pos->turn][rook] | pos->piece[1 - pos->turn][queen])) && !(bishop_attacks(king_square, 0, temp) & (pos->piece[1 - pos->turn][bishop] | pos->piece[1 - pos->turn][queen])))
					return 0;

			}

			piece = pawn_capture_e(pos->piece[pos->turn][pawn], target_bitboard, pos->turn) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square])
					return 0;
			}

			piece = pawn_capture_w(pos->piece[pos->turn][pawn], target_bitboard, pos->turn) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				temp = (all_pieces) ^ (target_bitboard | shift_color(target_bitboard, 1 - pos->turn) | shift_color_east(target_bitboard, 1 - pos->turn));

				if (!(rook_attacks(king_square, 0, temp) & (pos->piece[1 - pos->turn][rook] | pos->piece[1 - pos->turn][queen])) && !(bishop_attacks(king_square, 0, temp) & (pos->piece[1 - pos->turn][bishop] | pos->piece[1 - pos->turn][queen])))
					return 0;
			}

			piece = pawn_capture_w(pos->piece[pos->turn][pawn], target_bitboard, pos->turn) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square])
					return 0;
			}
		}

		piece = pos->piece[pos->turn][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[pos->turn][all]);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[pos->turn][all], all_pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[pos->turn][all], all_pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[pos->turn][all], all_pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = pawn_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & shift_color(pinned_squares, 1 - pos->turn) & ~pinned;
		if (piece)
			return 0;

		piece = pawn_double_push(pos->piece[pos->turn][pawn], all_pieces, pos->turn) & shift_color2(pinned_squares, 1 - pos->turn) & ~pinned;
		if (piece)
			return 0;

		piece = pawn_capture_e(pos->piece[pos->turn][pawn], checkers, pos->turn) & ~pinned;
		if (piece)
			return 0;

		piece = pawn_capture_w(pos->piece[pos->turn][pawn], checkers, pos->turn) & ~pinned;
		if (piece)
			return 0;

		if (pos->en_passant) {
			target_square = pos->en_passant;

			piece = pawn_capture_e(pos->piece[pos->turn][pawn], shift_color(checkers, pos->turn) & bitboard(target_square), pos->turn) & ~pinned;
			if (piece)
				return 0;

			piece = pawn_capture_w(pos->piece[pos->turn][pawn], shift_color(checkers, pos->turn) & bitboard(target_square), pos->turn) & ~pinned;
			if (piece)
				return 0;
		}

		piece = pos->piece[pos->turn][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[pos->turn][all]) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[pos->turn][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[pos->turn][all], all_pieces) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}
	}

	return checkers ? 2 : 1;
}

move *generate_quiescence(const struct position *pos, move *move_list) {
	//return pos->turn ? generate_quiescence_white(pos, move_list) : generate_quiescence_black(pos, move_list);
	return move_list;
}

//move *generate_quiescence_white(const struct position *pos, move *move_list) {
//	move *move_ptr = move_list;
//
//	uint64_t piece;
//	uint64_t attacks;
//	uint64_t pinned_squares;
//
//	uint64_t checkers = generate_checkers_white(pos);
//	uint64_t attacked = generate_attacked_white(pos);
//	uint64_t pinned = generate_pinned_white(pos);
//
//	uint8_t target_square;
//	uint8_t source_square;
//	uint8_t king_square;
//
//	king_square = ctz(pos->piece[white][king]);
//
//	attacks = white_king_attacks(king_square, pos->piece[white][all]) & ~attacked & pos->piece[black][all];
//	while (attacks) {
//		target_square = ctz(attacks);
//
//		*move_ptr++ = new_move(king_square, target_square, 0, 0);
//
//		attacks = clear_ls1b(attacks);
//	}
//
//	if (!checkers) {
//		piece = white_pawn_push(pos->piece[white][pawn] & RANK_7, all_pieces) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			*move_ptr++ = new_move(source_square, source_square + 8, 2, 3);
//			piece = clear_ls1b(piece);
//		}
//		
//		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (48 <= source_square) {
//				*move_ptr++ = new_move(source_square, source_square + 9, 2, 3);
//			}
//			else {
//				*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (source_square % 8 > king_square % 8 && source_square / 8 > king_square / 8) {
//				if (48 <= source_square) {
//					*move_ptr++ = new_move(source_square, source_square + 9, 2, 3);
//				}
//				else {
//					*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
//				}
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (48 <= source_square) {
//				*move_ptr++ = new_move(source_square, source_square + 7, 2, 3);
//			}
//			else {
//				*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (source_square % 8 < king_square % 8 && source_square / 8 > king_square / 8) {
//				if (48 <= source_square) {
//					*move_ptr++ = new_move(source_square, source_square + 7, 2, 3);
//				}
//				else {
//					*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
//				}
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][knight] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_knight_attacks(source_square, pos->piece[white][all]) & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][bishop] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_bishop_attacks(source_square, pos->piece[white][all], all_pieces) & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][bishop] & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_bishop_attacks(source_square, pos->piece[white][all], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][rook] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_rook_attacks(source_square, pos->piece[white][all], all_pieces) & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][rook] & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_rook_attacks(source_square, pos->piece[white][all], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][queen] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_queen_attacks(source_square, pos->piece[white][all], all_pieces) & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][queen] & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_queen_attacks(source_square, pos->piece[white][all], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//	}
//	else if (!(checkers & (checkers - 1))) {
//		source_square = ctz(checkers);
//		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
//
//		piece = white_pawn_capture_e(pos->piece[white][pawn], checkers) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (48 <= source_square) {
//				*move_ptr++ = new_move(source_square, source_square + 9, 2, 3);
//			}
//			else {
//				*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = white_pawn_capture_w(pos->piece[white][pawn], checkers) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (48 <= source_square) {
//				*move_ptr++ = new_move(source_square, source_square + 7, 2, 3);
//			}
//			else {
//				*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][knight] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_knight_attacks(source_square, pos->piece[white][all]) & pinned_squares & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][bishop] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_bishop_attacks(source_square, pos->piece[white][all], all_pieces) & pinned_squares & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][rook] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_rook_attacks(source_square, pos->piece[white][all], all_pieces) & pinned_squares & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[white][queen] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = white_queen_attacks(source_square, pos->piece[white][all], all_pieces) & pinned_squares & pos->piece[black][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//	}
//
//	/* set the terminating move */
//	*move_ptr = 0;
//	return move_ptr;
//}
//
//move *generate_quiescence_black(const struct position *pos, move *move_list) {
//	move *move_ptr = move_list;
//
//	uint64_t piece;
//	uint64_t attacks;
//	uint64_t pinned_squares;
//
//	uint64_t checkers = generate_checkers_black(pos);
//	uint64_t attacked = generate_attacked_black(pos);
//	uint64_t pinned = generate_pinned_black(pos);
//
//	uint8_t target_square;
//	uint8_t source_square;
//	uint8_t king_square;
//
//	king_square = ctz(pos->piece[black][king]);
//
//	attacks = black_king_attacks(king_square, pos->piece[black][all]) & ~attacked & pos->piece[white][all];
//	while (attacks) {
//		target_square = ctz(attacks);
//
//		*move_ptr++ = new_move(king_square, target_square, 0, 0);
//
//		attacks = clear_ls1b(attacks);
//	}
//	
//	if (!checkers) {
//		piece = black_pawn_push(pos->piece[black][pawn] & RANK_2, all_pieces) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			*move_ptr++ = new_move(source_square, source_square - 8, 2, 3);
//			piece = clear_ls1b(piece);
//		}
//
//		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (source_square < 16) {
//				*move_ptr++ = new_move(source_square, source_square - 7, 2, 3);
//			}
//			else {
//				*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (source_square % 8 > king_square % 8 && source_square / 8 < king_square / 8) {
//				if (source_square < 16) {
//					*move_ptr++ = new_move(source_square, source_square - 7, 2, 3);
//				}
//				else {
//					*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
//				}
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (source_square < 16) {
//				*move_ptr++ = new_move(source_square, source_square - 9, 2, 3);
//			}
//			else {
//				*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (source_square % 8 < king_square % 8 && source_square / 8 < king_square / 8) {
//				if (source_square < 16) {
//					*move_ptr++ = new_move(source_square, source_square - 9, 2, 3);
//				}
//				else {
//					*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
//				}
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][knight] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_knight_attacks(source_square, pos->piece[black][all]) & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][bishop] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_bishop_attacks(source_square, pos->piece[black][all], all_pieces) & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][bishop] & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_bishop_attacks(source_square, pos->piece[black][all], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][rook] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_rook_attacks(source_square, pos->piece[black][all], all_pieces) & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][rook] & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_rook_attacks(source_square, pos->piece[black][all], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][queen] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_queen_attacks(source_square, pos->piece[black][all], all_pieces) & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][queen] & pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_queen_attacks(source_square, pos->piece[black][all], all_pieces) & line_lookup[source_square + 64 * king_square] & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//	}
//	else if (!(checkers & (checkers - 1))) {
//		source_square = ctz(checkers);
//		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
//
//		piece = black_pawn_capture_e(pos->piece[black][pawn], checkers) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (source_square < 16) {
//				*move_ptr++ = new_move(source_square, source_square - 7, 2, 3);
//			}
//			else {
//				*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = black_pawn_capture_w(pos->piece[black][pawn], checkers) & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			if (source_square < 16) {
//				*move_ptr++ = new_move(source_square, source_square - 9, 2, 3);
//			}
//			else {
//				*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][knight] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_knight_attacks(source_square, pos->piece[black][all]) & pinned_squares & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][bishop] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_bishop_attacks(source_square, pos->piece[black][all], all_pieces) & pinned_squares & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][rook] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_rook_attacks(source_square, pos->piece[black][all], all_pieces) & pinned_squares & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//
//		piece = pos->piece[black][queen] & ~pinned;
//		while (piece) {
//			source_square = ctz(piece);
//			attacks = black_queen_attacks(source_square, pos->piece[black][all], all_pieces) & pinned_squares & pos->piece[white][all];
//			while (attacks) {
//				target_square = ctz(attacks);
//
//				*move_ptr++ = new_move(source_square, target_square, 0, 0);
//
//				attacks = clear_ls1b(attacks);
//			}
//			piece = clear_ls1b(piece);
//		}
//	}
//
//	/* set the terminating move */
//	*move_ptr = 0;
//	return move_ptr;
//}
//

int mobility(const struct position *pos, int color) {
	int moves = 0, t;

	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;
	uint64_t all_pieces = pos->piece[white][all] | pos->piece[black][all];

	uint64_t checkers = generate_checkers(pos, color);
	uint64_t attacked = generate_attacked(pos, color);
	uint64_t pinned = generate_pinned(pos, color);

	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[color][king]);

	if (!checkers) {
		piece = pos->piece[color][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[color][all]);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[color][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[color][all], all_pieces);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[color][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[color][all], all_pieces);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[color][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[color][all], all_pieces);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;

		piece = pos->piece[color][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = knight_attacks(source_square, pos->piece[color][all]) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[color][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = bishop_attacks(source_square, pos->piece[color][all], all_pieces) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[color][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = rook_attacks(source_square, pos->piece[color][all], all_pieces) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[color][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = queen_attacks(source_square, pos->piece[color][all], all_pieces) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}
	}

	return moves;
}
