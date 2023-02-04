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
#include "attack_gen.h"

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
			pos->piece[black][pos->mailbox[target_square] - 6] ^= to;
			move_set_captured(m, pos->mailbox[target_square] - 6);
			pos->piece[black][all] ^= to;
		}

		if (pos->mailbox[source_square] == white_pawn) {
			pos->mailbox[target_square] = white_pawn;
			if (source_square + 16 == target_square) {
				pos->en_passant = target_square - 8;
			}
			else if (move_flag(m) == 1) {
				pos->piece[black][pawn] ^= bitboard(target_square - 8);
				pos->piece[black][all] ^= bitboard(target_square - 8);
				pos->mailbox[target_square - 8] = empty;

			}
			else if (move_flag(m) == 2) {
				pos->piece[white][pawn] ^= to;
				pos->piece[white][move_promote(m) + 2] ^= to;
				pos->mailbox[target_square] = move_promote(m) + 2;
			}
			pos->piece[white][pawn] ^= from_to;
		}
		else if (pos->mailbox[source_square] == white_king) {
			pos->piece[white][king] ^= from_to;
			if (move_flag(m) == 3) {
				if (target_square == 6) {
					pos->piece[white][rook] ^= 0xA0;
					pos->piece[white][all] ^= 0xA0;
					pos->mailbox[h1] = empty;
					pos->mailbox[f1] = white_rook;
				}
				else if (target_square == 2) {
					pos->piece[white][rook] ^= 0x9;
					pos->piece[white][all] ^= 0x9;
					pos->mailbox[a1] = empty;
					pos->mailbox[d1] = white_rook;
				}
			}
			pos->mailbox[target_square] = white_king;
		}
		else {
			pos->piece[white][pos->mailbox[source_square]] ^= from_to;
			pos->mailbox[target_square] = pos->mailbox[source_square];
		}

		pos->piece[white][all] ^= from_to;
		pos->mailbox[source_square] = empty;
	}
	else {
		if (pos->mailbox[target_square]) {
			pos->piece[white][pos->mailbox[target_square]] ^= to;
			move_set_captured(m, pos->mailbox[target_square]);
			pos->piece[white][all] ^= to;
		}

		if (pos->mailbox[source_square] == black_pawn) {
			pos->mailbox[target_square] = black_pawn;
			if (source_square - 16 == target_square) {
				pos->en_passant = target_square + 8;
			}
			else if (move_flag(m) == 1) {
				pos->piece[white][pawn] ^= bitboard(target_square + 8);
				pos->piece[white][all] ^= bitboard(target_square + 8);
				pos->mailbox[target_square + 8] = 0;
			}
			else if (move_flag(m) == 2) {
				pos->piece[black][pawn] ^= to;
				pos->piece[black][move_promote(m) + 2] ^= to;
				pos->mailbox[target_square] = move_promote(m) + 8;
			}
			pos->piece[black][pawn] ^= from_to;
		}
		else if (pos->mailbox[source_square] == black_king) {
			pos->piece[black][king] ^= from_to;
			if (move_flag(m) == 3) {
				if (target_square == 62) {
					pos->piece[black][rook] ^= 0xA000000000000000;
					pos->piece[black][all] ^= 0xA000000000000000;
					pos->mailbox[h8] = empty;
					pos->mailbox[f8] = black_rook;
				}
				else if (target_square == 58) {
					pos->piece[black][rook] ^= 0x900000000000000;
					pos->piece[black][all] ^= 0x900000000000000;
					pos->mailbox[a8] = empty;
					pos->mailbox[d8] = black_rook;
				}
			}
			pos->mailbox[target_square] = black_king;
		}
		else {
			pos->piece[black][pos->mailbox[source_square] - 6] ^= from_to;
			pos->mailbox[target_square] = pos->mailbox[source_square];
		}

		pos->piece[black][all] ^= from_to;
		pos->mailbox[source_square] = empty;
	}
	pos->piece_all = pos->piece[white][all] | pos->piece[black][all];

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
		pos->piece[black][pos->mailbox[target_square] - 6] ^= from_to;
		if (move_flag(m) == 1) {
			pos->piece[white][pawn] |= bitboard(target_square + 8);
			pos->piece[white][all] |= pos->piece[white][pawn];
			pos->mailbox[target_square + 8] = white_pawn;
		}
		else if (move_flag(m) == 2) {
			pos->piece[black][pawn] ^= from;
			pos->piece[black][pos->mailbox[target_square] - 6] ^= from;
			pos->mailbox[target_square] = black_pawn;
		}
		else if (move_flag(m) == 3) {
			if (target_square == 62) {
				pos->piece[black][rook] ^= 0xA000000000000000;
				pos->piece[black][all] ^= 0xA000000000000000;
				pos->mailbox[h8] = black_rook;
				pos->mailbox[f8] = empty;
			}
			else if (target_square == 58) {
				pos->piece[black][rook] ^= 0x900000000000000;
				pos->piece[black][all] ^= 0x900000000000000;
				pos->mailbox[a8] = black_rook;
				pos->mailbox[d8] = empty;
			}
		}

		pos->mailbox[source_square] = pos->mailbox[target_square];
		pos->mailbox[target_square] = empty;

		if (move_capture(m)) {
			pos->piece[white][move_capture(m)] ^= to;
			pos->piece[white][all] ^= to;
			pos->mailbox[target_square] = move_capture(m);
		}
		pos->piece[black][all] ^= from_to;
	}
	else {
		pos->piece[white][pos->mailbox[target_square]] ^= from_to;
		if (move_flag(m) == 1) {
			pos->piece[black][pawn] |= bitboard(target_square - 8);
			pos->piece[black][all] |= pos->piece[black][pawn];
			pos->mailbox[target_square - 8] = black_pawn;
		}
		else if (move_flag(m) == 2) {
			pos->piece[white][pawn] ^= from;
			pos->piece[white][pos->mailbox[target_square]] ^= from;
			pos->mailbox[target_square] = white_pawn;
		}
		else if (move_flag(m) == 3) {
			if (target_square == 6) {
				pos->piece[white][rook] ^= 0xA0;
				pos->piece[white][all] ^= 0xA0;
				pos->mailbox[h1] = white_rook;
				pos->mailbox[f1] = empty;
			}
			else if (target_square == 2) {
				pos->piece[white][rook] ^= 0x9;
				pos->piece[white][all] ^= 0x9;
				pos->mailbox[a1] = white_rook;
				pos->mailbox[d1] = empty;
			}
		}

		pos->mailbox[source_square] = pos->mailbox[target_square];
		pos->mailbox[target_square] = empty;

		if (move_capture(m)) {
			pos->piece[black][move_capture(m)] ^= to;
			pos->piece[black][all] ^= to;
			pos->mailbox[target_square] = move_capture(m) + 6;
		}
		pos->piece[white][all] ^= from_to;
	}

	pos->turn = 1 - pos->turn;
	pos->piece_all = pos->piece[white][all] | pos->piece[black][all];
}

