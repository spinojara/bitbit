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

move *generate_all(struct position *pos, move *move_list) {
	return pos->turn ? generate_white(pos, move_list) : generate_black(pos, move_list);
}

move *generate_white(struct position *pos, move *move_list) {
	move *move_ptr = move_list;
	uint8_t i;

	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;

	uint64_t checkers = generate_checkers_white(pos);
	uint64_t attacked = generate_attacked_white(pos);
	uint64_t pinned = generate_pinned_white(pos);

	uint8_t target_square;
	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[white][king]);

	attacks = white_king_attacks(king_square, pos->piece[white][all]) & ~attacked;
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}

	if (!checkers) {
		piece = white_pawn_push(pos->piece[white][pawn], pos->piece_all) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 8, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 8, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_push(pos->piece[white][pawn], pos->piece_all) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square + 8, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_double_push(pos->piece[white][pawn], pos->piece_all) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_double_push(pos->piece[white][pawn], pos->piece_all) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square + 16, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 9, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && source_square / 8 > king_square / 8) {
				if (48 <= source_square) {
					for (i = 0; i < 4; i++) {
						*move_ptr++ = new_move(source_square, source_square + 9, 2, i);
					}
				}
				else {
					*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 7, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && source_square / 8 > king_square / 8) {
				if (48 <= source_square) {
					for (i = 0; i < 4; i++) {
						*move_ptr++ = new_move(source_square, source_square + 7, 2, i);
					}
				}
				else {
					*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			uint64_t target_bitboard = bitboard(target_square);

			piece = white_pawn_capture_e(pos->piece[white][pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->piece_all ^= target_bitboard | shift_south(target_bitboard) | shift_south_west(target_bitboard);

				if (!(rook_attacks(king_square, pos->piece_all) & (pos->piece[black][rook] | pos->piece[black][queen])) && !(bishop_attacks(king_square, pos->piece_all) & (pos->piece[black][bishop] | pos->piece[black][queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

				pos->piece_all ^= target_bitboard | shift_south(target_bitboard) | shift_south_west(target_bitboard);

			}

			piece = white_pawn_capture_e(pos->piece[white][pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}

			piece = white_pawn_capture_w(pos->piece[white][pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->piece_all ^= target_bitboard | shift_south(target_bitboard) | shift_south_east(target_bitboard);

				if (!(rook_attacks(king_square, pos->piece_all) & (pos->piece[black][rook] | pos->piece[black][queen])) && !(bishop_attacks(king_square, pos->piece_all) & (pos->piece[black][bishop] | pos->piece[black][queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

				pos->piece_all ^= target_bitboard | shift_south(target_bitboard) | shift_south_east(target_bitboard);

			}

			piece = white_pawn_capture_w(pos->piece[white][pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}
		}

		piece = pos->piece[white][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->piece[white][all]);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		if (pos->castle & 0x1) {
			if (!(pos->piece_all & 0x60)) {
				if (!(attacked & 0x60)) {
					*move_ptr++ = new_move(4, 6, 3, 0);
				}
			}
		}
		if (pos->castle & 0x2) {
			if (!(pos->piece_all & 0xE)) {
				if (!(attacked & 0xC)) {
					*move_ptr++ = new_move(4, 2, 3, 0);
				}
			}
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = white_pawn_push(pos->piece[white][pawn], pos->piece_all) & shift_south(pinned_squares) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 8, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 8, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_double_push(pos->piece[white][pawn], pos->piece_all) & shift_south_south(pinned_squares) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_e(pos->piece[white][pawn], checkers) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 9, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_w(pos->piece[white][pawn], checkers) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square + 7, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			piece = white_pawn_capture_e(pos->piece[white][pawn], shift_north(checkers) & bitboard(target_square)) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}

			piece = white_pawn_capture_w(pos->piece[white][pawn], shift_north(checkers) & bitboard(target_square)) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}
		}

		piece = pos->piece[white][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->piece[white][all]) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);
				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}
	}

	/* set the terminating move */
	*move_ptr = 0;
	return move_ptr;
}

move *generate_black(struct position *pos, move *move_list) {
	move *move_ptr = move_list;
	uint8_t i;

	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;

	uint64_t checkers = generate_checkers_black(pos);
	uint64_t attacked = generate_attacked_black(pos);
	uint64_t pinned = generate_pinned_black(pos);

	uint8_t target_square;
	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[black][king]);

	attacks = black_king_attacks(king_square, pos->piece[black][all]) & ~attacked;
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}
	
	if (!checkers) {
		piece = black_pawn_push(pos->piece[black][pawn], pos->piece_all) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 8, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 8, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_push(pos->piece[black][pawn], pos->piece_all) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square - 8, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_double_push(pos->piece[black][pawn], pos->piece_all) & ~pinned;
		while (piece) {
			source_square = ctz(piece);

			*move_ptr++ = new_move(source_square, source_square - 16, 0, 0);

			piece = clear_ls1b(piece);
		}

		piece = black_pawn_double_push(pos->piece[black][pawn], pos->piece_all) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square - 16, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 7, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && source_square / 8 < king_square / 8) {
				if (source_square < 16) {
					for (i = 0; i < 4; i++) {
						*move_ptr++ = new_move(source_square, source_square - 7, 2, i);
					}
				}
				else {
					*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 9, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && source_square / 8 < king_square / 8) {
				if (source_square < 16) {
					for (i = 0; i < 4; i++) {
						*move_ptr++ = new_move(source_square, source_square - 9, 2, i);
					}
				}
				else {
					*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			uint64_t target_bitboard = bitboard(target_square);

			piece = black_pawn_capture_e(pos->piece[black][pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->piece_all ^= target_bitboard | shift_north(target_bitboard) | shift_north_west(target_bitboard);

				if (!(rook_attacks(king_square, pos->piece_all) & (pos->piece[white][rook] | pos->piece[white][queen])) && !(bishop_attacks(king_square, pos->piece_all) & (pos->piece[white][bishop] | pos->piece[white][queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

				pos->piece_all ^= target_bitboard | shift_north(target_bitboard) | shift_north_west(target_bitboard);

			}

			piece = black_pawn_capture_e(pos->piece[black][pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}

			piece = black_pawn_capture_w(pos->piece[black][pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->piece_all ^= target_bitboard | shift_north(target_bitboard) | shift_north_east(target_bitboard);

				if (!(rook_attacks(king_square, pos->piece_all) & (pos->piece[white][rook] | pos->piece[white][queen])) && !(bishop_attacks(king_square, pos->piece_all) & (pos->piece[white][bishop] | pos->piece[white][queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

				pos->piece_all ^= target_bitboard | shift_north(target_bitboard) | shift_north_east(target_bitboard);

			}

			piece = black_pawn_capture_w(pos->piece[black][pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}
		}

		piece = pos->piece[black][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->piece[black][all]);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		if (pos->castle & 0x4) {
			if (!(pos->piece_all & 0x6000000000000000)) {
				if (!(attacked & 0x6000000000000000)) {
					*move_ptr++ = new_move(60, 62, 3, 0);
				}
			}

		}
		if (pos->castle & 0x8) {
			if (!(pos->piece_all & 0xE00000000000000)) {
				if (!(attacked & 0xC00000000000000)) {
					*move_ptr++ = new_move(60, 58, 3, 0);
				}
			}
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = black_pawn_push(pos->piece[black][pawn], pos->piece_all) & shift_north(pinned_squares) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 8, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 8, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_double_push(pos->piece[black][pawn], pos->piece_all) & shift_north_north(pinned_squares) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square - 16, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->piece[black][pawn], checkers) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 7, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_w(pos->piece[black][pawn], checkers) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				for (i = 0; i < 4; i++) {
					*move_ptr++ = new_move(source_square, source_square - 9, 2, i);
				}
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			piece = black_pawn_capture_e(pos->piece[black][pawn], shift_south(checkers) & bitboard(target_square)) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}

			piece = black_pawn_capture_w(pos->piece[black][pawn], shift_south(checkers) & bitboard(target_square)) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}
		}

		piece = pos->piece[black][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->piece[black][all]) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}
	}

	/* set the terminating move */
	*move_ptr = 0;
	return move_ptr;
}

int move_count(move *m) {
	move *n;
	for (n = m; *n; n++);
	return n - m;
}

int mate(struct position *pos) {
	/* no need to check for castling moves */
	return pos->turn ? mate_white(pos) : mate_black(pos);
}

int mate_white(struct position *pos) {
	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;

	uint64_t attacked = generate_attacked_white(pos);

	uint8_t target_square;
	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[white][king]);

	attacks = white_king_attacks(king_square, pos->piece[white][all]) & ~attacked;
	if (attacks)
		return 0;

	uint64_t checkers = generate_checkers_white(pos);
	uint64_t pinned = generate_pinned_white(pos);
	
	if (!checkers) {
		piece = white_pawn_push(pos->piece[white][pawn], pos->piece_all) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_push(pos->piece[white][pawn], pos->piece_all) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_double_push(pos->piece[white][pawn], pos->piece_all) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_double_push(pos->piece[white][pawn], pos->piece_all) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && source_square / 8 > king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && source_square / 8 > king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			uint64_t target_bitboard = bitboard(target_square);

			piece = white_pawn_capture_e(pos->piece[white][pawn], target_bitboard) & ~pinned;
			if (piece) {
				pos->piece_all ^= target_bitboard | shift_south(target_bitboard) | shift_south_west(target_bitboard);

				if (!(rook_attacks(king_square, pos->piece_all) & (pos->piece[black][rook] | pos->piece[black][queen])) && !(bishop_attacks(king_square, pos->piece_all) & (pos->piece[black][bishop] | pos->piece[black][queen]))) {
					return 0;
				}

				pos->piece_all ^= target_bitboard | shift_south(target_bitboard) | shift_south_west(target_bitboard);

			}

			piece = white_pawn_capture_e(pos->piece[white][pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					return 0;
				}
			}

			piece = white_pawn_capture_w(pos->piece[white][pawn], target_bitboard) & ~pinned;
			if (piece) {
				pos->piece_all ^= target_bitboard | shift_south(target_bitboard) | shift_south_east(target_bitboard);

				if (!(rook_attacks(king_square, pos->piece_all) & (pos->piece[black][rook] | pos->piece[black][queen])) && !(bishop_attacks(king_square, pos->piece_all) & (pos->piece[black][bishop] | pos->piece[black][queen]))) {
					return 0;
				}

				pos->piece_all ^= target_bitboard | shift_south(target_bitboard) | shift_south_east(target_bitboard);

			}

			piece = white_pawn_capture_w(pos->piece[white][pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					return 0;
				}
			}
		}

		piece = pos->piece[white][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->piece[white][all]);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = white_pawn_push(pos->piece[white][pawn], pos->piece_all) & shift_south(pinned_squares) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_double_push(pos->piece[white][pawn], pos->piece_all) & shift_south_south(pinned_squares) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_capture_e(pos->piece[white][pawn], checkers) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_capture_w(pos->piece[white][pawn], checkers) & ~pinned;
		if (piece)
			return 0;

		if (pos->en_passant) {
			target_square = pos->en_passant;

			piece = white_pawn_capture_e(pos->piece[white][pawn], shift_north(checkers) & bitboard(target_square)) & ~pinned;
			if (piece)
				return 0;

			piece = white_pawn_capture_w(pos->piece[white][pawn], shift_north(checkers) & bitboard(target_square)) & ~pinned;
			if (piece)
				return 0;
		}

		piece = pos->piece[white][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->piece[white][all]) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}
	}

	return checkers ? 2 : 1;
}

int mate_black(struct position *pos) {
	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;

	uint64_t attacked = generate_attacked_black(pos);

	uint8_t target_square;
	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[black][king]);

	attacks = black_king_attacks(king_square, pos->piece[black][all]) & ~attacked;
	if (attacks)
		return 0;
	
	uint64_t checkers = generate_checkers_black(pos);
	uint64_t pinned = generate_pinned_black(pos);

	if (!checkers) {
		piece = black_pawn_push(pos->piece[black][pawn], pos->piece_all) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_push(pos->piece[black][pawn], pos->piece_all) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_double_push(pos->piece[black][pawn], pos->piece_all) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_double_push(pos->piece[black][pawn], pos->piece_all) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && source_square / 8 < king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && source_square / 8 < king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			uint64_t target_bitboard = bitboard(target_square);

			piece = black_pawn_capture_e(pos->piece[black][pawn], target_bitboard) & ~pinned;
			if (piece) {
				pos->piece_all ^= target_bitboard | shift_north(target_bitboard) | shift_north_west(target_bitboard);

				if (!(rook_attacks(king_square, pos->piece_all) & (pos->piece[white][rook] | pos->piece[white][queen])) && !(bishop_attacks(king_square, pos->piece_all) & (pos->piece[white][bishop] | pos->piece[white][queen])))
					return 0;

				pos->piece_all ^= target_bitboard | shift_north(target_bitboard) | shift_north_west(target_bitboard);

			}

			piece = black_pawn_capture_e(pos->piece[black][pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					return 0;
				}
			}

			piece = black_pawn_capture_w(pos->piece[black][pawn], target_bitboard) & ~pinned;
			if (piece) {
				pos->piece_all ^= target_bitboard | shift_north(target_bitboard) | shift_north_east(target_bitboard);

				if (!(rook_attacks(king_square, pos->piece_all) & (pos->piece[white][rook] | pos->piece[white][queen])) && !(bishop_attacks(king_square, pos->piece_all) & (pos->piece[white][bishop] | pos->piece[white][queen]))) {
					return 0;
				}

				pos->piece_all ^= target_bitboard | shift_north(target_bitboard) | shift_north_east(target_bitboard);

			}

			piece = black_pawn_capture_w(pos->piece[black][pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					return 0;
				}
			}
		}

		piece = pos->piece[black][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->piece[black][all]);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = black_pawn_push(pos->piece[black][pawn], pos->piece_all) & shift_north(pinned_squares) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_double_push(pos->piece[black][pawn], pos->piece_all) & shift_north_north(pinned_squares) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_capture_e(pos->piece[black][pawn], checkers) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_capture_w(pos->piece[black][pawn], checkers) & ~pinned;
		if (piece)
			return 0;

		if (pos->en_passant) {
			target_square = pos->en_passant;

			piece = black_pawn_capture_e(pos->piece[black][pawn], shift_south(checkers) & bitboard(target_square)) & ~pinned;
			if (piece)
				return 0;

			piece = black_pawn_capture_w(pos->piece[black][pawn], shift_south(checkers) & bitboard(target_square)) & ~pinned;
			if (piece)
				return 0;
		}

		piece = pos->piece[black][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->piece[black][all]) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}
	}

	return checkers ? 2 : 1;
}

move *generate_quiescence(struct position *pos, move *move_list) {
	return pos->turn ? generate_quiescence_white(pos, move_list) : generate_quiescence_black(pos, move_list);
}

move *generate_quiescence_white(struct position *pos, move *move_list) {
	move *move_ptr = move_list;

	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;

	uint64_t checkers = generate_checkers_white(pos);
	uint64_t attacked = generate_attacked_white(pos);
	uint64_t pinned = generate_pinned_white(pos);

	uint8_t target_square;
	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[white][king]);

	attacks = white_king_attacks(king_square, pos->piece[white][all]) & ~attacked & pos->piece[black][all];
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}

	if (!checkers) {
		piece = white_pawn_push(pos->piece[white][pawn] & RANK_7, pos->piece_all) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 8, 2, 3);
			piece = clear_ls1b(piece);
		}
		
		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				*move_ptr++ = new_move(source_square, source_square + 9, 2, 3);
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && source_square / 8 > king_square / 8) {
				if (48 <= source_square) {
					*move_ptr++ = new_move(source_square, source_square + 9, 2, 3);
				}
				else {
					*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				*move_ptr++ = new_move(source_square, source_square + 7, 2, 3);
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && source_square / 8 > king_square / 8) {
				if (48 <= source_square) {
					*move_ptr++ = new_move(source_square, source_square + 7, 2, 3);
				}
				else {
					*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->piece[white][all]) & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all) & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square] & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all) & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square] & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all) & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all) & line_lookup[source_square + 64 * king_square] & pos->piece[black][all];
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

		piece = white_pawn_capture_e(pos->piece[white][pawn], checkers) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				*move_ptr++ = new_move(source_square, source_square + 9, 2, 3);
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 9, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_w(pos->piece[white][pawn], checkers) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (48 <= source_square) {
				*move_ptr++ = new_move(source_square, source_square + 7, 2, 3);
			}
			else {
				*move_ptr++ = new_move(source_square, source_square + 7, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->piece[white][all]) & pinned_squares & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);
				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares & pos->piece[black][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}
	}

	/* set the terminating move */
	*move_ptr = 0;
	return move_ptr;
}

