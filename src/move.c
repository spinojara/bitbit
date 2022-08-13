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

#include "move.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "bitboard.h"
#include "util.h"
#include "move_gen.h"
#include "transposition_table.h"

void do_move_perft(struct position *pos, move *m) {
	uint8_t source_square = move_from(m);
	uint8_t target_square = move_to(m);

	uint64_t from = bitboard(source_square);
	uint64_t to = bitboard(target_square);
	uint64_t from_to = from | to;

	move_set_castle(m, pos->castle);
	move_set_en_passant(m, pos->en_passant);

	pos->en_passant = 0;

	pos->castle = castle(source_square, target_square, pos->castle);

	if (pos->turn) {
		if (pos->mailbox[target_square]) {
			pos->black_pieces[pos->mailbox[target_square] - 6] ^= to;
			move_set_captured(m, pos->mailbox[target_square] - 6);
			pos->black_pieces[all] ^= to;
		}

		if (pos->mailbox[source_square] == white_pawn) {
			pos->mailbox[target_square] = white_pawn;
			if (source_square + 16 == target_square) {
				pos->en_passant = target_square - 8;
			}
			else if (move_flag(m) == 1) {
				pos->black_pieces[pawn] ^= bitboard(target_square - 8);
				pos->black_pieces[all] ^= bitboard(target_square - 8);
				pos->mailbox[target_square - 8] = empty;

			}
			else if (move_flag(m) == 2) {
				pos->white_pieces[pawn] ^= to;
				pos->white_pieces[move_promote(m) + 2] ^= to;
				pos->mailbox[target_square] = move_promote(m) + 2;
			}
			pos->white_pieces[pawn] ^= from_to;
		}
		else if (pos->mailbox[source_square] == white_king) {
			pos->white_pieces[king] ^= from_to;
			if (move_flag(m) == 3) {
				if (target_square == 6) {
					pos->white_pieces[rook] ^= 0xA0;
					pos->white_pieces[all] ^= 0xA0;
					pos->mailbox[h1] = empty;
					pos->mailbox[f1] = white_rook;
				}
				else if (target_square == 2) {
					pos->white_pieces[rook] ^= 0x9;
					pos->white_pieces[all] ^= 0x9;
					pos->mailbox[a1] = empty;
					pos->mailbox[d1] = white_rook;
				}
			}
			pos->mailbox[target_square] = white_king;
		}
		else {
			pos->white_pieces[pos->mailbox[source_square]] ^= from_to;
			pos->mailbox[target_square] = pos->mailbox[source_square];
		}

		pos->white_pieces[all] ^= from_to;
		pos->mailbox[source_square] = empty;
	}
	else {
		if (pos->mailbox[target_square]) {
			pos->white_pieces[pos->mailbox[target_square]] ^= to;
			move_set_captured(m, pos->mailbox[target_square]);
			pos->white_pieces[all] ^= to;
		}

		if (pos->mailbox[source_square] == black_pawn) {
			pos->mailbox[target_square] = black_pawn;
			if (source_square - 16 == target_square) {
				pos->en_passant = target_square + 8;
			}
			else if (move_flag(m) == 1) {
				pos->white_pieces[pawn] ^= bitboard(target_square + 8);
				pos->white_pieces[all] ^= bitboard(target_square + 8);
				pos->mailbox[target_square + 8] = 0;
			}
			else if (move_flag(m) == 2) {
				pos->black_pieces[pawn] ^= to;
				pos->black_pieces[move_promote(m) + 2] ^= to;
				pos->mailbox[target_square] = move_promote(m) + 8;
			}
			pos->black_pieces[pawn] ^= from_to;
		}
		else if (pos->mailbox[source_square] == black_king) {
			pos->black_pieces[king] ^= from_to;
			if (move_flag(m) == 3) {
				if (target_square == 62) {
					pos->black_pieces[rook] ^= 0xA000000000000000;
					pos->black_pieces[all] ^= 0xA000000000000000;
					pos->mailbox[h8] = empty;
					pos->mailbox[f8] = black_rook;
				}
				else if (target_square == 58) {
					pos->black_pieces[rook] ^= 0x900000000000000;
					pos->black_pieces[all] ^= 0x900000000000000;
					pos->mailbox[a8] = empty;
					pos->mailbox[d8] = black_rook;
				}
			}
			pos->mailbox[target_square] = black_king;
		}
		else {
			pos->black_pieces[pos->mailbox[source_square] - 6] ^= from_to;
			pos->mailbox[target_square] = pos->mailbox[source_square];
		}

		pos->black_pieces[all] ^= from_to;
		pos->mailbox[source_square] = empty;
	}
	pos->pieces = pos->white_pieces[all] | pos->black_pieces[all];

	pos->turn = 1 - pos->turn;
}

