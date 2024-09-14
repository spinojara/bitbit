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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include <getopt.h>
#include <errno.h>

#include "util.h"
#include "bitboard.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "position.h"
#include "move.h"
#include "option.h"
#include "moveorder.h"
#include "endgame.h"
#include "movegen.h"
#include "movepicker.h"
#include "tables.h"
#include "io.h"

int skip_mates = 0;
int shuffle = 0;
int quiet = 0;
int skip_endgames = 0;
int skip_halfmove = 0;
int skip_checks = 0;
int skip_unlucky = 0;
int verbose = 0;

static const struct searchinfo gsi = { 0 };

int32_t evaluate_material(const struct position *pos) {
	int32_t eval = 0;
	for (int piece = PAWN; piece < KING; piece++)
		eval += popcount(pos->piece[WHITE][piece]) * material_value[piece];
	for (int piece = PAWN; piece < KING; piece++)
		eval -= popcount(pos->piece[BLACK][piece]) * material_value[piece];
	return pos->turn ? eval : -eval;
}

int32_t total_material(const struct position *pos) {
	int32_t mat = 0;
	for (int piece = PAWN; piece < KING; piece++)
		mat += popcount(pos->piece[WHITE][piece]) * material_value[piece];
	for (int piece = PAWN; piece < KING; piece++)
		mat += popcount(pos->piece[BLACK][piece]) * material_value[piece];
	return mat;
}

int32_t search_material(struct position *pos, int alpha, int beta) {
	uint64_t checkers = generate_checkers(pos, pos->turn);
	int32_t eval = evaluate_material(pos), best_eval = -VALUE_INFINITE;

	if (!checkers) {
		if (eval >= beta)
			return beta;
		if (eval > alpha)
			alpha = eval;
		best_eval = eval;
	}

	struct pstate pstate;
	pstate_init(pos, &pstate);
	struct movepicker mp;
	movepicker_init(&mp, 1, pos, &pstate, 0, 0, 0, 0, &gsi);
	move_t move;
	while ((move = next_move(&mp))) {
		if (!legal(pos, &pstate, &move))
			continue;
		
		do_move(pos, &move);
		eval = -search_material(pos, -beta, -alpha);
		undo_move(pos, &move);
		
		if (eval > best_eval) {
			best_eval = eval;
			if (eval > alpha) {
				alpha = eval;
				if (eval >= beta)
					break;
			}
		}
	}

	return best_eval;
}

int parse_result(FILE *f) {
	char line[BUFSIZ];
	while (fgets(line, sizeof(line), f)) {
		if (strstr(line, "[Result")) {
			if (strstr(line, "1-0"))
				return 1;
			else if (strstr(line, "0-1"))
				return -1;
			else
				return 0;
		}
	}
	return 2;
}

void start_fen(struct position *pos, FILE *f) {
	char line[BUFSIZ];
	while (fgets(line, sizeof(line), f)) {
		if (strstr(line, "[FEN")) {
			char *ptr, *fen[6];
			int i = 0;
			fen[i++] = line + 6;
			for (; i < 7; i++) {
				ptr = strchr(fen[i - 1], ' ');
				if (!ptr)
					break;
				*ptr = '\0';
				if (i < 6)
					fen[i] = ptr + 1;
			}
			if ((ptr = strchr(fen[i - 1], '"')))
				*ptr = '\0';
			pos_from_fen(pos, i, fen);
			return;
		}
	}
	return;
}

void line_error(FILE *f, const char *error) {
	long newlines = 0;
	long chars = 0;
	long p = ftell(f);
	fseek(f, 0, SEEK_SET);
	for (long q = 1; q <= p; q++) {
		chars++;
		if (fgetc(f) == '\n') {
			newlines++;
			chars = 0;
		}
		else if (q == p)
			newlines++;
	}
	fprintf(stderr, "error: %s around line %ld %ld\n", error, newlines, chars);
	exit(1);
}

void line_expect(FILE *f, const char *expect, const char *got) {
	char error[3 * BUFSIZ];
	sprintf(error, "expected '%s' but got '%s'", expect, got);
	line_error(f, error);
}

char *parse_quote(char *line, char *quotes) {
	int in_quote = 0;
	int index = 0;

	for (char *c = line; *c; c++) {
		if (*c == '"') {
			in_quote = !in_quote;
			continue;
		}

		if (in_quote) {
			quotes[index++] = *c;
			quotes[index] = '\0';
		}
	}

	return index && in_quote == 0 ? quotes : NULL;
}

