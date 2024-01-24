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

#include "position.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bitboard.h"
#include "util.h"
#include "attackgen.h"
#include "movegen.h"
#include "history.h"
#include "interface.h"

static struct position start;

void print_position(const struct position *pos) {
	const int flip = 0;
	int i, j, t;
	char pieces[] = " PNBRQKpnbrqk";
	char letters[] = "abcdefgh";

	printf("\n   ");
	for (i = 0; i < 8; i++)
		printf("   %c", letters[flip ? 7 - i : i]);
	printf("\n");
	for (i = 0; i < 8; i++) {
		printf("    +---+---+---+---+---+---+---+---+\n  %i |", flip ? 1 + i : 8 - i);
		for (j = 0; j < 8; j++) {
			t = flip ? 8 * i + (7 - j) : 8 * (7 - i) + j;
			printf(" %c |", pieces[pos->mailbox[t]]);
		}
		printf(" %i\n", flip ? 1 + i : 8 - i);
	}
	printf("    +---+---+---+---+---+---+---+---+\n   ");
	for (i = 0; i < 8; i++)
		printf("   %c", letters[flip ? 7 - i : i]);
	printf("\n\n");
}

uint64_t generate_checkers(const struct position *pos, int color) {
	return generate_attackers(pos, ctz(pos->piece[color][KING]), color);
}

uint64_t generate_attackers(const struct position *pos, int square, int color) {
	uint64_t attackers = 0;
	const unsigned up = color ? N : S;

	attackers |= (shift(pos->piece[color][KING], up | E) | shift(pos->piece[color][KING], up | W)) & pos->piece[other_color(color)][PAWN];
	attackers |= rook_attacks(square, 0, pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL]) & (pos->piece[other_color(color)][ROOK] | pos->piece[other_color(color)][QUEEN]);
	attackers |= bishop_attacks(square, 0, pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL]) & (pos->piece[other_color(color)][BISHOP] | pos->piece[other_color(color)][QUEEN]);
	attackers |= knight_attacks(square, 0) & pos->piece[other_color(color)][KNIGHT];

	return attackers;
}

uint64_t generate_attacked(const struct position *pos, int color) {
	uint64_t attacked = 0;
	const unsigned up = color ? N : S;
	uint64_t piece;
	int square;

	square = ctz(pos->piece[color][KING]);

	attacked = king_attacks(square, 0);
	attacked |= shift(pos->piece[color][PAWN], up | E);
	attacked |= shift(pos->piece[color][PAWN], up | W);

	piece = pos->piece[color][KNIGHT];
	while (piece) {
		square = ctz(piece);
		attacked |= knight_attacks(square, 0);
		piece = clear_ls1b(piece);
	}

	piece = pos->piece[color][BISHOP] | pos->piece[color][QUEEN];
	while (piece) {
		square = ctz(piece);
		attacked |= bishop_attacks(square, 0, (pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL]) ^ pos->piece[other_color(color)][KING]);
		piece = clear_ls1b(piece);
	}

	piece = pos->piece[color][ROOK] | pos->piece[color][QUEEN];
	while (piece) {
		square = ctz(piece);
		attacked |= rook_attacks(square, 0, (pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL]) ^ pos->piece[other_color(color)][KING]);
		piece = clear_ls1b(piece);
	}

	return attacked;
}

uint64_t generate_pinned(const struct position *pos, int color) {
	uint64_t pinners = pos->piece[other_color(color)][ALL];
	return generate_blockers(pos, pinners, ctz(pos->piece[color][KING]));
}

uint64_t generate_blockers(const struct position *pos, uint64_t pinners, int king_square) {
	uint64_t pinned = 0;

	pinners = ((rook_attacks(king_square, 0, 0) & (pos->piece[WHITE][ROOK] | pos->piece[BLACK][ROOK] | pos->piece[WHITE][QUEEN] | pos->piece[BLACK][QUEEN]))
		| (bishop_attacks(king_square, 0, 0) & (pos->piece[WHITE][BISHOP] | pos->piece[BLACK][BISHOP] | pos->piece[WHITE][QUEEN] | pos->piece[BLACK][QUEEN])))
		& pinners;

	while (pinners) {
		int square = ctz(pinners);

		uint64_t b = between(square, king_square) & all_pieces(pos);
		if (single(b))
			pinned |= b;

		pinners = clear_ls1b(pinners);
	}
	return pinned;
}