void do_move(struct position *pos, move *m) {
	uint8_t source_square = move_from(m);
	uint8_t target_square = move_to(m);

	uint64_t from = bitboard(source_square);
	uint64_t to = bitboard(target_square);
	uint64_t from_to = from | to;

	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	move_set_castle(m, pos->castle);
	move_set_en_passant(m, pos->en_passant);
	move_set_halfmove(m, pos->halfmove);

	pos->halfmove++;
	pos->en_passant = 0;

	pos->castle = castle(source_square, target_square, pos->castle);

	if (pos->turn) {
		if (pos->mailbox[target_square]) {
			pos->piece[black][pos->mailbox[target_square] - 6] ^= to;
			move_set_captured(m, pos->mailbox[target_square] - 6);
			pos->piece[black][all] ^= to;
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);

			pos->halfmove = 0;
		}

		if (pos->mailbox[source_square] == white_pawn) {
			pos->mailbox[target_square] = white_pawn;
			if (source_square + 16 == target_square) {
				pos->zobrist_key ^= zobrist_en_passant_key(target_square - 8);
				pos->en_passant = target_square - 8;
			}
			else if (move_flag(m) == 1) {
				pos->piece[black][pawn] ^= bitboard(target_square - 8);
				pos->piece[black][all] ^= bitboard(target_square - 8);
				pos->mailbox[target_square - 8] = empty;
				pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square - 8);

			}
			else if (move_flag(m) == 2) {
				pos->piece[white][pawn] ^= to;
				pos->piece[white][move_promote(m) + 2] ^= to;
				pos->mailbox[target_square] = move_promote(m) + 2;
				pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square);
				pos->zobrist_key ^= zobrist_piece_key(move_promote(m) + 1, target_square);
			}
			pos->piece[white][pawn] ^= from_to;
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square);

			pos->halfmove = 0;
		}
		else if (pos->mailbox[source_square] == white_king) {
			pos->piece[white][king] ^= from_to;
			if (move_flag(m) == 3) {
				if (target_square == 6) {
					pos->piece[white][rook] ^= 0xA0;
					pos->piece[white][all] ^= 0xA0;
					pos->mailbox[h1] = empty;
					pos->mailbox[f1] = white_rook;
					pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, h1);
					pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, f1);
				}
				else if (target_square == 2) {
					pos->piece[white][rook] ^= 0x9;
					pos->piece[white][all] ^= 0x9;
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
			pos->piece[white][pos->mailbox[source_square]] ^= from_to;
			pos->mailbox[target_square] = pos->mailbox[source_square];
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}

		pos->piece[white][all] ^= from_to;
		pos->mailbox[source_square] = empty;
	}
	else {
		pos->fullmove++;
		if (pos->mailbox[target_square]) {
			pos->piece[white][pos->mailbox[target_square]] ^= to;
			move_set_captured(m, pos->mailbox[target_square]);
			pos->piece[white][all] ^= to;
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);

			pos->halfmove = 0;
		}

		if (pos->mailbox[source_square] == black_pawn) {
			pos->mailbox[target_square] = black_pawn;
			if (source_square - 16 == target_square) {
				pos->en_passant = target_square + 8;
				pos->zobrist_key ^= zobrist_en_passant_key(target_square + 8);
			}
			else if (move_flag(m) == 1) {
				pos->piece[white][pawn] ^= bitboard(target_square + 8);
				pos->piece[white][all] ^= bitboard(target_square + 8);
				pos->mailbox[target_square + 8] = 0;
				pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square + 8);
			}
			else if (move_flag(m) == 2) {
				pos->piece[black][pawn] ^= to;
				pos->piece[black][move_promote(m) + 2] ^= to;
				pos->mailbox[target_square] = move_promote(m) + 8;
				pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square);
				pos->zobrist_key ^= zobrist_piece_key(move_promote(m) + 7, target_square);
			}
			pos->piece[black][pawn] ^= from_to;
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square);

			pos->halfmove = 0;
		}
		else if (pos->mailbox[source_square] == black_king) {
			pos->piece[black][king] ^= from_to;
			if (move_flag(m) == 3) {
				if (target_square == 62) {
					pos->piece[black][rook] ^= 0xA000000000000000;
					pos->piece[black][all] ^= 0xA000000000000000;
					pos->mailbox[h8] = empty;
					pos->mailbox[f8] = black_rook;
					pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, h8);
					pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, f8);
				}
				else if (target_square == 58) {
					pos->piece[black][rook] ^= 0x900000000000000;
					pos->piece[black][all] ^= 0x900000000000000;
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
			pos->piece[black][pos->mailbox[source_square] - 6] ^= from_to;
			pos->mailbox[target_square] = pos->mailbox[source_square];
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, source_square);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}

		pos->piece[black][all] ^= from_to;
		pos->mailbox[source_square] = empty;
	}
	pos->piece_all = pos->piece[white][all] | pos->piece[black][all];

	pos->turn = 1 - pos->turn;
	pos->zobrist_key ^= zobrist_turn_key();
	pos->zobrist_key ^= zobrist_castle_key(pos->castle);

	if (pos->castle != move_castle(m))
		pos->halfmove = 0;
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

	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->en_passant = move_en_passant(m);
	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);

	pos->halfmove = move_halfmove(m);

	if (pos->turn) {
		pos->fullmove--;
		pos->piece[black][pos->mailbox[target_square] - 6] ^= from_to;
		if (move_flag(m) == 1) {
			pos->piece[white][pawn] |= bitboard(target_square + 8);
			pos->piece[white][all] |= pos->piece[white][pawn];
			pos->mailbox[target_square + 8] = white_pawn;
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square + 8);
		}
		else if (move_flag(m) == 2) {
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square);
			pos->piece[black][pawn] ^= from;
			pos->piece[black][pos->mailbox[target_square] - 6] ^= from;
			pos->mailbox[target_square] = black_pawn;
		}
		else if (move_flag(m) == 3) {
			if (target_square == 62) {
				pos->piece[black][rook] ^= 0xA000000000000000;
				pos->piece[black][all] ^= 0xA000000000000000;
				pos->mailbox[h8] = black_rook;
				pos->mailbox[f8] = empty;
				pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, h8);
				pos->zobrist_key ^= zobrist_piece_key(black_rook - 1, f8);
			}
			else if (target_square == 58) {
				pos->piece[black][rook] ^= 0x900000000000000;
				pos->piece[black][all] ^= 0x900000000000000;
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
			pos->piece[white][move_capture(m)] ^= to;
			pos->piece[white][all] ^= to;
			pos->mailbox[target_square] = move_capture(m);
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}
		pos->piece[black][all] ^= from_to;
	}
	else {
		pos->piece[white][pos->mailbox[target_square]] ^= from_to;
		if (move_flag(m) == 1) {
			pos->piece[black][pawn] |= bitboard(target_square - 8);
			pos->piece[black][all] |= pos->piece[black][pawn];
			pos->mailbox[target_square - 8] = black_pawn;
			pos->zobrist_key ^= zobrist_piece_key(black_pawn - 1, target_square - 8);
		}
		else if (move_flag(m) == 2) {
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
			pos->zobrist_key ^= zobrist_piece_key(white_pawn - 1, target_square);
			pos->piece[white][pawn] ^= from;
			pos->piece[white][pos->mailbox[target_square]] ^= from;
			pos->mailbox[target_square] = white_pawn;
		}
		else if (move_flag(m) == 3) {
			if (target_square == 6) {
				pos->piece[white][rook] ^= 0xA0;
				pos->piece[white][all] ^= 0xA0;
				pos->mailbox[h1] = white_rook;
				pos->mailbox[f1] = empty;
				pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, h1);
				pos->zobrist_key ^= zobrist_piece_key(white_rook - 1, f1);
			}
			else if (target_square == 2) {
				pos->piece[white][rook] ^= 0x9;
				pos->piece[white][all] ^= 0x9;
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
			pos->piece[black][move_capture(m)] ^= to;
			pos->piece[black][all] ^= to;
			pos->mailbox[target_square] = move_capture(m) + 6;
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[target_square] - 1, target_square);
		}
		pos->piece[white][all] ^= from_to;
	}

	pos->turn = 1 - pos->turn;
	pos->piece_all = pos->piece[white][all] | pos->piece[black][all];
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