void undo_move_perft(struct position *pos, move *m) {
	uint8_t source_square = move_from(m);
	uint8_t target_square = move_to(m);

	uint64_t from = bitboard(source_square);
	uint64_t to = bitboard(target_square);
	uint64_t from_to = from | to;

	pos->castle = move_castle(m);

	pos->en_passant = move_en_passant(m);

	if (pos->turn) {
		pos->black_pieces[pos->mailbox[target_square] - 6] ^= from_to;
		if (move_flag(m) == 1) {
			pos->white_pieces[pawn] |= bitboard(target_square + 8);
			pos->white_pieces[all] |= pos->white_pieces[pawn];
			pos->mailbox[target_square + 8] = white_pawn;
		}
		else if (move_flag(m) == 2) {
			pos->black_pieces[pawn] ^= from;
			pos->black_pieces[pos->mailbox[target_square] - 6] ^= from;
			pos->mailbox[target_square] = black_pawn;
		}
		else if (move_flag(m) == 3) {
			if (target_square == 62) {
				pos->black_pieces[rook] ^= 0xA000000000000000;
				pos->black_pieces[all] ^= 0xA000000000000000;
				pos->mailbox[h8] = black_rook;
				pos->mailbox[f8] = empty;
			}
			else if (target_square == 58) {
				pos->black_pieces[rook] ^= 0x900000000000000;
				pos->black_pieces[all] ^= 0x900000000000000;
				pos->mailbox[a8] = black_rook;
				pos->mailbox[d8] = empty;
			}
		}

		pos->mailbox[source_square] = pos->mailbox[target_square];
		pos->mailbox[target_square] = empty;

		if (move_capture(m)) {
			pos->white_pieces[move_capture(m)] ^= to;
			pos->white_pieces[all] ^= to;
			pos->mailbox[target_square] = move_capture(m);
		}
		pos->black_pieces[all] ^= from_to;
	}
	else {
		pos->white_pieces[pos->mailbox[target_square]] ^= from_to;
		if (move_flag(m) == 1) {
			pos->black_pieces[pawn] |= bitboard(target_square - 8);
			pos->black_pieces[all] |= pos->black_pieces[pawn];
			pos->mailbox[target_square - 8] = black_pawn;
		}
		else if (move_flag(m) == 2) {
			pos->white_pieces[pawn] ^= from;
			pos->white_pieces[pos->mailbox[target_square]] ^= from;
			pos->mailbox[target_square] = white_pawn;
		}
		else if (move_flag(m) == 3) {
			if (target_square == 6) {
				pos->white_pieces[rook] ^= 0xA0;
				pos->white_pieces[all] ^= 0xA0;
				pos->mailbox[h1] = white_rook;
				pos->mailbox[f1] = empty;
			}
			else if (target_square == 2) {
				pos->white_pieces[rook] ^= 0x9;
				pos->white_pieces[all] ^= 0x9;
				pos->mailbox[a1] = white_rook;
				pos->mailbox[d1] = empty;
			}
		}

		pos->mailbox[source_square] = pos->mailbox[target_square];
		pos->mailbox[target_square] = empty;

		if (move_capture(m)) {
			pos->black_pieces[move_capture(m)] ^= to;
			pos->black_pieces[all] ^= to;
			pos->mailbox[target_square] = move_capture(m) + 6;
		}
		pos->white_pieces[all] ^= from_to;
	}

	pos->turn = 1 - pos->turn;
	pos->pieces = pos->white_pieces[all] | pos->black_pieces[all];
}

