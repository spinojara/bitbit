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

#include "move.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include "bitboard.h"
#include "util.h"
#include "movegen.h"
#include "attackgen.h"

void do_move(struct position *pos, move_t *m) {
	assert(*m);
	int source_square = move_from(m);
	int target_square = move_to(m);
	assert(pos->mailbox[source_square]);
	assert(color_of_piece(pos->mailbox[source_square]) == pos->turn);
	assert(uncolored_piece(pos->mailbox[target_square]) != KING);
	assert(!pos->mailbox[target_square] || color_of_piece(pos->mailbox[target_square]) == other_color(pos->turn));

	uint64_t from = bitboard(source_square);
	uint64_t to = bitboard(target_square);
	uint64_t from_to = from | to;

	move_set_castle(m, pos->castle);
	move_set_en_passant(m, pos->en_passant);
	move_set_halfmove(m, pos->halfmove);

	pos->en_passant = 0;
	pos->halfmove++;

	pos->castle = castle(source_square, target_square, pos->castle);

	if (pos->mailbox[target_square]) {
		pos->piece[other_color(pos->turn)][uncolored_piece(pos->mailbox[target_square])] ^= to;
		move_set_captured(m, uncolored_piece(pos->mailbox[target_square]));
		pos->piece[other_color(pos->turn)][ALL] ^= to;
		pos->halfmove = 0;
	}

	if (source_square + 16 == target_square && pos->mailbox[source_square] == WHITE_PAWN)
		pos->en_passant = target_square - 8;
	if (source_square - 16 == target_square && pos->mailbox[source_square] == BLACK_PAWN)
		pos->en_passant = target_square + 8;

	if (uncolored_piece(pos->mailbox[source_square]) == PAWN)
		pos->halfmove = 0;

	pos->piece[pos->turn][uncolored_piece(pos->mailbox[source_square])] ^= from_to;
	pos->piece[pos->turn][ALL] ^= from_to;

	pos->mailbox[target_square] = pos->mailbox[source_square];
	pos->mailbox[source_square] = EMPTY;

	if (move_flag(m) == 1) {
		int direction = 2 * pos->turn - 1;
		pos->piece[other_color(pos->turn)][PAWN] ^= bitboard(target_square - direction * 8);
		pos->piece[other_color(pos->turn)][ALL] ^= bitboard(target_square - direction * 8);
		pos->mailbox[target_square - direction * 8] = EMPTY;
	}
	else if (move_flag(m) == 2) {
		pos->piece[pos->turn][PAWN] ^= to;
		pos->piece[pos->turn][move_promote(m) + 2] ^= to;
		pos->mailbox[target_square] = colored_piece(move_promote(m) + 2, pos->turn);
	}
	else if (move_flag(m) == 3) {
		switch (target_square) {
		case g1:
			pos->piece[WHITE][ROOK] ^= 0xA0;
			pos->piece[WHITE][ALL] ^= 0xA0;
			pos->mailbox[h1] = EMPTY;
			pos->mailbox[f1] = WHITE_ROOK;
			break;
		case c1:
			pos->piece[WHITE][ROOK] ^= 0x9;
			pos->piece[WHITE][ALL] ^= 0x9;
			pos->mailbox[a1] = EMPTY;
			pos->mailbox[d1] = WHITE_ROOK;
			break;
		case g8:
			pos->piece[BLACK][ROOK] ^= 0xA000000000000000;
			pos->piece[BLACK][ALL] ^= 0xA000000000000000;
			pos->mailbox[h8] = EMPTY;
			pos->mailbox[f8] = BLACK_ROOK;
			break;
		case c8:
			pos->piece[BLACK][ROOK] ^= 0x900000000000000;
			pos->piece[BLACK][ALL] ^= 0x900000000000000;
			pos->mailbox[a8] = EMPTY;
			pos->mailbox[d8] = BLACK_ROOK;
			break;
		}
	}

	if (!pos->turn)
		pos->fullmove++;
	pos->turn = other_color(pos->turn);
}