char *next_token(char *token, int n, FILE *f) {
	int c, index = 0, in_braces = 0;
	token[0] = '\0';

	while (index < n - 1) {
		c = fgetc(f);
		switch (c) {
		case EOF:
			line_error(f, "unexpected EOF");
			break;
		case '{':
			in_braces = 1;
			break;
		case '}':
			in_braces = 0;
			break;
		case '\n':
		case ' ':
			if (!in_braces && index)
				return token;
			/* fallthrough */
		default:
			token[index++] = c;
			token[index] = '\0';
		}
	}
	return token;
}

void parse_pgn(FILE *infile, FILE *outfile) {
	struct position pos;
	move_t move;
	int32_t eval;
	int done_early = 0;
	startpos(&pos);
	int plycount = -1;
	int result = -1;
	char white[BUFSIZ] = { 0 };
	char black[BUFSIZ] = { 0 };
	char line[BUFSIZ], quotes[BUFSIZ], *endptr;
	char token[BUFSIZ] = { 0 }, expect[BUFSIZ] = { 0 };

	while (fgets(line, sizeof(line), infile)) {
		if (!strncmp(line, "[White ", 7)) {
			if (!parse_quote(line, white))
				line_error(infile, "bad player");
		}
		else if (!strncmp(line, "[Black ", 7)) {
			if (!parse_quote(line, black))
				line_error(infile, "bad player");
		}
		else if (!strncmp(line, "[FEN ", 5)) {
			if (!parse_quote(line, quotes) || !fen_is_ok2(quotes))
				line_error(infile, "bad fen");
			pos_from_fen2(&pos, quotes);
		}
		else if (!strncmp(line, "[Result ", 8)) {
			if (!parse_quote(line, quotes))
				line_error(infile, "bad result");

			if (!strcmp(quotes, "1-0"))
				result = RESULT_WIN;
			else if (!strcmp(quotes, "0-1"))
				result = RESULT_LOSS;
			else if (!strcmp(quotes, "1/2-1/2"))
				result = RESULT_DRAW;
			else
				line_error(infile, "bad result");
		}
		else if (!strncmp(line, "[PlyCount ", 10)) {
			errno = 0;
			if (!parse_quote(line, quotes) || (plycount = strtol(quotes, &endptr, 10)) <= 0 || errno || *endptr != '\0')
				line_error(infile, "bad plycount");
		}
		else if (!strncmp(line, "[Termination ", 13)) {
			if (!strstr(line, "adjudication")) {
				fprintf(stderr, "warning: skipping bad game\n");
				return;
			}
		}

		if (!strcmp(line, "\n"))
			break;
	}

	if (result == -1)
		line_error(infile, "no result");

	if (plycount == -1)
		line_error(infile, "no plycount");

	write_move(outfile, 0);
	write_position(outfile, &pos);
	write_result(outfile, result);

	/* Special case when black is the first to move. */
	if (!pos.turn) {
		sprintf(expect, "%d...", pos.fullmove);
		if (strcmp(expect, next_token(token, sizeof(token), infile)))
			line_expect(infile, expect, token);
	}

	for (int i = 1; i <= plycount; i++) {
		if (pos.turn) {
			sprintf(expect, "%d.", pos.fullmove);
			if (strcmp(expect, next_token(token, sizeof(token), infile)))
				line_expect(infile, expect, token);
		}

		/* move */
		next_token(token, sizeof(token), infile);
		move = string_to_move(&pos, token);
		if (!move)
			line_error(infile, "bad move");



		/* comment */
		eval = VALUE_NONE;
		next_token(token, sizeof(token), infile);

		endptr = strchr(token, '/');
		if (endptr) {
			*endptr = '\0';
			if (token[1] == 'M') {
				if (skip_mates) {
					if (done_early == 0)
						done_early = 1;
					eval = VALUE_NONE;
				}
				else {
					errno = 0;
					int ply = strtol(&token[2], &endptr, 10);
					if ((token[0] != '+' && token[0] != '-') || errno || *endptr != '\0' || ply <= 0)
						line_error(infile, "bad mate score");
					eval = (2 * (token[0] == '+') - 1) * (VALUE_MATE - ply);
				}
			}
			else {
				errno = 0;
				double score = strtod(token, &endptr);
				if (errno || *endptr != '\0')
					line_error(infile, "bad score");
				eval = 100 * score;
			}
		}

		if (eval != VALUE_NONE && skip_endgames) {
			refresh_endgame_key(&pos);
			if (endgame_probe(&pos)) {
				if (done_early == 0)
					done_early = 1;
				eval = VALUE_NONE;
			}
		}

		if (eval != VALUE_NONE && skip_halfmove && !gbernoulli(exp(-pos.halfmove)))
			eval = VALUE_NONE;

		if (eval != VALUE_NONE && quiet && (generate_checkers(&pos, pos.turn) ||
					is_capture(&pos, &move) ||
					move_flag(&move) == MOVE_EN_PASSANT ||
					move_flag(&move) == MOVE_PROMOTION ||
					search_material(&pos, -VALUE_INFINITE, VALUE_INFINITE) != evaluate_material(&pos)))
			eval = VALUE_NONE;

		if (eval != VALUE_NONE && skip_unlucky && result == RESULT_DRAW) {
			int32_t mat = total_material(&pos);
			int32_t eval1, eval2;
			if (mat <= 2000 && abs(eval1 = evaluate_material(&pos)) >= 100 &&
					abs(eval2 = evaluate_classical(&pos)) >= 50 &&
					(double)eval1 * (double)eval2 > 0.0) {
				if (done_early == 0)
					done_early = 1;
				eval = VALUE_NONE;
			}
		}

		do_move(&pos, &move);
		if (eval != VALUE_NONE && skip_checks && generate_checkers(&pos, pos.turn))
			eval = VALUE_NONE;

		if (done_early <= 1) {
			write_eval(outfile, eval);
		}
		if (done_early == 0) {
			if (i < plycount)
				write_move(outfile, move);
		}
		if (done_early == 1)
			done_early = 2;
	}

	sprintf(expect, "%s", result == RESULT_WIN ? "1-0" : result == RESULT_LOSS ? "0-1" : "1/2-1/2");
	if (strcmp(expect, next_token(token, sizeof(token), infile)))
		line_expect(infile, expect, token);
}

