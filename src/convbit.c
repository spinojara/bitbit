/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022-2025 Isak Ellmer
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "io.h"
#include "move.h"
#include "movegen.h"
#include "util.h"
#include "evaluate.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "bitboard.h"

long count1, count2;

move_t difference_brute(struct position *before, const struct position *after) {
	move_t moves[MOVES_MAX];

	movegen_legal(before, moves, MOVETYPE_ALL);

	for (move_t *move = moves; *move; move++) {
		if (after->mailbox[move_from(move)] || color_of_piece(after->mailbox[move_to(move)]) != before->turn)
			continue;
		do_move(before, move);
		char *c = poscmp(before, after, 0);
		undo_move(before, move);
		if (!c)
			return *move;
	}

	return 0;
}

move_t difference(struct position *before, const struct position *after) {
	if (before->turn == after->turn || before->halfmove + 1 < after->halfmove)
		return 0;

	int from = -1;
	int to = -1;
	uint64_t diff = before->piece[before->turn][ALL] ^ after->piece[before->turn][ALL];
	switch (popcount(diff & before->piece[before->turn][ALL])) {
	case 1:
		from = ctz(diff & before->piece[before->turn][ALL]);
		break;
	case 2:
		from = ctz(before->piece[before->turn][KING]);
		break;
	default:
		return 0;
	}
	switch (popcount(diff & after->piece[before->turn][ALL])) {
	case 1:
		to = ctz(diff & after->piece[before->turn][ALL]);
		break;
	case 2:
		to = ctz(after->piece[before->turn][KING]);
		break;
	default:
		return 0;
	}

	if (to != -1 && from != -1) {
		int flag = 0;
		int piece = uncolored_piece(before->mailbox[from]);

		if (piece == PAWN) {
			switch (rank_of(from)) {
			case 1:
				if (!before->turn)
					flag = MOVE_PROMOTION;
				break;
			case 6:
				if (before->turn)
					flag = MOVE_PROMOTION;
				break;
			default:
				break;
			}
			if (before->en_passant && to == before->en_passant)
				flag = MOVE_EN_PASSANT;
		}

		if (piece == KING && distance(from, to) == 2)
			flag = MOVE_CASTLE;

		int promotion = flag == MOVE_PROMOTION ? uncolored_piece(after->mailbox[to]) - KNIGHT : 0;
		move_t move = M(from, to, flag, promotion);

		struct pstate ps;
		pstate_init(before, &ps);
		if (pseudo_legal(before, &ps, &move) && legal(before, &ps, &move)) {
			count2++;
			do_move(before, &move);
			char *c = poscmp(before, after, 0);
			undo_move(before, &move);
			if (!c) {
				count1++;
				return move;
			}
#if 0
			else
				printf("error: %s\n", c);
#endif
		}
#if 0
		printf("before: ");
		print_fen(before);
		printf("after:  ");
		print_fen(after);
		printf("legal: %d %d\n", pseudo_legal(before, &ps, &move), legal(before, &ps, &move));
		print_move(&move);
		printf("\n\n");
		exit(1);
#endif
	}

	/* Backup if there is something we missed. */
	move_t moves[MOVES_MAX];

	movegen_legal(before, moves, MOVETYPE_ALL);

	for (move_t *move = moves; *move; move++) {
		if (after->mailbox[move_from(move)] || color_of_piece(after->mailbox[move_to(move)]) != before->turn)
			continue;
		do_move(before, move);
		char *c = poscmp(before, after, 0);
		undo_move(before, move);
		if (!c)
			return *move;
	}

	return 0;
}