void undo_move(struct position *pos, const move_t *m) {
	assert(*m);
	int source_square = move_from(m);
	int target_square = move_to(m);
	assert(pos->mailbox[target_square]);

	uint64_t from = bitboard(source_square);
	uint64_t to = bitboard(target_square);
	uint64_t from_to = from | to;

	pos->castle = move_castle(m);
	pos->en_passant = move_en_passant(m);
	pos->halfmove = move_halfmove(m);
	pos->turn = other_color(pos->turn);

	if (move_flag(m) == 1) {
		int direction = 2 * pos->turn - 1;
		pos->piece[other_color(pos->turn)][PAWN] ^= bitboard(target_square - direction * 8);
		pos->piece[other_color(pos->turn)][ALL] ^= bitboard(target_square - direction * 8);
		pos->mailbox[target_square - direction * 8] = colored_piece(PAWN, other_color(pos->turn));
	}
	else if (move_flag(m) == 2) {
		pos->piece[pos->turn][PAWN] ^= to;
		pos->piece[pos->turn][uncolored_piece(pos->mailbox[target_square])] ^= to;
		/* Later gets updated as a normal move would. */
		pos->mailbox[target_square] = colored_piece(PAWN, pos->turn);
	}
	else if (move_flag(m) == 3) {
		switch (target_square) {
		case g1:
			pos->piece[WHITE][ROOK] ^= 0xA0;
			pos->piece[WHITE][ALL] ^= 0xA0;
			pos->mailbox[h1] = WHITE_ROOK;
			pos->mailbox[f1] = EMPTY;
			break;
		case c1:
			pos->piece[WHITE][ROOK] ^= 0x9;
			pos->piece[WHITE][ALL] ^= 0x9;
			pos->mailbox[a1] = WHITE_ROOK;
			pos->mailbox[d1] = EMPTY;
			break;
		case g8:
			pos->piece[BLACK][ROOK] ^= 0xA000000000000000;
			pos->piece[BLACK][ALL] ^= 0xA000000000000000;
			pos->mailbox[h8] = BLACK_ROOK;
			pos->mailbox[f8] = EMPTY;
			break;
		case c8:
			pos->piece[BLACK][ROOK] ^= 0x900000000000000;
			pos->piece[BLACK][ALL] ^= 0x900000000000000;
			pos->mailbox[a8] = BLACK_ROOK;
			pos->mailbox[d8] = EMPTY;
			break;
		}
	}

	pos->piece[pos->turn][uncolored_piece(pos->mailbox[target_square])] ^= from_to;
	pos->piece[pos->turn][ALL] ^= from_to;

	pos->mailbox[source_square] = pos->mailbox[target_square];
	pos->mailbox[target_square] = EMPTY;

	if (move_capture(m)) {
		pos->piece[other_color(pos->turn)][move_capture(m)] ^= to;
		pos->piece[other_color(pos->turn)][ALL] ^= to;
		pos->mailbox[target_square] = colored_piece(move_capture(m), other_color(pos->turn));
	}

	if (!pos->turn)
		pos->fullmove--;
}