move *generate_quiescence_black(struct position *pos, move *move_list) {
	move *move_ptr = move_list;

	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;

	uint64_t checkers = generate_checkers_black(pos);
	uint64_t attacked = generate_attacked_black(pos);
	uint64_t pinned = generate_pinned_black(pos);

	uint8_t target_square;
	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[black][king]);

	attacks = black_king_attacks(king_square, pos->piece[black][all]) & ~attacked & pos->piece[white][all];
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}
	
	if (!checkers) {
		piece = black_pawn_push(pos->piece[black][pawn] & RANK_2, pos->piece_all) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square - 8, 2, 3);
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				*move_ptr++ = new_move(source_square, source_square - 7, 2, 3);
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && source_square / 8 < king_square / 8) {
				if (source_square < 16) {
					*move_ptr++ = new_move(source_square, source_square - 7, 2, 3);
				}
				else {
					*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				*move_ptr++ = new_move(source_square, source_square - 9, 2, 3);
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && source_square / 8 < king_square / 8) {
				if (source_square < 16) {
					*move_ptr++ = new_move(source_square, source_square - 9, 2, 3);
				}
				else {
					*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
				}
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->piece[black][all]) & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all) & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square] & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all) & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square] & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all) & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all) & line_lookup[source_square + 64 * king_square] & pos->piece[white][all];
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

		piece = black_pawn_capture_e(pos->piece[black][pawn], checkers) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				*move_ptr++ = new_move(source_square, source_square - 7, 2, 3);
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 7, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_w(pos->piece[black][pawn], checkers) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square < 16) {
				*move_ptr++ = new_move(source_square, source_square - 9, 2, 3);
			}
			else {
				*move_ptr++ = new_move(source_square, source_square - 9, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->piece[black][all]) & pinned_squares & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares & pos->piece[white][all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}
	}

	/* set the terminating move */
	*move_ptr = 0;
	return move_ptr;
}