/* str needs to be at least 6 bytes */
char *move_str_algebraic(char *str, move *m) {
	algebraic(str, move_from(m));
	algebraic(str + 2, move_to(m));
	str[4] = str[5] = '\0';
	if (move_flag(m) == 2)
		str[4] = "nbrq"[move_promote(m)];
	return str;
}

/* str needs to be at least 8 bytes */
/* m can be illegal */
char *move_str_pgn(char *str, struct position *pos, move *m) {
	if (!is_legal(pos, m))
		return NULL;
	int x = move_from(m) % 8;
	int y = move_from(m) / 8;
	int i = 0;
	uint64_t attackers = 0;

	str[i] = '\0';
	switch((pos->mailbox[move_from(m)] - 1) % 6) {
	case 0:
		if (pos->mailbox[move_to(m)] || move_flag(m) == 1)
			attackers = rank(move_from(m));
		break;
	case 1:
		str[i++] = 'N';
		attackers = knight_attacks(move_to(m)) &
			(pos->turn ? pos->piece[white][knight] : pos->piece[black][knight]);
		break;
	case 2:
		str[i++] = 'B';
		attackers = bishop_attacks(move_to(m), pos->piece_all) &
			(pos->turn ? pos->piece[white][bishop] : pos->piece[black][bishop]);
		break;
	case 3:
		str[i++] = 'R';
		attackers = rook_attacks(move_to(m), pos->piece_all) &
			(pos->turn ? pos->piece[white][rook] : pos->piece[black][rook]);
		break;
	case 4:
		str[i++] = 'Q';
		attackers = queen_attacks(move_to(m), pos->piece_all) &
			(pos->turn ? pos->piece[white][queen] : pos->piece[black][queen]);
		break;
	case 5:
		if (x == 4 && move_to(m) % 8 == 6) {
			sprintf(str, "O-O");
			i = 3;
		}
		else if (x == 4 && move_to(m) % 8 == 2) {
			sprintf(str, "O-O-O");
			i = 5;
		}
		else {
			str[i++] = 'K';
		}
	}
	if (popcount(attackers & file(move_from(m))) > 1) {
		if (popcount(attackers & rank(move_from(m))) > 1) {
			str[i++] = "abcdefgh"[x];
			str[i++] = "12345678"[y];
		}
		else {
			str[i++] = "12345678"[y];
		}
	}
	else if (popcount(attackers) > 1) {
		str[i++] = "abcdefgh"[x];
	}

	if (pos->mailbox[move_to(m)] || move_flag(m) == 1)
		str[i++] = 'x';
	if (str[0] != 'O') {
		algebraic(str + i, move_to(m));
		i += 2;
	}

	if (move_flag(m) == 2) {
		str[i++] = '=';
		str[i++] = "NBRQ"[move_promote(m)];
	}
	
	do_move(pos, m);
	int ma = mate(pos);
	uint64_t checkers = generate_checkers(pos);
	undo_move(pos, m);
	if (ma == 2)
		str[i++] = '#';
	else if (ma == 1)
		str[i++] = '=';
	else if (checkers)
		str[i++] = '+';
	str[i] = '\0';
	return str;
}

/* str can be illegal */
move string_to_move(struct position *pos, char *str) {
	move move_list[MOVES_MAX];
	generate_all(pos, move_list);
	char str_t[8];
	for (move *move_ptr = move_list; *move_ptr; move_ptr++) {
		move_str_algebraic(str_t, move_ptr);
		if (strcmp(str_t, str) == 0)
			return *move_ptr;
		move_str_pgn(str_t, pos, move_ptr);
		if (strcmp(str_t, str) == 0)
			return *move_ptr;
	}
	return 0;
}

int is_legal(struct position *pos, move *m) {
	move move_list[MOVES_MAX];
	generate_all(pos, move_list);
	for (move *move_ptr = move_list; *move_ptr; move_ptr++)
		if ((*m & 0xFFFF) == (*move_ptr & 0xFFFF))
			return 1;
	return 0;
}

void do_null_move(struct position *pos, int en_passant) {
	pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
	pos->zobrist_key ^= zobrist_en_passant_key(en_passant);
	pos->en_passant = en_passant;

	pos->turn = 1 - pos->turn;
	pos->zobrist_key ^= zobrist_turn_key();
}