int pseudo_legal(const struct position *pos, const struct pstate *pstate, const move_t *move) {
	if (!*move)
		return 0;
	const int us = pos->turn;
	const int them = other_color(us);
	unsigned down = us ? S : N;

	int from_square = move_from(move);
	int to_square = move_to(move);
	uint64_t from = bitboard(from_square);
	uint64_t to = bitboard(to_square);
	uint64_t all = all_pieces(pos);

	if (!(from & pos->piece[us][ALL]))
		return 0;

	if (to & pos->piece[us][ALL])
		return 0;

	int piece = uncolored_piece(pos->mailbox[from_square]);

	/* Promotions are handled after the switch. */
	switch (move_flag(move)) {
	case MOVE_EN_PASSANT:
		/* It could happen that pos->en_passant is 0 in which case
		 * to_square would have to be 0 as well. It would thus
		 * have to be a black pawn promotion but the flag is not
		 * the promotion flag. This error will thus get caught below.
		 */
		if (piece != PAWN || to_square != pos->en_passant)
			return 0;
		break;
	case MOVE_CASTLE:
		if (piece != KING || pstate->checkers)
			return 0;
		if (us) {
			/* from_square must be equal to e1 or e8 by the conditions
			 * pos->castle & x and piece == KING unless the position
			 * is faulty.
			 */
			if (pos->castle & 0x1 && to_square == g1 &&
					!(all & 0x60) && !(pstate->attacked & 0x60))
				return 1;
			if (pos->castle & 0x2 && to_square == c1 &&
					!(all & 0xE) && !(pstate->attacked & 0xC))
				return 1;
		}
		else {
			if (pos->castle & 0x4 && to_square == g8 &&
					!(all & 0x6000000000000000) && !(pstate->attacked & 0x6000000000000000))
				return 1;
			if (pos->castle & 0x8 && to_square == c8 &&
					!(all & 0xE00000000000000) && !(pstate->attacked & 0xC00000000000000))
				return 1;
		}
		return 0;
	default:
		break;
	}

	/* A move is a promotion iff it is a pawn on the 7'th (or 2'nd) rank. */
	if ((move_flag(move) == MOVE_PROMOTION) != (piece == PAWN && (from & (us ? RANK_7 : RANK_2))))
		return 0;

	if (piece == PAWN) {
		int pawn_sign = us ? 8 : -8;

		if (from_square + pawn_sign != to_square && from_square + 2 * pawn_sign != to_square &&
				from_square + 1 + pawn_sign != to_square && from_square - 1 + pawn_sign != to_square)
			return 0;

		if ((from_square + pawn_sign == to_square && !pawn_push(from, all, us)) ||
				(from_square + 2 * pawn_sign == to_square && !(from & (us ? RANK_2 : RANK_7) && pawn_double_push(from, all, us))) ||
				(from_square + 1 + pawn_sign == to_square && !pawn_capture_e(from, pos->piece[them][ALL], us) && (to_square != pos->en_passant || move_flag(move) != MOVE_EN_PASSANT)) ||
				(from_square - 1 + pawn_sign == to_square && !pawn_capture_w(from, pos->piece[them][ALL], us) && (to_square != pos->en_passant || move_flag(move) != MOVE_EN_PASSANT)))
			return 0;
	}
	else if (!(attacks(piece, move_from(move), pos->piece[us][ALL], all) & to)) {
		return 0;
	}

	if (piece == KING && pstate->attacked & to)
		return 0;

	if (single(pstate->checkers)) {
		if (piece != KING &&
				!(to & pstate->checkray ||
					(piece == PAWN && pos->en_passant &&
					 to_square == pos->en_passant &&
					 shift(bitboard(pos->en_passant), down) == pstate->checkers)))
			return 0;
	}
	else if (pstate->checkers && piece != KING) {
		return 0;
	}

	return 1;
}

int legal(const struct position *pos, const struct pstate *pstate, const move_t *move) {
	if (!*move)
		return 0;
	const int us = pos->turn;
	const int them = other_color(us);
	const unsigned down = us ? S : N;

	int from_square = move_from(move);
	int to_square = move_to(move);
	uint64_t from = bitboard(from_square);
	uint64_t to = bitboard(to_square);
	int piece = uncolored_piece(pos->mailbox[from_square]);
	uint64_t all;
	int king_square;

	switch (move_flag(move)) {
	case MOVE_EN_PASSANT:
		king_square = ctz(pos->piece[us][KING]);
		all = all_pieces(pos);
		all ^= from | to | shift(to, down);
		return !(attacks(ROOK, king_square, 0, all) & (pos->piece[them][ROOK] | pos->piece[them][QUEEN])) &&
			!(attacks(BISHOP, king_square, 0, all) & (pos->piece[them][BISHOP] | pos->piece[them][QUEEN]));
		break;
	case MOVE_CASTLE:
		return 1;
	default:
		break;
	}

	return piece == KING || !(pstate->pinned & from) || (ray(ctz(pos->piece[us][KING]), from_square) & to);
}

void print_move(const move_t *m) {
	char move_from_str[3];
	char move_to_str[3];
	algebraic(move_from_str, move_from(m));
	algebraic(move_to_str, move_to(m));
	printf("%s%s", move_from_str, move_to_str);
	if (move_flag(m) == 2)
		printf("%c", "nbrq"[move_promote(m)]);
}