int mobility_white(struct position *pos) {
	int moves = 0, t;

	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;

	uint64_t checkers = generate_checkers_white(pos);
	uint64_t attacked = generate_attacked_white(pos);
	uint64_t pinned = generate_pinned_white(pos);

	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[white][king]);

	attacks = white_king_attacks(king_square, pos->piece[white][all]) & ~attacked;
	moves += (t = popcount(attacks)) ? t : 0;

	if (!checkers) {
		piece = white_pawn_push(pos->piece[white][pawn], pos->piece_all) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = white_pawn_double_push(pos->piece[white][pawn], pos->piece_all) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = white_pawn_capture_e(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = white_pawn_capture_w(pos->piece[white][pawn], pos->piece[black][all]) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = pos->piece[white][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->piece[white][all]);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = white_pawn_push(pos->piece[white][pawn], pos->piece_all) & shift_south(pinned_squares) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = white_pawn_double_push(pos->piece[white][pawn], pos->piece_all) & shift_south_south(pinned_squares) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = white_pawn_capture_e(pos->piece[white][pawn], checkers) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = white_pawn_capture_w(pos->piece[white][pawn], checkers) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = pos->piece[white][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->piece[white][all]) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[white][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->piece[white][all], pos->piece_all) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}
	}
	return moves;
}