void do_move(struct position *pos, move *m) {
	uint8_t source_square = move_from(m);
	uint8_t target_square = move_to(m);

	uint64_t from = bitboard(source_square);
	uint64_t to = bitboard(target_square);
	uint64_t from_to = from | to;

	if (pos->en_passant)
		pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	move_set_castle(m, pos->castle);
	move_set_en_passant(m, pos->en_passant);

	pos->en_passant = 0;

	pos->castle = castle(source_square, target_square, pos->castle);

	if (pos->turn) {
		if (pos->mailbox[target_square]) {
			pos->black_pieces[pos->mailbox[target_square] - 6] ^= to;
			move_set_captured(m, pos->mailbox[target_square] - 6);
			pos->black_pieces[all] ^= to;
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}

		if (pos->mailbox[source_square] == white_pawn) {
			pos->mailbox[target_square] = white_pawn;
			if (source_square + 16 == target_square) {
				pos->zobrist_key ^= zobrist_en_passant_key(target_square - 8);
				pos->en_passant = target_square - 8;
			}
			else if (move_flag(m) == 1) {
				pos->black_pieces[pawn] ^= bitboard(target_square - 8);
				pos->black_pieces[all] ^= bitboard(target_square - 8);
				pos->mailbox[target_square - 8] = empty;
				pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square - 8);

			}
			else if (move_flag(m) == 2) {
				pos->white_pieces[pawn] ^= to;
				pos->white_pieces[move_promote(m) + 2] ^= to;
				pos->mailbox[target_square] = move_promote(m) + 2;
				pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square);
				pos->zobrist_key ^= zobrist_piece_key(move_promote(m) + 1, target_square);
			}
			pos->white_pieces[pawn] ^= from_to;
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square);
		}
		else if (pos->mailbox[source_square] == white_king) {
			pos->white_pieces[king] ^= from_to;
			if (move_flag(m) == 3) {
				if (target_square == 6) {
					pos->white_pieces[rook] ^= 0xA0;
					pos->white_pieces[all] ^= 0xA0;
					pos->mailbox[h1] = empty;
					pos->mailbox[f1] = white_rook;
					pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, h1);
					pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, f1);
				}
				else if (target_square == 2) {
					pos->white_pieces[rook] ^= 0x9;
					pos->white_pieces[all] ^= 0x9;
					pos->mailbox[a1] = empty;
					pos->mailbox[d1] = white_rook;
					pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, a1);
					pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, d1);
				}
			}
			pos->mailbox[target_square] = white_king;
			pos->zobrist_key ^= zobrist_piece_key(white_king - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(white_king - 1, target_square);
		}
		else {
			pos->white_pieces[pos->mailbox[source_square]] ^= from_to;
			pos->mailbox[target_square] = pos->mailbox[source_square];
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}

		pos->white_pieces[all] ^= from_to;
		pos->mailbox[source_square] = empty;
	}
	else {
		pos->fullmove++;
		if (pos->mailbox[target_square]) {
			pos->white_pieces[pos->mailbox[target_square]] ^= to;
			move_set_captured(m, pos->mailbox[target_square]);
			pos->white_pieces[all] ^= to;
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}

		if (pos->mailbox[source_square] == black_pawn) {
			pos->mailbox[target_square] = black_pawn;
			if (source_square - 16 == target_square) {
				pos->en_passant = target_square + 8;
				pos->zobrist_key ^= zobrist_en_passant_key(target_square + 8);
			}
			else if (move_flag(m) == 1) {
				pos->white_pieces[pawn] ^= bitboard(target_square + 8);
				pos->white_pieces[all] ^= bitboard(target_square + 8);
				pos->mailbox[target_square + 8] = 0;
				pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square + 8);
			}
			else if (move_flag(m) == 2) {
				pos->black_pieces[pawn] ^= to;
				pos->black_pieces[move_promote(m) + 2] ^= to;
				pos->mailbox[target_square] = move_promote(m) + 8;
				pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square);
				pos->zobrist_key ^= zobrist_piece_key(move_promote(m) + 7, target_square);
			}
			pos->black_pieces[pawn] ^= from_to;
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square);
		}
		else if (pos->mailbox[source_square] == black_king) {
			pos->black_pieces[king] ^= from_to;
			if (move_flag(m) == 3) {
				if (target_square == 62) {
					pos->black_pieces[rook] ^= 0xA000000000000000;
					pos->black_pieces[all] ^= 0xA000000000000000;
					pos->mailbox[h8] = empty;
					pos->mailbox[f8] = black_rook;
					pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, h8);
					pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, f8);
				}
				else if (target_square == 58) {
					pos->black_pieces[rook] ^= 0x900000000000000;
					pos->black_pieces[all] ^= 0x900000000000000;
					pos->mailbox[a8] = empty;
					pos->mailbox[d8] = black_rook;
					pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, a8);
					pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, d8);
				}
			}
			pos->mailbox[target_square] = black_king;
			pos->zobrist_key ^= zobrist_piece_key(black_king - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(black_king - 1, target_square);
		}
		else {
			pos->black_pieces[pos->mailbox[source_square] - 6] ^= from_to;
			pos->mailbox[target_square] = pos->mailbox[source_square];
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}

		pos->black_pieces[all] ^= from_to;
		pos->mailbox[source_square] = empty;
	}
	pos->pieces = pos->white_pieces[all] | pos->black_pieces[all];

	pos->turn = 1 - pos->turn;
	pos->zobrist_key ^= zobrist_turn_key();
	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
}

