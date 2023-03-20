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

#include "position.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitboard.h"
#include "util.h"
#include "attack_gen.h"
#include "transposition_table.h"
#include "move.h"
#include "move_gen.h"
#include "history.h"
#include "interface.h"

struct position *start = NULL;

void print_position(const struct position *pos, int flip) {
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
	uint64_t checkers = 0;
	int square;

	square = ctz(pos->piece[color][king]);
	checkers |= (shift_color_west(pos->piece[color][king], color) | shift_color_east(pos->piece[color][king], color)) & pos->piece[1 - color][pawn];
	checkers |= rook_attacks(square, 0, pos->piece[white][all] | pos->piece[black][all]) & (pos->piece[1 - color][rook] | pos->piece[1 - color][queen]);
	checkers |= bishop_attacks(square, 0, pos->piece[white][all] | pos->piece[black][all]) & (pos->piece[1 - color][bishop] | pos->piece[1 - color][queen]);
	checkers |= knight_attacks(square, 0) & pos->piece[1 - color][knight];

	return checkers;
}

uint64_t generate_attacked(const struct position *pos, int color) {
	uint64_t attacked = 0;
	uint64_t piece;
	int square;

	square = ctz(pos->piece[1 - color][king]);

	attacked = king_attacks(square, 0);
	attacked |= shift_color_west(pos->piece[1 - color][pawn], 1 - color);
	attacked |= shift_color_east(pos->piece[1 - color][pawn], 1 - color);

	piece = pos->piece[1 - color][knight];
	while (piece) {
		square = ctz(piece);
		attacked |= knight_attacks(square, 0);
		piece = clear_ls1b(piece);
	}

	piece = pos->piece[1 - color][bishop] | pos->piece[1 - color][queen];
	while (piece) {
		square = ctz(piece);
		attacked |= bishop_attacks(square, 0, (pos->piece[white][all] | pos->piece[black][all]) ^ pos->piece[color][king]);
		piece = clear_ls1b(piece);
	}

	piece = pos->piece[1 - color][rook] | pos->piece[1 - color][queen];
	while (piece) {
		square = ctz(piece);
		attacked |= rook_attacks(square, 0, (pos->piece[white][all] | pos->piece[black][all]) ^ pos->piece[color][king]);
		piece = clear_ls1b(piece);
	}

	return attacked;
}

uint64_t generate_pinned(const struct position *pos, int color) {
	uint64_t pinned_all = 0;
	int king_square;
	int square;
	uint64_t rook_pinners;
	uint64_t bishop_pinners;
	uint64_t pinned;

	king_square = ctz(pos->piece[color][king]);
	rook_pinners = rook_attacks(king_square, 0, pos->piece[1 - color][all]) & (pos->piece[1 - color][rook] | pos->piece[1 - color][queen]);
	bishop_pinners = bishop_attacks(king_square, 0, pos->piece[1 - color][all]) & (pos->piece[1 - color][bishop] | pos->piece[1 - color][queen]);

	while (rook_pinners) {
		square = ctz(rook_pinners);

		pinned = between_lookup[square + 64 * king_square] & pos->piece[color][all];
		if (single(pinned)) {
			pinned_all |= pinned;
		}

		rook_pinners = clear_ls1b(rook_pinners);
	}

	while (bishop_pinners) {
		square = ctz(bishop_pinners);
		pinned = between_lookup[square + 64 * king_square] & pos->piece[color][all];

		if (single(pinned)) {
			pinned_all |= pinned;
		}

		bishop_pinners = clear_ls1b(bishop_pinners);
	}

	return pinned_all;
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
		str[0] = "abcdefgh"[square % 8];
		str[1] = "12345678"[square / 8];
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

void setpos(struct position *pos, uint8_t turn, int8_t en_passant, uint8_t castle,
		uint16_t halfmove, uint16_t fullmove, uint8_t *mailbox) {
	int i;

	pos->turn = turn;
	pos->en_passant = en_passant;
	pos->castle = castle;
	pos->halfmove = halfmove;
	pos->fullmove = fullmove;

	memcpy(pos->mailbox, mailbox, 64);
	for (i = all; i <= king; i++) {
		pos->piece[white][i] = 0;
		pos->piece[black][i] = 0;
	}

	for (i = 0; i < 64; i++) {
		if (0 < mailbox[i] && mailbox[i] < 7) {
			pos->piece[white][mailbox[i]] = set_bit(pos->piece[white][mailbox[i]], i);
			pos->piece[white][all] = set_bit(pos->piece[white][all], i);
		}
		else if (6 < mailbox[i]) {
			pos->piece[black][mailbox[i] - 6] = set_bit(pos->piece[black][mailbox[i] - 6], i);
			pos->piece[black][all] = set_bit(pos->piece[black][all], i);
		}
	}

	pos->zobrist_key = 0;
	for (i = 0; i < 64; i++)
		if (pos->mailbox[i])
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[i] - 1, i);
	if (pos->turn)
		pos->zobrist_key ^= zobrist_turn_key();
	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	if (pos->en_passant)
		pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
}

