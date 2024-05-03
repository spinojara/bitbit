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
int skip_first = 0;
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

void write_fens(struct position *pos, int result, FILE *infile, FILE *outfile) {
	char *ptr[2] = { 0 }, line[BUFSIZ];
	move_t move = 0;
	int moves = 0;
	int flag = 0;
	int16_t perspective_result;

	while ((ptr[0] = fgets(line, sizeof(line), infile))) {
		if (*ptr[0] == '\n' || *ptr[0] == '[') {
			if (flag)
				break;
			else
				continue;
		}
		flag = 1;
		while (1) {
			ptr[1] = strchr(ptr[0], ' ');
			if (!ptr[1])
				break;
			*ptr[1] = '\0';

			/* Mate lookahead. */
			if (skip_mates)
				for (int i = 1; ptr[1][i] && ptr[1][i] != '\n' && ptr[1][i] != ' '; i++)
					if (ptr[1][i] == 'M')
						goto early_exit;

			move = string_to_move(pos, ptr[0]);
			if (move) {
				if (moves >= skip_first) {
					perspective_result = (2 * pos->turn - 1) * VALUE_MATE * result;
					if (skip_endgames) {
						refresh_endgame_key(pos);
						if (endgame_probe(pos))
							goto early_exit;
					}

					int skip = 0;
					if (skip_halfmove && !gbernoulli(exp(-pos->halfmove)))
						skip = 1;

					if (!skip && quiet && (generate_checkers(pos, pos->turn) ||
								is_capture(pos, &move) ||
								move_flag(&move) == MOVE_EN_PASSANT ||
								move_flag(&move) == MOVE_PROMOTION ||
								search_material(pos, -VALUE_INFINITE, VALUE_INFINITE) != evaluate_material(pos)))
						skip = 1;

					if (!skip && skip_checks) {
						do_move(pos, &move);
						if (generate_checkers(pos, pos->turn))
							skip = 1;
						undo_move(pos, &move);
					}

					if (!skip && skip_unlucky && perspective_result == 0) {
						int32_t mat = total_material(pos);
						int64_t eval1, eval2;
						if (mat <= 2000 &&
								abs((int32_t)(eval1 = evaluate_material(pos))) >= 100 &&
								abs((int32_t)(eval2 = evaluate_classical(pos))) >= 50 &&
								eval1 * eval2 > 0) {
							skip = 1;
						}
					}

					if (skip)
						perspective_result = VALUE_NONE;
					
					/* This is the first written move. */
					if (moves == skip_first) {
						write_move(outfile, 0);
						write_position(outfile, pos);
					}

					write_eval(outfile, perspective_result);
					write_move(outfile, move);
				}
				do_move(pos, &move);
				moves++;
			}

			ptr[0] = ptr[1] + 1;
		}
	}
early_exit:

	/* If at least one move has been written. */
	if (moves > skip_first)
		write_eval(outfile, VALUE_NONE);
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
		{ "skip-first",    required_argument, NULL, 'f' },
		{ "skip-unlucky",  no_argument,       NULL, 'u' },
		{ NULL,            0,                 NULL,  0  },
	};
	char *endptr;
	int c, option_index = 0;
	int error = 0;
	while ((c = getopt_long(argc, argv, "vmsqcehf:u", opts, &option_index)) != -1) {
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
		case 'f':
			skip_first = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				return 2;
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
		if (strstr(line, "[Round"))
			total++;

	long *offset = malloc(total * sizeof(*offset));
	fseek(infile, 0, SEEK_SET);
	count = 0;
	while (fgets(line, sizeof(line), infile))
		if (strstr(line, "[Round"))
			offset[count++] = ftell(infile);

	/* Fisher-Yates shuffle. */
	if (shuffle) {
		for (size_t i = total - 1; i > 0; i--) {
			size_t j = xorshift64(&seed) % (i + 1);
			long t = offset[i];
			offset[i] = offset[j];
			offset[j] = t;
		}
	}

	struct position pos;
	for (count = 1; count <= total; count++) {
		fseek(infile, offset[count], SEEK_SET);
		int result = parse_result(infile);
		if (verbose)
			printf("%ld / %ld\n", count, total);
		start_fen(&pos, infile);
		write_fens(&pos, result, infile, outfile);
	}

	free(offset);
	fclose(infile);
	fclose(outfile);
}