uint64_t generate_pinners(const struct position *pos, uint64_t pinned, int color) {
	int king_square = ctz(pos->piece[color][KING]);
	uint64_t ret = 0;
	uint64_t pinners = (rook_attacks(king_square, 0, pos->piece[other_color(color)][ALL]) & (pos->piece[other_color(color)][ROOK] | pos->piece[other_color(color)][QUEEN]))
			 | (bishop_attacks(king_square, 0, pos->piece[other_color(color)][ALL]) & (pos->piece[other_color(color)][BISHOP] | pos->piece[other_color(color)][QUEEN]));

	while (pinned) {
		int square = ctz(pinned);
		if (ray(king_square, square) & pinners)
			ret |= ray(king_square, square) & pinners;
		pinned = clear_ls1b(pinned);
	}

	return ret;
}

int square(const char *algebraic) {
	if (strlen(algebraic) != 2) {
		return -1;
	}
	if (find_char("abcdefgh", algebraic[0]) == -1 || find_char("12345678", algebraic[1]) == -1)
		return -1;
	return find_char("abcdefgh", algebraic[0]) + 8 * find_char("12345678", algebraic[1]);
}

char *algebraic(char *str, int square) {
	if (square < 0 || 63 < square) {
		str[0] = '-';
		str[1] = '\0';
	}
	else {
		str[0] = "abcdefgh"[file_of(square)];
		str[1] = "12345678"[rank_of(square)];
	}
	str[2] = '\0';
	return str;
}

char *castle_string(char *str, int castle) {
	int counter = 0;
	for (int i = 0; i < 4; i++)
		if (get_bit(castle, i))
			str[counter++] = "KQkq"[i];
	if (!counter)
		str[counter++] = '-';
	str[counter] = '\0';
	return str;
}

void startpos(struct position *pos) {
	memcpy(pos, &start, sizeof(*pos));
}

/* Assumes that fen is ok. */
void pos_from_fen(struct position *pos, int argc, char **argv) {
	int t = 0;
	size_t i;

	pos->castle = 0;
	for (i = ALL; i <= KING; i++) {
		pos->piece[WHITE][i] = 0;
		pos->piece[BLACK][i] = 0;
	}
	for (i = 0; i < 64; i++)
		pos->mailbox[i] = 0;

	int counter = 56;
	for (i = 0; i < strlen(argv[0]); i++) {
		switch (argv[0][i]) {
		case 'P':
		case 'N':
		case 'B':
		case 'R':
		case 'Q':
		case 'K':
		case 'p':
		case 'n':
		case 'b':
		case 'r':
		case 'q':
		case 'k':
			t = find_char(" PNBRQKpnbrqk", argv[0][i]);
			if (t < 7) {
				pos->piece[WHITE][t] = set_bit(pos->piece[WHITE][t], counter);
				pos->mailbox[counter] = t;
			}
			else {
				pos->piece[BLACK][t - 6] = set_bit(pos->piece[BLACK][t - 6], counter);
				pos->mailbox[counter] = t;
			}
			counter++;
			break;
		case '/':
			counter -= 16;
			break;
		default:
			counter += find_char(" 12345678", argv[0][i]);
		}
	}
	for (i = PAWN; i <= KING; i++) {
		pos->piece[WHITE][ALL] |= pos->piece[WHITE][i];
		pos->piece[BLACK][ALL] |= pos->piece[BLACK][i];
	}

	pos->turn = (argv[1][0] == 'w');

	if (-1 < find_char(argv[2], 'K') && find_char(argv[2], 'K') < 4)
		pos->castle = set_bit(pos->castle, 0);
	if (-1 < find_char(argv[2], 'Q') && find_char(argv[2], 'Q') < 4)
		pos->castle = set_bit(pos->castle, 1);
	if (-1 < find_char(argv[2], 'k') && find_char(argv[2], 'k') < 4)
		pos->castle = set_bit(pos->castle, 2);
	if (-1 < find_char(argv[2], 'q') && find_char(argv[2], 'q') < 4)
		pos->castle = set_bit(pos->castle, 3);

	pos->en_passant = square(argv[3]) == -1 ? 0 : square(argv[3]);

	pos->halfmove = 0;
	pos->fullmove = 1;
	if (argc >= 5)
		pos->halfmove = strint(argv[4]);
	if (argc >= 6)
		pos->fullmove = strint(argv[5]);
}

void mirror_position(struct position *pos) {
	if (pos->en_passant)
		pos->en_passant = orient_horizontal(BLACK, pos->en_passant);
	pos->turn = other_color(pos->turn);

	for (int wsq = a1; wsq <= h4; wsq++) {
		int bsq = orient_horizontal(BLACK, wsq);
		int wp = pos->mailbox[wsq];
		int bp = pos->mailbox[bsq];
		pos->mailbox[wsq] = bp ? colored_piece(uncolored_piece(bp), WHITE) : EMPTY;
		pos->mailbox[bsq] = wp ? colored_piece(uncolored_piece(wp), BLACK) : EMPTY;
	}

	for (int color = 0; color < 2; color++) {
		for (int piece = ALL; piece <= KING; piece++) {
			uint64_t b = pos->piece[color][piece];
			pos->piece[color][piece] = 0;
			while (b) {
				int sq = ctz(b);
				pos->piece[color][piece] |= bitboard(orient_horizontal(BLACK, sq));
				b = clear_ls1b(b);
			}
		}
	}

	for (int piece = ALL; piece <= KING; piece++) {
		uint64_t b = pos->piece[WHITE][piece];
		pos->piece[WHITE][piece] = pos->piece[BLACK][piece];
		pos->piece[BLACK][piece] = b;
	}

	pos->castle = ((pos->castle & 0x3) << 2) | ((pos->castle & 0xC) >> 2);
}