void startpos(struct position *pos) {
	memcpy(pos, start, sizeof(struct position));
}

/* assumes that fen is ok */
void pos_from_fen(struct position *pos, int argc, char **argv) {
	int t = 0;
	size_t i;

	pos->castle = 0;
	for (i = all; i <= king; i++) {
		pos->piece[white][i] = 0;
		pos->piece[black][i] = 0;
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
				pos->piece[white][t] = set_bit(pos->piece[white][t], counter);
				pos->mailbox[counter] = t;
			}
			else {
				pos->piece[black][t - 6] = set_bit(pos->piece[black][t - 6], counter);
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
	for (i = all; i <= king; i++) {
		pos->piece[white][all] |= pos->piece[white][i];
		pos->piece[black][all] |= pos->piece[black][i];
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

	pos->zobrist_key = 0;
	for (i = 0; i < 64; i++)
		if (pos->mailbox[i])
			pos->zobrist_key ^= zobrist_piece_key(pos->mailbox[i] - 1, i);
	if (pos->turn)
		pos->zobrist_key ^= zobrist_turn_key();
	pos->zobrist_key ^= zobrist_castle_key(pos->castle);
	if (pos->en_passant)
		pos->zobrist_key ^= zobrist_en_passant_key(pos->en_passant);
}

int fen_is_ok(int argc, char **argv) {
	struct position *pos = NULL;
	int ret = 1;
	int t = 0;
	size_t i;

	if (argc < 4)
		goto failure;

	if (argc >= 5)
		if (strint(argv[4]) >= 100)
			goto failure;
	if (argc >= 6)
		if (strint(argv[5]) > 6000)
			goto failure;

	int counter = 56;
	int counter_mem = counter + 16;
	int white_king_counter = 0;
	int black_king_counter = 0;
	int current_line = 0;

	int mailbox[64];
	for (i = 0; i < 64; i++)
		mailbox[i] = 0;
	for (i = 0; i < strlen(argv[0]); i++) {
		if (counter < 0)
			goto failure;
		switch (argv[0][i]) {
		case 'K':
			white_king_counter++;
			/* fallthrough */
		case 'k':
			black_king_counter++;
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
				goto failure;
			t = find_char(" PNBRQKpnbrqk", argv[0][i]);
			if (t < 7) {
				mailbox[counter] = t;
			}
			else {
				mailbox[counter] = t;
			}
			counter++;
			current_line++;
			break;
		case '/':
			if (counter % 8 || counter + 8 != counter_mem)
				goto failure;
			counter_mem = counter;
			counter -= 16;
			current_line = 0;
			break;
		default:
			if (find_char("12345678", argv[0][i]) == -1 || current_line >= 8)
				goto failure;
			counter += find_char(" 12345678", argv[0][i]);
			current_line += find_char(" 12345678", argv[0][i]);
		}
	}

	/* black_king != 2 because fallthrough */
	if (white_king_counter != 1 || black_king_counter != 2)
		goto failure;

	if (strlen(argv[1]) != 1 || (argv[1][0] != 'w' && argv[1][0] != 'b'))
		goto failure;

	for (i = 0; i < strlen(argv[2]); i++) {
		switch(argv[2][i]) {
		case 'K':
		case 'Q':
		case 'k':
		case 'q':
			if (i > 3)
				goto failure;
			break;
		case '-':
			if (strlen(argv[2]) != 1)
				goto failure;
			break;
		default:
			goto failure;
		}
	}

	if (find_char(argv[2], 'K') != -1)
		if (mailbox[e1] != white_king || mailbox[h1] != white_rook)
			goto failure;
	if (find_char(argv[2], 'Q') != -1)
		if (mailbox[e1] != white_king || mailbox[a1] != white_rook)
			goto failure;
	if (find_char(argv[2], 'k') != -1)
		if (mailbox[e8] != black_king || mailbox[h8] != black_rook)
			goto failure;
	if (find_char(argv[2], 'q') != -1)
		if (mailbox[e8] != black_king || mailbox[a8] != black_rook)
			goto failure;

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
				goto failure;
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
				goto failure;
			break;
		case '-':
			if (strlen(argv[3]) != 1)
				goto failure;
			break;
		default:
			goto failure;
		}
	}

	t = square(argv[3]);
	if (t != -1) {
		if (t / 8 == 2) {
			if (mailbox[t + 8] != white_pawn ||
				mailbox[t] != empty ||
				mailbox[t - 8] != empty ||
				argv[1][0] != 'b')
				goto failure;
		}
		else if (t / 8 == 5) {
			if (mailbox[t - 8] != black_pawn ||
				mailbox[t] != empty ||
				mailbox[t + 8] != empty ||
				argv[1][0] != 'w') {
				goto failure;
			}
		}
		else {
			goto failure;
		}

	}

	for (i = 0; i < 8; i++)
		if (mailbox[i] % 6 == pawn || mailbox[i + 56] % 6 == pawn)
			goto failure;

	/* now ok to generate preliminary position */
	pos = malloc(sizeof(struct position));
	pos_from_fen(pos, argc, argv);
	swap_turn(pos);
	uint64_t attacked = generate_attacked(pos, pos->turn);
	swap_turn(pos);
	if (pos->turn) {
		if (attacked & pos->piece[black][king])
			goto failure;
	}
	else {
		if (attacked & pos->piece[white][king])
			goto failure;
	}

	/* check full and half move? */
	goto no_failure;
failure:;
	ret = 0;
no_failure:;
	free(pos);
	return ret;
}