void undo_move(struct position *pos, move *m) {
	uint8_t source_square = move_from(m);
	uint8_t target_square = move_to(m);

	uint64_t from = bitboard(source_square);
	uint64_t to = bitboard(target_square);
	uint64_t from_to = from | to;

	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	pos->zobrist_key ^= zobrist_castle_key(move_castle(m));
	pos->castle = move_castle(m);

	if (pos->en_passant)
		pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->en_passant = move_en_passant(m);
	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);

	if (pos->turn) {
		pos->fullmove--;
		pos->black_pieces[pos->mailbox[target_square] - 6] ^= from_to;
		if (move_flag(m) == 1) {
			pos->white_pieces[pawn] |= bitboard(target_square + 8);
			pos->white_pieces[all] |= pos->white_pieces[pawn];
			pos->mailbox[target_square + 8] = white_pawn;
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square + 8);
		}
		else if (move_flag(m) == 2) {
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square);
			pos->black_pieces[pawn] ^= from;
			pos->black_pieces[pos->mailbox[target_square] - 6] ^= from;
			pos->mailbox[target_square] = black_pawn;
		}
		else if (move_flag(m) == 3) {
			if (target_square == 62) {
				pos->black_pieces[rook] ^= 0xA000000000000000;
				pos->black_pieces[all] ^= 0xA000000000000000;
				pos->mailbox[h8] = black_rook;
				pos->mailbox[f8] = empty;
				pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, h8);
				pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, f8);
			}
			else if (target_square == 58) {
				pos->black_pieces[rook] ^= 0x900000000000000;
				pos->black_pieces[all] ^= 0x900000000000000;
				pos->mailbox[a8] = black_rook;
				pos->mailbox[d8] = empty;
				pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, a8);
				pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, d8);
			}
		}

		pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
		pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		pos->mailbox[source_square] = pos->mailbox[target_square];
		pos->mailbox[target_square] = empty;

		if (move_capture(m)) {
			pos->white_pieces[move_capture(m)] ^= to;
			pos->white_pieces[all] ^= to;
			pos->mailbox[target_square] = move_capture(m);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}
		pos->black_pieces[all] ^= from_to;
	}
	else {
		pos->white_pieces[pos->mailbox[target_square]] ^= from_to;
		if (move_flag(m) == 1) {
			pos->black_pieces[pawn] |= bitboard(target_square - 8);
			pos->black_pieces[all] |= pos->black_pieces[pawn];
			pos->mailbox[target_square - 8] = black_pawn;
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square - 8);
		}
		else if (move_flag(m) == 2) {
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square);
			pos->white_pieces[pawn] ^= from;
			pos->white_pieces[pos->mailbox[target_square]] ^= from;
			pos->mailbox[target_square] = white_pawn;
		}
		else if (move_flag(m) == 3) {
			if (target_square == 6) {
				pos->white_pieces[rook] ^= 0xA0;
				pos->white_pieces[all] ^= 0xA0;
				pos->mailbox[h1] = white_rook;
				pos->mailbox[f1] = empty;
				pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, h1);
				pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, f1);
			}
			else if (target_square == 2) {
				pos->white_pieces[rook] ^= 0x9;
				pos->white_pieces[all] ^= 0x9;
				pos->mailbox[a1] = white_rook;
				pos->mailbox[d1] = empty;
				pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, a1);
				pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, d1);
			}
		}

		pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
		pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		pos->mailbox[source_square] = pos->mailbox[target_square];
		pos->mailbox[target_square] = empty;

		if (move_capture(m)) {
			pos->black_pieces[move_capture(m)] ^= to;
			pos->black_pieces[all] ^= to;
			pos->mailbox[target_square] = move_capture(m) + 6;
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}
		pos->white_pieces[all] ^= from_to;
	}

	pos->turn = 1 - pos->turn;
	pos->pieces = pos->white_pieces[all] | pos->black_pieces[all];
	pos->zobrist_key ^= zobrist_turn_key();
}

void print_move(move *m) {
	char move_from_str[3];
	char move_to_str[3];
	algebraic(move_from_str, move_from(m));
	algebraic(move_to_str, move_to(m));
	printf("%s%s", move_from_str, move_to_str);
	if (move_flag(m) == 2)
		printf("%c", "nbrq"[move_promote(m)]);
}

move string_to_move(struct position *pos, char *str) {
	if (strlen(str) < 4 || strlen(str) > 5)
		return 0;
	uint8_t move_from_t, move_to_t, promotion = 4;
	char tmp[3] = "\0\0\0";

	strncpy(tmp, str, 2);
	move_from_t = square(tmp);
	strncpy(tmp, str + 2, 2);
	move_to_t = square(tmp);

	if (strlen(str) == 5)
		promotion = find_char("nbrq", str[4]);

	move move_list[256];
	generate_all(pos, move_list);
	for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
		if (move_from(move_ptr) != move_from_t || move_to(move_ptr) != move_to_t)
			continue;
		if (promotion != 4) {
			if (move_flag(move_ptr) != 2)
				continue;
			else
				if (promotion != move_promote(move_ptr))
					continue;
		}

		return *move_ptr;
	}
	return 0;
}