int fen_is_ok(int argc, char **argv) {
	int t = 0;
	size_t i;

	if (argc < 4)
		return 0;

	if (argc >= 5 && strint(argv[4]) > 100)
		return 0;
	if (argc >= 6 && strint(argv[5]) > 6000)
		return 0;

	int counter = 56;
	int counter_mem = counter + 16;
	int current_line = 0;

	int mailbox[64];
	for (i = 0; i < 64; i++)
		mailbox[i] = 0;
	for (i = 0; i < strlen(argv[0]); i++) {
		if (counter < 0)
			return 0;
		switch (argv[0][i]) {
		case 'K':
		case 'k':
		case 'P':
		case 'N':
		case 'B':
		case 'R':
		case 'Q':
		case 'p':
		case 'n':
		case 'b':
		case 'r':
		case 'q':
			if (counter > 63 || current_line >= 8)
				return 0;
			t = find_char(" PNBRQKpnbrqk", argv[0][i]);
			mailbox[counter] = t;
			counter++;
			current_line++;
			break;
		case '/':
			if (counter % 8 || counter + 8 != counter_mem)
				return 0;
			counter_mem = counter;
			counter -= 16;
			current_line = 0;
			break;
		default:
			if (find_char("12345678", argv[0][i]) == -1 || current_line >= 8)
				return 0;
			counter += find_char(" 12345678", argv[0][i]);
			current_line += find_char(" 12345678", argv[0][i]);
		}
	}

	if (strlen(argv[1]) != 1 || (argv[1][0] != 'w' && argv[1][0] != 'b'))
		return 0;

	for (i = 0; i < strlen(argv[2]); i++) {
		switch(argv[2][i]) {
		case 'K':
		case 'Q':
		case 'k':
		case 'q':
			if (i > 3)
				return 0;
			break;
		case '-':
			if (strlen(argv[2]) != 1)
				return 0;
			break;
		default:
			return 0;
		}
	}

	if (find_char(argv[2], 'K') != -1)
		if (mailbox[e1] != WHITE_KING || mailbox[h1] != WHITE_ROOK)
			return 0;
	if (find_char(argv[2], 'Q') != -1)
		if (mailbox[e1] != WHITE_KING || mailbox[a1] != WHITE_ROOK)
			return 0;
	if (find_char(argv[2], 'k') != -1)
		if (mailbox[e8] != BLACK_KING || mailbox[h8] != BLACK_ROOK)
			return 0;
	if (find_char(argv[2], 'q') != -1)
		if (mailbox[e8] != BLACK_KING || mailbox[a8] != BLACK_ROOK)
			return 0;

	for (i = 0; i < strlen(argv[3]); i++) {
		switch(argv[3][i]) {
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
			if (i != 0 || strlen(argv[3]) != 2)
				return 0;
			break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			if (i != 1 || strlen(argv[3]) != 2)
				return 0;
			break;
		case '-':
			if (strlen(argv[3]) != 1)
				return 0;
			break;
		default:
			return 0;
		}
	}

	t = square(argv[3]);
	if (t != -1) {
		if (t / 8 == 2) {
			if (mailbox[t + 8] != WHITE_PAWN ||
					mailbox[t] != EMPTY ||
					mailbox[t - 8] != EMPTY ||
					argv[1][0] != 'b')
				return 0;
		}
		else if (t / 8 == 5) {
			if (mailbox[t - 8] != BLACK_PAWN ||
					mailbox[t] != EMPTY ||
					mailbox[t + 8] != EMPTY ||
					argv[1][0] != 'w')
				return 0;
		}
		else {
			return 0;
		}

	}

	for (i = 0; i < 8; i++)
		if (uncolored_piece(mailbox[i]) == PAWN || uncolored_piece(mailbox[i + 56]) == PAWN)
			return 0;

	/* Now ok to generate preliminary position. */
	struct position pos;
	pos_from_fen(&pos, argc, argv);
	uint64_t checkers = generate_checkers(&pos, other_color(pos.turn));
	if (checkers)
		return 0;
	if (popcount(pos.piece[WHITE][ALL]) > 16 || popcount(pos.piece[BLACK][ALL]) > 16)
		return 0;
	if (popcount(pos.piece[WHITE][PAWN]) > 8 || popcount(pos.piece[BLACK][PAWN]) > 8)
		return 0;
	for (int piece = KNIGHT; piece <= ROOK; piece++)
		if (popcount(pos.piece[WHITE][piece]) > 10 || popcount(pos.piece[BLACK][piece]) > 10)
			return 0;
	if (popcount(pos.piece[WHITE][QUEEN]) > 9 || popcount(pos.piece[BLACK][QUEEN]) > 9)
		return 0;
	if (popcount(pos.piece[WHITE][KING]) > 1 || popcount(pos.piece[BLACK][KING]) > 1)
		return 0;
	return 1;
}

