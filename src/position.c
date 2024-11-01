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

void print_fen(const struct position *pos) {
	char fen[128];
	printf("%s\n", pos_to_fen(fen, pos));
}

void pstate_init(const struct position *pos, struct pstate *pstate) {
	const int us = pos->turn;
	const int them = other_color(us);
	pstate->checkers = generate_checkers(pos, us);
	generate_attacked(pos, them, pstate->attacked);
	pstate->checkray = single(pstate->checkers) ? between(ctz(pos->piece[us][KING]), ctz(pstate->checkers)) | pstate->checkers : 0;
	pstate->pinned = generate_pinned(pos, us);
	generate_check_threats(pos, us, pstate->check_threats);
}

uint64_t generate_checkers(const struct position *pos, int color) {
	return generate_attackers(pos, ctz(pos->piece[color][KING]), other_color(color));
}

uint64_t generate_attackers(const struct position *pos, int square, int color) {
	uint64_t attackers = 0;
	const unsigned up = color ? S : N;

	attackers |= (shift(bitboard(square), up | E) | shift(bitboard(square), up | W)) & pos->piece[color][PAWN];
	attackers |= rook_attacks(square, 0, pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL]) & (pos->piece[color][ROOK] | pos->piece[color][QUEEN]);
	attackers |= bishop_attacks(square, 0, pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL]) & (pos->piece[color][BISHOP] | pos->piece[color][QUEEN]);
	attackers |= knight_attacks(square, 0) & pos->piece[color][KNIGHT];

	return attackers;
}

void generate_attacked(const struct position *pos, int color, uint64_t attacked[7]) {
	const unsigned up = color ? N : S;
	uint64_t piece;
	int square;

	square = ctz(pos->piece[color][KING]);

	attacked[KING] = king_attacks(square, 0);
	attacked[PAWN] = 0;
	attacked[PAWN] |= shift(pos->piece[color][PAWN], up | E);
	attacked[PAWN] |= shift(pos->piece[color][PAWN], up | W);

	attacked[KNIGHT] = 0;
	piece = pos->piece[color][KNIGHT];
	while (piece) {
		square = ctz(piece);
		attacked[KNIGHT] |= knight_attacks(square, 0);
		piece = clear_ls1b(piece);
	}

	attacked[BISHOP] = 0;
	piece = pos->piece[color][BISHOP];
	while (piece) {
		square = ctz(piece);
		attacked[BISHOP] |= bishop_attacks(square, 0, all_pieces(pos) ^ pos->piece[other_color(color)][KING]);
		piece = clear_ls1b(piece);
	}

	attacked[ROOK] = 0;
	piece = pos->piece[color][ROOK];
	while (piece) {
		square = ctz(piece);
		attacked[ROOK] |= rook_attacks(square, 0, all_pieces(pos) ^ pos->piece[other_color(color)][KING]);
		piece = clear_ls1b(piece);
	}

	attacked[QUEEN] = 0;
	piece = pos->piece[color][QUEEN];
	while (piece) {
		square = ctz(piece);
		attacked[ROOK] |= queen_attacks(square, 0, all_pieces(pos) ^ pos->piece[other_color(color)][KING]);
		piece = clear_ls1b(piece);
	}

	attacked[ALL] = attacked[PAWN] | attacked[KNIGHT] | attacked[BISHOP] | attacked[ROOK] | attacked[QUEEN] | attacked[KING];
}