int mobility_black(struct position *pos) {
	int moves = 0, t;

	uint64_t piece;
	uint64_t attacks;
	uint64_t pinned_squares;

	uint64_t checkers = generate_checkers_black(pos);
	uint64_t attacked = generate_attacked_black(pos);
	uint64_t pinned = generate_pinned_black(pos);

	uint8_t source_square;
	uint8_t king_square;

	king_square = ctz(pos->piece[black][king]);

	attacks = black_king_attacks(king_square, pos->piece[black][all]) & ~attacked;
	moves += (t = popcount(attacks)) ? t : 0;
	
	if (!checkers) {
		piece = black_pawn_push(pos->piece[black][pawn], pos->piece_all) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = black_pawn_double_push(pos->piece[black][pawn], pos->piece_all) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = black_pawn_capture_e(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = black_pawn_capture_w(pos->piece[black][pawn], pos->piece[white][all]) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = pos->piece[black][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->piece[black][all]);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all);
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = black_pawn_push(pos->piece[black][pawn], pos->piece_all) & shift_north(pinned_squares) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = black_pawn_double_push(pos->piece[black][pawn], pos->piece_all) & shift_north_north(pinned_squares) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = black_pawn_capture_e(pos->piece[black][pawn], checkers) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = black_pawn_capture_w(pos->piece[black][pawn], checkers) & ~pinned;
		moves += (t = popcount(piece)) ? t : 0;

		piece = pos->piece[black][knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->piece[black][all]) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}

		piece = pos->piece[black][queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->piece[black][all], pos->piece_all) & pinned_squares;
			moves += (t = popcount(attacks)) ? t : -5;
			piece = clear_ls1b(piece);
		}
	}
	return moves;
}