int main(int argc, char **argv) {
	double scale_eval = 1.0;
	char *inpath, *outpath;
	static struct option opts[] = {
		{ "scale-eval", required_argument, NULL, 's' },
		{ NULL,         0,                 NULL,  0  },
	};
	char *endptr;
	int c, option_index = 0;
	int error = 0;
	while ((c = getopt_long(argc, argv, "s:", opts, &option_index)) != -1) {
		switch (c) {
		case 's':
			errno = 0;
			scale_eval = strtod(optarg, &endptr);
			if (errno || *endptr || scale_eval <= 0.0)
				error = 1;
			break;
		default:
			error = 1;
			break;
		}
	}

	if (error || optind + 1 >= argc) {
		fprintf(stderr, "usage: %s [--scale-eval] infile outfile\n", argv[0]);
		return 1;
	}
	inpath = argv[optind];
	outpath = argv[optind + 1];

	FILE *in = fopen(inpath, "r");
	if (!in) {
		fprintf(stderr, "error: failed to open file '%s'\n", inpath);
		return 2;
	}

	FILE *out = fopen(outpath, "wb");
	if (!out) {
		fprintf(stderr, "error: failed to open file '%s'\n", outpath);
		return 3;
	}

	magicbitboard_init();
	attackgen_init();
	bitboard_init();

	char fen[128] = { 0 };
	char move[64] = { 0 };
	char score[64] = { 0 };
	char ply[64] = { 0 };
	char result[64] = { 0 };
	char e[64] = { 0 };

	struct position pos, new;
	move_t m;
	int32_t eval;
	signed char resultnow = RESULT_UNKNOWN, resultsaved = RESULT_UNKNOWN;
	int newgame = 1;
	unsigned char flag;

	while (fgets(fen, sizeof(fen), in)) {
		fgets(move, sizeof(move), in);
		fgets(score, sizeof(score), in);
		fgets(ply, sizeof(ply), in);
		fgets(result, sizeof(result), in);
		fgets(e, sizeof(e), in);

		/* Remove newline. */
		fen[strcspn(fen, "\n")] = '\0';
		move[strcspn(move, "\n")] = '\0';
		result[strcspn(result, "\n")] = '\0';

		flag = 0;
		resultnow = result[7] == '1' ? RESULT_WIN : result[7] == '0' ? RESULT_DRAW : result[7] == '-' ? RESULT_LOSS : RESULT_UNKNOWN;

		if (resultnow == RESULT_UNKNOWN) {
			fprintf(stderr, "error: bad result '%s'\n", result);
			return 1;
		}

		if (move[4 + 5] == 0 && square(move + 2 + 5) == -1) {
			fprintf(stderr, "error: bad move '%s'\n", move);
			return 1;
		}

		if (!fen_is_ok2(fen + 4)) {
			newgame = 1;
			continue;
		}

		pos_from_fen2(&new, fen + 4);

		if (!newgame) {
			m = difference(&pos, &new);
			if (!m || resultnow != -(2 * pos.turn - 1) * resultsaved)
				newgame = 1;
			memcpy(&pos, &new, ESSENTIALPOSITION);
		}

		if (newgame) {
			pos_from_fen2(&pos, fen + 4);

			resultsaved = (2 * pos.turn - 1) * resultnow;
			write_move(out, 0);
			write_position(out, &pos);
			write_result(out, resultsaved);
		}
		else {
			write_move(out, m);
		}

		errno = 0;
		eval = strtol(score + 6, &endptr, 10);
		if (errno || *endptr != '\n') {
			fprintf(stderr, "error: %s", score);
			return 5;
		}

		eval = clamp(eval * scale_eval, -VALUE_WIN, VALUE_WIN);

		/* Skip if in check, or promotion or capture. */
		if (generate_checkers(&pos, pos.turn) || move[4 + 5] || pos.mailbox[square(move + 2 + 5)])
			flag |= FLAG_SKIP;

		write_eval(out, eval);
		write_flag(out, flag);

#if 0
		print_position(&pos);
		print_move(&m);
		printf("\n");
#endif

		newgame = 0;
	}

	fclose(in);
	fclose(out);

	printf("%lf\n", (double)count1 / count2);
}