int main(int argc, char **argv) {
	char *inpath;
	char *outpath;
	static struct option opts[] = {
		{ "verbose",       no_argument,       NULL, 'v' },
		{ "skip-mates",    no_argument,       NULL, 'm' },
		{ "shuffle",       no_argument,       NULL, 's' },
		{ "quiet",         no_argument,       NULL, 'q' },
		{ "skip-checks",   no_argument,       NULL, 'c' },
		{ "skip-endgames", no_argument,       NULL, 'e' },
		{ "skip-halfmove", no_argument,       NULL, 'h' },
		{ "skip-unlucky",  no_argument,       NULL, 'u' },
		{ NULL,            0,                 NULL,  0  },
	};
	int c, option_index = 0;
	int error = 0;
	while ((c = getopt_long(argc, argv, "vmsqcehu", opts, &option_index)) != -1) {
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'm':
			skip_mates = 1;
			break;
		case 's':
			shuffle = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'c':
			skip_checks = 1;
			break;
		case 'e':
			skip_endgames = 1;
			break;
		case 'h':
			skip_halfmove = 1;
			break;
		case 'u':
			skip_unlucky = 1;
			break;
		default:
			error = 1;
			break;
		}
	}
	if (error)
		return 1;
	if (optind + 1 >= argc) {
		fprintf(stderr, "usage: %s infile outfile\n", argv[0]);
		return 3;
	}
	inpath = argv[optind];
	outpath = argv[optind + 1];

	FILE *infile = fopen(inpath, "r");
	if (!infile) {
		fprintf(stderr, "failed to open file \"%s\"\n", inpath);
		return 2;
	}
	FILE *outfile = fopen(outpath, "w");
	if (!outfile) {
		fprintf(stderr, "failed to open file \"%s\"\n", outpath);
		return 2;
	}

	option_nnue = 0;
	option_transposition = 0;
	option_history = 0;
	option_endgame = 1;
	option_damp = 0;

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	tables_init();
	moveorder_init();
	position_init();
	endgame_init();
	uint64_t seed = time(NULL);

	size_t total = 0, count;
	char line[BUFSIZ];
	while (fgets(line, sizeof(line), infile))
		if (!strncmp(line, "[Event ", 7))
			total++;

	long *offset = malloc(total * sizeof(*offset));
	fseek(infile, 0, SEEK_SET);
	count = 0;
	while (fgets(line, sizeof(line), infile))
		if (!strncmp(line, "[Event ", 7))
			offset[count++] = ftell(infile) - strlen(line);

	/* Fisher-Yates shuffle. */
	if (shuffle) {
		for (size_t i = total - 1; i > 0; i--) {
			size_t j = xorshift64(&seed) % (i + 1);
			long t = offset[i];
			offset[i] = offset[j];
			offset[j] = t;
		}
	}

	for (count = 0; count < total; count++) {
		fseek(infile, offset[count], SEEK_SET);
		if (verbose)
			printf("%ld / %ld\n", count + 1, total);
		parse_pgn(infile, outfile);
	}

	free(offset);
	fclose(infile);
	fclose(outfile);
}