/* str needs to be at least 6 bytes. */
char *move_str_algebraic(char *str, const move_t *m) {
	algebraic(str, move_from(m));
	algebraic(str + 2, move_to(m));
	str[4] = str[5] = '\0';
	if (move_flag(m) == 2)
		str[4] = "nbrq"[move_promote(m)];
	return str;
}

/* str needs to be at least 8 bytes. */
/* m can be illegal. */
char *move_str_pgn(char *str, const struct position *pos, const move_t *m) {
	if (!is_legal(pos, m))
		return NULL;
	int f = file_of(move_from(m));
	int r = rank_of(move_from(m));
	int i = 0;

	str[i] = '\0';
	switch (uncolored_piece(pos->mailbox[move_from(m)])) {
	case PAWN:
		break;
	case KNIGHT:
		str[i++] = 'N';
		break;
	case BISHOP:
		str[i++] = 'B';
		break;
	case ROOK:
		str[i++] = 'R';
		break;
	case QUEEN:
		str[i++] = 'Q';
		break;
	/* King. */
	case KING:
		if (f == 4 && file_of(move_to(m)) == 6) {
			sprintf(str, "O-O");
			i = 3;
		}
		else if (f == 4 && file_of(move_to(m)) == 2) {
			sprintf(str, "O-O-O");
			i = 5;
		}
		else {
			str[i++] = 'K';
		}
		break;
	}

	uint64_t attackers = 0;
	if (uncolored_piece(pos->mailbox[move_from(m)]) == PAWN) {
		if (is_capture(pos, m) || move_flag(m) == 1)
			attackers = rank(move_from(m));
		else
			attackers = bitboard(move_from(m));
	}
	else {
		move_t move_list[MOVES_MAX];
		generate_all(pos, move_list);
		for (move_t *ptr = move_list; *ptr; ptr++) {
			if (move_to(ptr) == move_to(m) && pos->mailbox[move_from(ptr)] == pos->mailbox[move_from(m)])
				attackers |= bitboard(move_from(ptr));
		}
	}
	assert(attackers);

	if (popcount(attackers & file(move_from(m))) > 1) {
		if (popcount(attackers & rank(move_from(m))) > 1) {
			str[i++] = "abcdefgh"[f];
			str[i++] = "12345678"[r];
		}
		else {
			str[i++] = "12345678"[r];
		}
	}
	else if (popcount(attackers) > 1) {
		str[i++] = "abcdefgh"[f];
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
	
	struct position pos_t;
	move_t m_t;
	m_t = *m;
	memcpy(&pos_t, pos, sizeof(pos_t));
	do_move(&pos_t, &m_t);
	move_t movelist[MOVES_MAX];
	generate_all(&pos_t, movelist);
	int mate = !movelist[0];
	uint64_t checkers = generate_checkers(&pos_t, pos_t.turn);

	if (mate && checkers)
		str[i++] = '#';
	else if (checkers)
		str[i++] = '+';
	str[i] = '\0';
	return str;
}

/* str can be illegal. */
move_t string_to_move(const struct position *pos, const char *str) {
	if (!str)
		return 0;
	move_t move_list[MOVES_MAX];
	generate_all(pos, move_list);
	char str_t[8];
	for (move_t *move_ptr = move_list; *move_ptr; move_ptr++) {
		move_str_algebraic(str_t, move_ptr);
		if (strcmp(str_t, str) == 0)
			return *move_ptr;
		move_str_pgn(str_t, pos, move_ptr);
		if (strcmp(str_t, str) == 0)
			return *move_ptr;
	}
	return 0;
}

int is_legal(const struct position *pos, const move_t *m) {
	move_t move_list[MOVES_MAX];
	generate_all(pos, move_list);
	for (move_t *move_ptr = move_list; *move_ptr; move_ptr++)
		if ((*m & 0xFFFF) == (*move_ptr & 0xFFFF))
			return 1;
	return 0;
}

void do_null_move(struct position *pos, int en_passant) {
	pos->en_passant = en_passant;
	pos->turn = other_color(pos->turn);
}