void random_pos(struct position *pos, int n) {
	char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
	pos_from_fen(pos, SIZE(fen), fen);
	move m[MOVES_MAX];

	for (int i = 0; i < n; i++) {
		generate_all(pos, m);
		if (!*m)
			return;
		do_move(pos, m + rand_int(move_count(m)));
	}
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

int interactive_setpos(struct position *pos) {
	char *u = " PNBRQKpnbrqk";
	char turn[2];
	char en_passant[3];
	char castling[5];
	char line[BUFSIZ];
	int i, j, k, t, mailbox[64];
	int x, y;
	char *argv[4];
	int argc;
	for (i = 0; i < 64; i++)
		mailbox[i] = 0;
	x = y = 0;
	int quit = 0;
	while (!quit) {
		printf("\033[1;1H\033[2J");
		printf("\n      a   b   c   d   e   f   g   h\n");
		for (i = 0; i < 8; i++) {
			printf("    +---+---+---+---+---+---+---+---+\n  %i |", 8 - i);
			for (j = 0; j < 8; j++) {
				t = 8 * i + j;
				printf(" %c |", u[mailbox[t]]);
			}
			printf(" %i\n", 8 - i);
		}
		printf("    +---+---+---+---+---+---+---+---+\n");
		printf("      a   b   c   d   e   f   g   h\n\n");
		printf("\033[1;1H\033[%iB\033[%iC", 3 + 2 * y, 6 + 4 * x);

		if(!fgets(line, sizeof(line), stdin))
			return DONE;
		switch (line[0]) {
		case 's':
			quit = 1;
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
			t = find_char("abcdefgh", line[0]);
			if (find_char("12345678", line[1]) != -1) {
				x = t;
				y = 7 - find_char("12345678", line[1]);
				break;
			}
			if (line[0] != 'b') {
				break;
			}
			/* fallthrough */
		case 'P':
		case 'N':
		case 'B':
		case 'R':
		case 'Q':
		case 'K':
		case 'p':
		case 'n':
		/* case 'b': */
		case 'r':
		case 'q':
		case 'k':
		case 'x':
			mailbox[x + 8 * y] = find_char("xPNBRQKpnbrqk", line[0]);
			/* fallthrough */
		case '\n':
			x++;
			if (x == 8) {
				y++;
				x = 0;
			}
			if (y == 8)
				y = 0;
		}
	}
	printf("\033[1;1H\033[2J");
	printf("\n      a   b   c   d   e   f   g   h\n");
	for (i = 0; i < 8; i++) {
		printf("    +---+---+---+---+---+---+---+---+\n  %i |", 8 - i);
		for (j = 0; j < 8; j++) {
			t = 8 * i + j;
			printf(" %c |", u[mailbox[t]]);
		}
		printf(" %i\n", 8 - i);
	}
	printf("    +---+---+---+---+---+---+---+---+\n");
	printf("      a   b   c   d   e   f   g   h\n\n");

	turn[0] = turn[1] = '\0';
	while (1) {
		printf("\033[22;1H\033[2Kturn: ");
		if (!fgets(line, sizeof(line), stdin))
			return DONE;
		if (line[0] == 'w' || line[0] == 'b')
			turn[0] = line[0];

		if (line[0] == '\n')
			turn[0] = 'w';
		if (turn[0] != '\0')
			break;
	}

	while (1) {
		printf("\033[23;1H\033[2Kcastling: ");
		if (!fgets(line, sizeof(line), stdin))
			return DONE;

		for (t = 0; t < 4; t++) {
			if (find_char("KQkq", line[t]) != -1) {
				castling[t] = line[t];
			}
			else if ((line[t] == '\n' || line[t] == '-') && t == 0) {
				castling[0] = '-';
				castling[1] = '\0';
				break;
			}
			else {
				castling[t] = '\0';
				break;
			}
		}

		castling[4] = '\0';
		if (castling[0] != '\0')
			break;
	}

	while (1) {
		printf("\033[24;1H\033[2Ken passant: ");
		if (!fgets(line, sizeof(line), stdin))
			return DONE;
		if (find_char("abcdefgh", line[0]) != -1) {
			if (find_char("12345678", line[1]) != -1) {
				break;
			}
		}
		else if (line[0] == '\n') {
			line[0] = '-';
			break;
		}
		else if (line[0] == '-') {
			break;
		}
	}

	en_passant[0] = line[0];
	en_passant[1] = (en_passant[0] == '-') ? '\0' : line[1];
	en_passant[2] = '\0';
	
	j = k = 0;
	for (i = 0; i < 64; i++) {
		if (mailbox[i]) {
			if (j)
				line[k++] = "012345678"[j];
			j = 0;
			line[k++] = " PNBRQKpnbrqk"[mailbox[i]];
		}
		else {
			j++;
		}
		if (i && i % 8 == 7) {
			if (j)
				line[k++] = "012345678"[j];
			j = 0;
			if (i != 63)
				line[k++] = '/';
		}
	}
	line[k] = '\0';

	argc = 4;
	argv[0] = line;
	argv[1] = turn;
	argv[2] = castling;
	argv[3] = en_passant;

	if (!fen_is_ok(argc, argv))
		return ERR_BAD_ARG;
	pos_from_fen(pos, argc, argv);
	return DONE;
}

void fischer_pos(struct position *pos) {
	uint8_t mailbox[64];
	memset(mailbox, 0, 64);
	int i, j;
	for (i = 0; i < 8; i++) {
		mailbox[8 + i] = white_pawn;
		mailbox[48 + i] = black_pawn;
	}

	/* first bishop */
	i = rand_int(4);
	mailbox[2 * i] = white_bishop;
	mailbox[56 + 2 * i] = black_bishop;

	/* second bishop */
	i = rand_int(4);
	mailbox[2 * i + 1] = white_bishop;
	mailbox[56 + 2 * i + 1] = black_bishop;

	/* queen */
	i = rand_int(6);
	j = 0;
	while (1) {
		if (mailbox[j]) {
			j++;
		}
		else if (0 < i) {
			i--;
			j++;
		}
		else {
			break;
		}
	}
	mailbox[j] = white_queen;
	mailbox[56 + j] = black_queen;

	/* first knight */
	i = rand_int(5);
	j = 0;
	while (1) {
		if (mailbox[j]) {
			j++;
		}
		else if (0 < i) {
			i--;
			j++;
		}
		else {
			break;
		}
	}
	mailbox[j] = white_knight;
	mailbox[56 + j] = black_knight;

	/* second knight */
	i = rand_int(4);
	j = 0;
	while (1) {
		if (mailbox[j]) {
			j++;
		}
		else if (0 < i) {
			i--;
			j++;
		}
		else {
			break;
		}
	}
	mailbox[j] = white_knight;
	mailbox[56 + j] = black_knight;

	/* rook, king, rook */
	j = 0;
	for (i = 0; i < 8; i++) {
		if (!mailbox[i]) {
			mailbox[i] = j ? white_king : white_rook;
			mailbox[56 + i] = j ? black_king : black_rook;
			j = 1 - j;
		}
	}

	setpos(pos, 1, 0, 0, 0, 1, mailbox);
}

void copy_position(struct position *dest, const struct position *src) {
	for (int j = 0; j < 2; j++)
		for (int i = 0; i < 7; i++)
			dest->piece[j][i] = src->piece[j][i];
	dest->turn = src->turn;
	dest->en_passant = src->en_passant;
	dest->castle = src->castle;
	dest->halfmove = src->halfmove;
	dest->fullmove = src->fullmove;
	for (int i = 0; i < 64; i++)
		dest->mailbox[i] = src->mailbox[i];
	dest->zobrist_key = src->zobrist_key;
}

int pos_are_equal(const struct position *pos1, const struct position *pos2) {
	for (int j = 0; j < 2; j++)
		for (int i = 0; i < 7; i++)
			if (pos1->piece[j][i] != pos2->piece[j][i])
				return 0;
	if (pos1->turn != pos2->turn)
		return 0;
	if (pos1->en_passant != pos2->en_passant)
		return 0;
	if (pos1->castle != pos2->castle)
		return 0;
	for (int i = 0; i < 64; i++)
		if (pos1->mailbox[i] != pos2->mailbox[i])
			return 0;
	if (pos1->zobrist_key != pos2->zobrist_key)
		return 0;
	return 1;
}

void print_history_pgn(const struct history *history) {
	if (!history)
		return;
	const struct history *t, *t_last = NULL;
	char str[8];
	while (1) {
		for (t = history; t->previous != t_last; t = t->previous);
		t_last = t;
		if (t->pos->turn) {
			printf("%i. ", t->pos->fullmove);
		}
		else if (!t->previous && !t->pos->turn) {
			printf("%i. ... ", t->pos->fullmove);
		}
		printf("%s ", move_str_pgn(str, t->pos, t->move));
		if (t == history) {
			printf("\n");
			break;
		}
		if (!t->pos->turn)
			printf("\n");
	}
}

void print_history_algebraic(const struct history *history, FILE *file) {
	if (!history)
		return;
	const struct history *t, *t_last = NULL;
	char str[8];
	while (1) {
		for (t = history; t->previous != t_last; t = t->previous);
		t_last = t;
		if (t->previous)
			fprintf(file, " ");
		fprintf(file, "%s", move_str_algebraic(str, t->move));
		if (t == history)
			break;
	}
}

int has_big_piece(const struct position *pos) {
	return pos->turn ? pos->piece[white][bishop] || pos->piece[white][rook] || pos->piece[white][queen] :
		pos->piece[black][bishop] || pos->piece[black][rook] || pos->piece[black][queen];
}

int is_threefold(struct position *pos, struct history *history) {
	int count;
	struct history *t;
	for (t = history, count = 0; t; t = t->previous)
		if (pos_are_equal(pos, t->pos))
			count++;
	return count >= 2;
}

void position_init(void) {
	char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
	start = malloc(sizeof(struct position));
	pos_from_fen(start, SIZE(fen), fen);
}

void position_term(void) {
	free(start);
}
