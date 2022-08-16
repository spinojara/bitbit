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

#include <stdlib.h>

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

	king_square = ctz(pos->white_pieces[king]);

	attacks = white_king_attacks(king_square, pos->white_pieces[all]) & ~attacked;
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}

	if (!checkers) {
		piece = white_pawn_push(pos->white_pieces[pawn], pos->pieces) & ~pinned;
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

		piece = white_pawn_push(pos->white_pieces[pawn], pos->pieces) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square + 8, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_double_push(pos->white_pieces[pawn], pos->pieces) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_double_push(pos->white_pieces[pawn], pos->pieces) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square + 16, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_e(pos->white_pieces[pawn], pos->black_pieces[all]) & ~pinned;
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

		piece = white_pawn_capture_e(pos->white_pieces[pawn], pos->black_pieces[all]) & pinned;
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

		piece = white_pawn_capture_w(pos->white_pieces[pawn], pos->black_pieces[all]) & ~pinned;
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

		piece = white_pawn_capture_w(pos->white_pieces[pawn], pos->black_pieces[all]) & pinned;
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

			piece = white_pawn_capture_e(pos->white_pieces[pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->pieces ^= target_bitboard | shift_south(target_bitboard) | shift_south_west(target_bitboard);

				if (!(rook_attacks(king_square, pos->pieces) & (pos->black_pieces[rook] | pos->black_pieces[queen])) && !(bishop_attacks(king_square, pos->pieces) & (pos->black_pieces[bishop] | pos->black_pieces[queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

				pos->pieces ^= target_bitboard | shift_south(target_bitboard) | shift_south_west(target_bitboard);

			}

			piece = white_pawn_capture_e(pos->white_pieces[pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}

			piece = white_pawn_capture_w(pos->white_pieces[pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->pieces ^= target_bitboard | shift_south(target_bitboard) | shift_south_east(target_bitboard);

				if (!(rook_attacks(king_square, pos->pieces) & (pos->black_pieces[rook] | pos->black_pieces[queen])) && !(bishop_attacks(king_square, pos->pieces) & (pos->black_pieces[bishop] | pos->black_pieces[queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

				pos->pieces ^= target_bitboard | shift_south(target_bitboard) | shift_south_east(target_bitboard);

			}

			piece = white_pawn_capture_w(pos->white_pieces[pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}
		}

		piece = pos->white_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->white_pieces[all]);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		if (pos->castle & 0x1) {
			if (!(pos->pieces & 0x60)) {
				if (!(attacked & 0x60)) {
					*move_ptr++ = new_move(4, 6, 3, 0);
				}
			}
		}
		if (pos->castle & 0x2) {
			if (!(pos->pieces & 0xE)) {
				if (!(attacked & 0xC)) {
					*move_ptr++ = new_move(4, 2, 3, 0);
				}
			}
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = white_pawn_push(pos->white_pieces[pawn], pos->pieces) & shift_south(pinned_squares) & ~pinned;
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

		piece = white_pawn_double_push(pos->white_pieces[pawn], pos->pieces) & shift_south_south(pinned_squares) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square + 16, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_e(pos->white_pieces[pawn], checkers) & ~pinned;
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

		piece = white_pawn_capture_w(pos->white_pieces[pawn], checkers) & ~pinned;
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

			piece = white_pawn_capture_e(pos->white_pieces[pawn], shift_north(checkers) & bitboard(target_square)) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}

			piece = white_pawn_capture_w(pos->white_pieces[pawn], shift_north(checkers) & bitboard(target_square)) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}
		}

		piece = pos->white_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->white_pieces[all]) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);
				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares;
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

	king_square = ctz(pos->black_pieces[king]);

	attacks = black_king_attacks(king_square, pos->black_pieces[all]) & ~attacked;
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}
	
	if (!checkers) {
		piece = black_pawn_push(pos->black_pieces[pawn], pos->pieces) & ~pinned;
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

		piece = black_pawn_push(pos->black_pieces[pawn], pos->pieces) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square - 8, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_double_push(pos->black_pieces[pawn], pos->pieces) & ~pinned;
		while (piece) {
			source_square = ctz(piece);

			*move_ptr++ = new_move(source_square, source_square - 16, 0, 0);

			piece = clear_ls1b(piece);
		}

		piece = black_pawn_double_push(pos->black_pieces[pawn], pos->pieces) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0) {
				*move_ptr++ = new_move(source_square, source_square - 16, 0, 0);
			}
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->black_pieces[pawn], pos->white_pieces[all]) & ~pinned;
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

		piece = black_pawn_capture_e(pos->black_pieces[pawn], pos->white_pieces[all]) & pinned;
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

		piece = black_pawn_capture_w(pos->black_pieces[pawn], pos->white_pieces[all]) & ~pinned;
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

		piece = black_pawn_capture_w(pos->black_pieces[pawn], pos->white_pieces[all]) & pinned;
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

			piece = black_pawn_capture_e(pos->black_pieces[pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->pieces ^= target_bitboard | shift_north(target_bitboard) | shift_north_west(target_bitboard);

				if (!(rook_attacks(king_square, pos->pieces) & (pos->white_pieces[rook] | pos->white_pieces[queen])) && !(bishop_attacks(king_square, pos->pieces) & (pos->white_pieces[bishop] | pos->white_pieces[queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

				pos->pieces ^= target_bitboard | shift_north(target_bitboard) | shift_north_west(target_bitboard);

			}

			piece = black_pawn_capture_e(pos->black_pieces[pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}

			piece = black_pawn_capture_w(pos->black_pieces[pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->pieces ^= target_bitboard | shift_north(target_bitboard) | shift_north_east(target_bitboard);

				if (!(rook_attacks(king_square, pos->pieces) & (pos->white_pieces[rook] | pos->white_pieces[queen])) && !(bishop_attacks(king_square, pos->pieces) & (pos->white_pieces[bishop] | pos->white_pieces[queen]))) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}

				pos->pieces ^= target_bitboard | shift_north(target_bitboard) | shift_north_east(target_bitboard);

			}

			piece = black_pawn_capture_w(pos->black_pieces[pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					*move_ptr++ = new_move(source_square, target_square, 1, 0);
				}
			}
		}

		piece = pos->black_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->black_pieces[all]);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces);
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		if (pos->castle & 0x4) {
			if (!(pos->pieces & 0x6000000000000000)) {
				if (!(attacked & 0x6000000000000000)) {
					*move_ptr++ = new_move(60, 62, 3, 0);
				}
			}

		}
		if (pos->castle & 0x8) {
			if (!(pos->pieces & 0xE00000000000000)) {
				if (!(attacked & 0xC00000000000000)) {
					*move_ptr++ = new_move(60, 58, 3, 0);
				}
			}
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = black_pawn_push(pos->black_pieces[pawn], pos->pieces) & shift_north(pinned_squares) & ~pinned;
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

		piece = black_pawn_double_push(pos->black_pieces[pawn], pos->pieces) & shift_north_north(pinned_squares) & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			*move_ptr++ = new_move(source_square, source_square - 16, 0, 0);
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->black_pieces[pawn], checkers) & ~pinned;
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

		piece = black_pawn_capture_w(pos->black_pieces[pawn], checkers) & ~pinned;
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

			piece = black_pawn_capture_e(pos->black_pieces[pawn], shift_south(checkers) & bitboard(target_square)) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}

			piece = black_pawn_capture_w(pos->black_pieces[pawn], shift_south(checkers) & bitboard(target_square)) & ~pinned;
			if (piece) {
				source_square = ctz(piece);
				*move_ptr++ = new_move(source_square, target_square, 1, 0);
			}
		}

		piece = pos->black_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->black_pieces[all]) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares;
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares;
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
	for (int i = 0; i < 256; i++)
		if (!m[i])
			return i;
	return 256;
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

	king_square = ctz(pos->white_pieces[king]);

	attacks = white_king_attacks(king_square, pos->white_pieces[all]) & ~attacked;
	if (attacks)
		return 0;

	uint64_t checkers = generate_checkers_white(pos);
	uint64_t pinned = generate_pinned_white(pos);
	
	if (!checkers) {
		piece = white_pawn_push(pos->white_pieces[pawn], pos->pieces) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_push(pos->white_pieces[pawn], pos->pieces) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_double_push(pos->white_pieces[pawn], pos->pieces) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_double_push(pos->white_pieces[pawn], pos->pieces) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_e(pos->white_pieces[pawn], pos->black_pieces[all]) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_capture_e(pos->white_pieces[pawn], pos->black_pieces[all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && source_square / 8 > king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = white_pawn_capture_w(pos->white_pieces[pawn], pos->black_pieces[all]) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_capture_w(pos->white_pieces[pawn], pos->black_pieces[all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && source_square / 8 > king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			uint64_t target_bitboard = bitboard(target_square);

			piece = white_pawn_capture_e(pos->white_pieces[pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->pieces ^= target_bitboard | shift_south(target_bitboard) | shift_south_west(target_bitboard);

				if (!(rook_attacks(king_square, pos->pieces) & (pos->black_pieces[rook] | pos->black_pieces[queen])) && !(bishop_attacks(king_square, pos->pieces) & (pos->black_pieces[bishop] | pos->black_pieces[queen]))) {
					return 0;
				}

				pos->pieces ^= target_bitboard | shift_south(target_bitboard) | shift_south_west(target_bitboard);

			}

			piece = white_pawn_capture_e(pos->white_pieces[pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					return 0;
				}
			}

			piece = white_pawn_capture_w(pos->white_pieces[pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->pieces ^= target_bitboard | shift_south(target_bitboard) | shift_south_east(target_bitboard);

				if (!(rook_attacks(king_square, pos->pieces) & (pos->black_pieces[rook] | pos->black_pieces[queen])) && !(bishop_attacks(king_square, pos->pieces) & (pos->black_pieces[bishop] | pos->black_pieces[queen]))) {
					return 0;
				}

				pos->pieces ^= target_bitboard | shift_south(target_bitboard) | shift_south_east(target_bitboard);

			}

			piece = white_pawn_capture_w(pos->white_pieces[pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					return 0;
				}
			}
		}

		piece = pos->white_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->white_pieces[all]);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = white_pawn_push(pos->white_pieces[pawn], pos->pieces) & shift_south(pinned_squares) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_double_push(pos->white_pieces[pawn], pos->pieces) & shift_south_south(pinned_squares) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_capture_e(pos->white_pieces[pawn], checkers) & ~pinned;
		if (piece)
			return 0;

		piece = white_pawn_capture_w(pos->white_pieces[pawn], checkers) & ~pinned;
		if (piece)
			return 0;

		if (pos->en_passant) {
			target_square = pos->en_passant;

			piece = white_pawn_capture_e(pos->white_pieces[pawn], shift_north(checkers) & bitboard(target_square)) & ~pinned;
			if (piece)
				return 0;

			piece = white_pawn_capture_w(pos->white_pieces[pawn], shift_north(checkers) & bitboard(target_square)) & ~pinned;
			if (piece)
				return 0;
		}

		piece = pos->white_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->white_pieces[all]) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares;
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

	king_square = ctz(pos->black_pieces[king]);

	attacks = black_king_attacks(king_square, pos->black_pieces[all]) & ~attacked;
	if (attacks)
		return 0;
	
	uint64_t checkers = generate_checkers_black(pos);
	uint64_t pinned = generate_pinned_black(pos);

	if (!checkers) {
		piece = black_pawn_push(pos->black_pieces[pawn], pos->pieces) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_push(pos->black_pieces[pawn], pos->pieces) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_double_push(pos->black_pieces[pawn], pos->pieces) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_double_push(pos->black_pieces[pawn], pos->pieces) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if ((source_square - king_square) % 8 == 0)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_e(pos->black_pieces[pawn], pos->white_pieces[all]) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_capture_e(pos->black_pieces[pawn], pos->white_pieces[all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 > king_square % 8 && source_square / 8 < king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = black_pawn_capture_w(pos->black_pieces[pawn], pos->white_pieces[all]) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_capture_w(pos->black_pieces[pawn], pos->white_pieces[all]) & pinned;
		while (piece) {
			source_square = ctz(piece);
			if (source_square % 8 < king_square % 8 && source_square / 8 < king_square / 8)
				return 0;
			piece = clear_ls1b(piece);
		}

		if (pos->en_passant) {
			target_square = pos->en_passant;

			uint64_t target_bitboard = bitboard(target_square);

			piece = black_pawn_capture_e(pos->black_pieces[pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->pieces ^= target_bitboard | shift_north(target_bitboard) | shift_north_west(target_bitboard);

				if (!(rook_attacks(king_square, pos->pieces) & (pos->white_pieces[rook] | pos->white_pieces[queen])) && !(bishop_attacks(king_square, pos->pieces) & (pos->white_pieces[bishop] | pos->white_pieces[queen])))
					return 0;

				pos->pieces ^= target_bitboard | shift_north(target_bitboard) | shift_north_west(target_bitboard);

			}

			piece = black_pawn_capture_e(pos->black_pieces[pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					return 0;
				}
			}

			piece = black_pawn_capture_w(pos->black_pieces[pawn], target_bitboard) & ~pinned;
			if (piece) {
				source_square = ctz(piece);

				pos->pieces ^= target_bitboard | shift_north(target_bitboard) | shift_north_east(target_bitboard);

				if (!(rook_attacks(king_square, pos->pieces) & (pos->white_pieces[rook] | pos->white_pieces[queen])) && !(bishop_attacks(king_square, pos->pieces) & (pos->white_pieces[bishop] | pos->white_pieces[queen]))) {
					return 0;
				}

				pos->pieces ^= target_bitboard | shift_north(target_bitboard) | shift_north_east(target_bitboard);

			}

			piece = black_pawn_capture_w(pos->black_pieces[pawn], target_bitboard) & pinned;
			if (piece) {
				source_square = ctz(piece);

				if (target_bitboard & line_lookup[source_square + 64 * king_square]) {
					return 0;
				}
			}
		}

		piece = pos->black_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->black_pieces[all]);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces);
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square];
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}
	}
	else if (!(checkers & (checkers - 1))) {
		source_square = ctz(checkers);
		pinned_squares = between_lookup[source_square + 64 * king_square] | checkers;
		piece = black_pawn_push(pos->black_pieces[pawn], pos->pieces) & shift_north(pinned_squares) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_double_push(pos->black_pieces[pawn], pos->pieces) & shift_north_north(pinned_squares) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_capture_e(pos->black_pieces[pawn], checkers) & ~pinned;
		if (piece)
			return 0;

		piece = black_pawn_capture_w(pos->black_pieces[pawn], checkers) & ~pinned;
		if (piece)
			return 0;

		if (pos->en_passant) {
			target_square = pos->en_passant;

			piece = black_pawn_capture_e(pos->black_pieces[pawn], shift_south(checkers) & bitboard(target_square)) & ~pinned;
			if (piece)
				return 0;

			piece = black_pawn_capture_w(pos->black_pieces[pawn], shift_south(checkers) & bitboard(target_square)) & ~pinned;
			if (piece)
				return 0;
		}

		piece = pos->black_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->black_pieces[all]) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares;
			if (attacks)
				return 0;
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares;
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

	king_square = ctz(pos->white_pieces[king]);

	attacks = white_king_attacks(king_square, pos->white_pieces[all]) & ~attacked & pos->black_pieces[all];
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}

	if (!checkers) {
		piece = white_pawn_capture_e(pos->white_pieces[pawn], pos->black_pieces[all]) & ~pinned;
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

		piece = white_pawn_capture_e(pos->white_pieces[pawn], pos->black_pieces[all]) & pinned;
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

		piece = white_pawn_capture_w(pos->white_pieces[pawn], pos->black_pieces[all]) & ~pinned;
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

		piece = white_pawn_capture_w(pos->white_pieces[pawn], pos->black_pieces[all]) & pinned;
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

		piece = pos->white_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->white_pieces[all]) & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces) & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square] & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces) & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square] & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces) & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square] & pos->black_pieces[all];
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

		piece = white_pawn_capture_e(pos->white_pieces[pawn], checkers) & ~pinned;
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

		piece = white_pawn_capture_w(pos->white_pieces[pawn], checkers) & ~pinned;
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

		piece = pos->white_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_knight_attacks(source_square, pos->white_pieces[all]) & pinned_squares & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);
				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_bishop_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_rook_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares & pos->black_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->white_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = white_queen_attacks(source_square, pos->white_pieces[all], pos->pieces) & pinned_squares & pos->black_pieces[all];
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

	king_square = ctz(pos->black_pieces[king]);

	attacks = black_king_attacks(king_square, pos->black_pieces[all]) & ~attacked & pos->white_pieces[all];
	while (attacks) {
		target_square = ctz(attacks);

		*move_ptr++ = new_move(king_square, target_square, 0, 0);

		attacks = clear_ls1b(attacks);
	}
	
	if (!checkers) {
		piece = black_pawn_capture_e(pos->black_pieces[pawn], pos->white_pieces[all]) & ~pinned;
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

		piece = black_pawn_capture_e(pos->black_pieces[pawn], pos->white_pieces[all]) & pinned;
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

		piece = black_pawn_capture_w(pos->black_pieces[pawn], pos->white_pieces[all]) & ~pinned;
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

		piece = black_pawn_capture_w(pos->black_pieces[pawn], pos->white_pieces[all]) & pinned;
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

		piece = pos->black_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->black_pieces[all]) & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces) & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square] & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces) & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square] & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces) & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces) & line_lookup[source_square + 64 * king_square] & pos->white_pieces[all];
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

		piece = black_pawn_capture_e(pos->black_pieces[pawn], checkers) & ~pinned;
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

		piece = black_pawn_capture_w(pos->black_pieces[pawn], checkers) & ~pinned;
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

		piece = pos->black_pieces[knight] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_knight_attacks(source_square, pos->black_pieces[all]) & pinned_squares & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[bishop] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_bishop_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[rook] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_rook_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares & pos->white_pieces[all];
			while (attacks) {
				target_square = ctz(attacks);

				*move_ptr++ = new_move(source_square, target_square, 0, 0);

				attacks = clear_ls1b(attacks);
			}
			piece = clear_ls1b(piece);
		}

		piece = pos->black_pieces[queen] & ~pinned;
		while (piece) {
			source_square = ctz(piece);
			attacks = black_queen_attacks(source_square, pos->black_pieces[all], pos->pieces) & pinned_squares & pos->white_pieces[all];
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