char *pos_to_fen(char *fen, const struct position *pos) {
	int k = 0;
	char tmp[128];

	size_t i = 56, j = 0;
	while (1) {
		if (pos->mailbox[i]) {
			if (j)
				fen[k++] = " 12345678"[j];
			j = 0;
			fen[k++] = " PNBRQKpnbrqk"[pos->mailbox[i]];
		}
		else {
			j++;
		}
		i++;
		if (i == 8) {
			if (j)
				fen[k++] = " 12345678"[j];
			break;
		}
		if (i % 8 == 0)  {
			if (j)
				fen[k++] = " 12345678"[j];
			j = 0;
			fen[k++] = '/';
			i -= 16;
		}
	}
	fen[k++] = ' ';
	fen[k++] = pos->turn ? 'w' : 'b';
	fen[k++] = ' ';
	castle_string(tmp, pos->castle);
	for (i = 0; i < strlen(tmp); i++)
		fen[k++] = tmp[i];
	fen[k++] = ' ';
	algebraic(tmp, pos->en_passant ? pos->en_passant : -1);
	for (i = 0; i < strlen(tmp); i++)
		fen[k++] = tmp[i];
	fen[k++] = ' ';
	sprintf(tmp, "%i", pos->halfmove);
	for (i = 0; i < strlen(tmp); i++)
		fen[k++] = tmp[i];
	fen[k++] = ' ';
	sprintf(tmp, "%i", pos->fullmove);
	for (i = 0; i < strlen(tmp); i++)
		fen[k++] = tmp[i];
	fen[k++] = '\0';
	return fen;
}

int pos_are_equal(const struct position *pos1, const struct position *pos2) {
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 7; i++) {
			if (pos1->piece[j][i] != pos2->piece[j][i]) {
				printf("BITBOARD ERROR: %i, %i\n", j, i);
				return 0;
			}
		}
	}
	if (pos1->turn != pos2->turn) {
		printf("TURN ERROR\n");
		return 0;
	}
	if (pos1->en_passant != pos2->en_passant) {
		printf("EN PASSANT ERROR\n");
		return 0;
	}
	if (pos1->castle != pos2->castle) {
		printf("CASTLE ERROR\n");
		return 0;
	}
	for (int i = 0; i < 64; i++) {
		if (pos1->mailbox[i] != pos2->mailbox[i]) {
			printf("MAILBOX ERROR: %i\n", i);
			return 0;
		}
	}
	if (pos1->zobrist_key != pos2->zobrist_key) {
		printf("ZOBRIST KEY ERROR\n");
		return 0;
	}
	return 1;
}

void print_history_pgn(const struct history *h) {
	if (!h)
		return;

	struct position pos;
	memcpy(&pos, &h->start, sizeof(pos));
	move_t m[POSITIONS_MAX];
	memcpy(m, h->move, sizeof(m));

	char str[8];
	for (int i = 0; i < h->ply; i++) {
		if (pos.turn) {
			printf("%i. ", pos.fullmove);
		}
		else if (!i && !pos.turn) {
			printf("%i. ... ", pos.fullmove);
		}
		printf("%s ", move_str_pgn(str, &pos, m + i));
		if (!pos.turn)
			printf("\n");
		do_move(&pos, m + i);
	}
	if (h->ply)
		printf("\n");
}

int has_sliding_piece(const struct position *pos) {
	return pos->piece[pos->turn][QUEEN] || pos->piece[pos->turn][ROOK] || pos->piece[pos->turn][QUEEN];
}

/* Check for irreversible moves. */
int is_repetition(const struct position *pos, const struct history *h, int ply, int count) {
	for (int i = ply + h->ply - 2, c = 0; i >= 0; i -= 2) {
		if (pos->zobrist_key == h->zobrist_key[i])
			c++;
		if (c == count)
			return 1;
	}
	return 0;
}

void position_init(void) {
	char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
	pos_from_fen(&start, SIZE(fen), fen);
}