uint64_t generate_attacked_all(const struct position *pos, int color) {
	uint64_t attacked[7];
	generate_attacked(pos, color, attacked);
	return attacked[ALL];
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

void generate_check_threats(const struct position *pos, int color, uint64_t check_threats[7]) {
	const int them = other_color(color);
	const unsigned down = color ? S : N;
	int square = ctz(pos->piece[them][KING]);

	check_threats[ALL] = 0;
	check_threats[PAWN] = shift(pos->piece[them][KING], down | W) | shift(pos->piece[them][KING], down | E);
	for (int piece = KNIGHT; piece <= QUEEN; piece++)
		check_threats[piece] = attacks(piece, square, 0, all_pieces(pos));
	check_threats[KING] = 0;
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
	*pos = start;
}

void pos_from_fen2(struct position *pos, const char *str) {
	char fen[128] = { 0 };
	snprintf(fen, 127, "%s", str);
	
	int argc;
	char *argv[6] = { fen };

	for (argc = 1; argc < 6; argc++) {
		char *c = strchr(argv[argc - 1], ' ');
		if (!c)
			break;
		*c = '\0';
		argv[argc] = &c[1];
	}
	pos_from_fen(pos, argc, argv);
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

int fen_is_ok2(const char *str) {
	char fen[128] = { 0 };
	snprintf(fen, 127, "%s", str);
	
	int argc;
	char *argv[6] = { fen };

	for (argc = 1; argc < 6; argc++) {
		char *c = strchr(argv[argc - 1], ' ');
		if (!c)
			break;
		*c = '\0';
		argv[argc] = &c[1];
	}
	return fen_is_ok(argc, argv);
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

	int king1 = 0, king2 = 0;

	int mailbox[64];
	for (i = 0; i < 64; i++)
		mailbox[i] = 0;
	for (i = 0; i < strlen(argv[0]); i++) {
		if (counter < 0)
			return 0;
		switch (argv[0][i]) {
		case 'K':
			king1++;
			/* fallthrough */
		case 'k':
			king2++;
			/* fallthrough */
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

	if (strlen(argv[1]) != 1 || (argv[1][0] != 'w' && argv[1][0] != 'b') || king1 != 1 || king2 != 2)
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
	return pos_is_ok(&pos);
}

int pos_is_ok(const struct position *pos) {
	if (pos->halfmove > 100 || pos->fullmove > 6000)
		return 0;

	if (pos->turn != WHITE && pos->turn != BLACK)
		return 0;

	if (popcount(pos->piece[WHITE][ALL]) > 16 || popcount(pos->piece[BLACK][ALL]) > 16)
		return 0;
	if (popcount(pos->piece[WHITE][PAWN]) > 8 || popcount(pos->piece[BLACK][PAWN]) > 8)
		return 0;
	for (int piece = KNIGHT; piece <= ROOK; piece++)
		if (popcount(pos->piece[WHITE][piece]) > 10 || popcount(pos->piece[BLACK][piece]) > 10)
			return 0;
	if (popcount(pos->piece[WHITE][QUEEN]) > 9 || popcount(pos->piece[BLACK][QUEEN]) > 9)
		return 0;
	if (popcount(pos->piece[WHITE][KING]) > 1 || popcount(pos->piece[BLACK][KING]) > 1)
		return 0;

	for (int color = BLACK; color <= WHITE; color++) {
		uint64_t all = 0;
		for (int piece = WHITE_PAWN; piece <= WHITE_KING; piece++) {
			if (all & pos->piece[color][piece])
				return 0;
			all |= pos->piece[color][piece];
		}
		if (pos->piece[color][ALL] != all)
			return 0;
	}

	if (pos->piece[WHITE][ALL] & pos->piece[BLACK][ALL])
		return 0;

	for (int sq = 0; sq < 63; sq++) {
		int cpiece = pos->mailbox[sq];
		if (cpiece < EMPTY || cpiece > BLACK_KING)
			return 0;
		if (cpiece == EMPTY) {
			if ((pos->piece[WHITE][ALL] | pos->piece[BLACK][ALL]) & bitboard(sq))
				return 0;
		}
		else {
			int color = color_of_piece(cpiece);
			int piece = uncolored_piece(cpiece);

			if (!(pos->piece[color][piece] & bitboard(sq)))
				return 0;
		}
	}

	if (pos->en_passant) {
		if (pos->turn && (pos->en_passant / 8 != 5 || pos->mailbox[pos->en_passant - 8] != BLACK_PAWN ||
				pos->mailbox[pos->en_passant + 8] != EMPTY || pos->mailbox[pos->en_passant] != EMPTY))
			return 0;
		if (!pos->turn && (pos->en_passant / 8 != 2 || pos->mailbox[pos->en_passant + 8] != WHITE_PAWN ||
				pos->mailbox[pos->en_passant - 8] != EMPTY || pos->mailbox[pos->en_passant] != EMPTY))
			return 0;
	}

	if (pos->castle & 0x1 && (pos->mailbox[e1] != WHITE_KING || pos->mailbox[h1] != WHITE_ROOK))
		return 0;
	if (pos->castle & 0x2 && (pos->mailbox[e1] != WHITE_KING || pos->mailbox[a1] != WHITE_ROOK))
		return 0;
	if (pos->castle & 0x4 && (pos->mailbox[e8] != BLACK_KING || pos->mailbox[h8] != BLACK_ROOK))
		return 0;
	if (pos->castle & 0x8 && (pos->mailbox[e8] != BLACK_KING || pos->mailbox[a8] != BLACK_ROOK))
		return 0;

	uint64_t checkers = generate_checkers(pos, other_color(pos->turn));
	if (checkers)
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

char *poscmp(const struct position *pos1, const struct position *pos2, int check_zobrist) {
	static char error[128];
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 7; i++) {
			if (pos1->piece[j][i] != pos2->piece[j][i]) {
				sprintf(error, "bitboard[%i][%i]", j, i);
				return error;
			}
		}
	}
	if (pos1->turn != pos2->turn) {
		sprintf(error, "turn");
		return error;
	}

	if (pos1->castle != pos2->castle) {
		sprintf(error, "castle");
		return error;
	}
	for (int i = 0; i < 64; i++) {
		if (pos1->mailbox[i] != pos2->mailbox[i]) {
			sprintf(error, "mailbox[%i]", i);
			return error;
		}
	}
	if (check_zobrist && pos1->zobrist_key != pos2->zobrist_key) {
		sprintf(error, "zobrist key");
		return error;
	}
	if (pos1->halfmove != pos2->halfmove) {
		sprintf(error, "halfmove");
		return error;
	}

	/* It could be that the en passant is different for two position, but
	 * they are still equal. This happens if the en passant square is set,
	 * but is not actually attacked by any enemy pawns.
	 */
	int en_passant_error = 0;
	int en_passant1 = pos1->en_passant;
	int en_passant2 = pos2->en_passant;
	if (en_passant1 < 8 || en_passant1 >= 56)
		en_passant1 = 0;
	if (en_passant2 < 8 || en_passant2 >= 56)
		en_passant2 = 0;

	if ((en_passant1 && !en_passant2) || (!en_passant1 && en_passant2)) {
		const struct position *pos = en_passant1 ? pos1 : pos2;

		int en_passant = max(en_passant1, en_passant2);
		en_passant1 = en_passant2 = 0;
		struct pstate ps;
		pstate_init(pos, &ps);

		
		int from[2];
		if (pos->turn) {
			from[0] = en_passant + 9;
			from[1] = en_passant + 7;
		}
		else {
			from[0] = en_passant - 9;
			from[1] = en_passant - 7;
		}

		for (int i = 0; i < 2; i++) {
			move_t move = M(from[i], en_passant, 0, 0);
			if (uncolored_piece(pos->mailbox[from[i]]) == PAWN && pseudo_legal(pos, &ps, &move) && legal(pos, &ps, &move)) {
				en_passant_error = 1;
				break;
			}
		}
	}
	if (en_passant_error || en_passant1 != en_passant2) {
		sprintf(error, "en passant");
		return error;
	}

	return NULL;
}

void print_history_pgn(const struct history *h) {
	if (!h || !h->ply)
		return;

	struct position pos = h->start;
	move_t move[SIZE(h->move)];
	memcpy(move, h->move, sizeof(move));

	char str[8];
	for (int i = 0; i < h->ply; i++) {
		if (pos.turn) {
			printf("%i. ", pos.fullmove);
		}
		else if (!i && !pos.turn) {
			printf("%i. ... ", pos.fullmove);
		}
		printf("%s ", move_str_pgn(str, &pos, &move[i]));
		if (!pos.turn)
			printf("\n");
		do_move(&pos, &move[i]);
	}
	if (!pos.turn)
		printf("\n");
}

int has_sliding_piece(const struct position *pos) {
	return pos->piece[pos->turn][QUEEN] || pos->piece[pos->turn][ROOK] || pos->piece[pos->turn][BISHOP];
}

void position_init(void) {
	pos_from_fen2(&start, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}
