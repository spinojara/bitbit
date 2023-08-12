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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>

#include "util.h"
#include "bitboard.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "position.h"
#include "move.h"
#include "evaluate.h"
#include "search.h"
#include "option.h"
#include "tables.h"
#include "moveorder.h"
#include "endgame.h"

const int quiet_eval_delta = 50;

int skip_mates = 0;
int shuffle = 0;
int quiet = 0;
int skip_first = 0;
int skip_endgames = 0;
int skip_halfmove = 0;

int quiescence_eval_differs(struct position *pos) {
	struct searchinfo si = { 0 };
	int32_t q = quiescence(pos, 0, -VALUE_MATE, VALUE_MATE, &si);
	int32_t e = evaluate_classical(pos);
	return ABS(q - e) > quiet_eval_delta;
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

void write_fens(struct position *pos, int result, FILE *fin, FILE *fout) {
	char *ptr[2] = { 0 }, line[BUFSIZ];
	move m = 0;
	int moves = 0;
	int flag = 0;
	int16_t perspective_result;
	const uint16_t zero = 0;

	while ((ptr[0] = fgets(line, sizeof(line), fin))) {
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

			m = string_to_move(pos, ptr[0]);
			if (m) {
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

					if (quiet && (generate_checkers(pos, pos->turn) || quiescence_eval_differs(pos)))
						skip = 1;

					if (skip)
						perspective_result = VALUE_NONE;

					/* This is the first written move. */
					if (moves == skip_first) {
						fwrite(&zero, 2, 1, fout);
						fwrite(pos, sizeof(struct partialposition), 1, fout);
					}

					fwrite(&perspective_result, 2, 1, fout);
					fwrite(&m, 2, 1, fout);
				}
				do_move(pos, &m);
				moves++;
			}

			ptr[0] = ptr[1] + 1;
		}
	}
early_exit:

	/* If at least one move has been written. */
	if (moves > skip_first) {
		perspective_result = VALUE_NONE;
		fwrite(&perspective_result, 2, 1, fout);
	}
}

int main(int argc, char **argv) {
	char *infilename = NULL;
	char *outfilename = "out.bin";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--skip-mates"))
			skip_mates = 1;
		else if (!strcmp(argv[i], "--shuffle"))
			shuffle = 1;
		else if (!strcmp(argv[i], "--quiet"))
			quiet = 1;
		else if (!strcmp(argv[i], "--skip-endgames"))
			skip_endgames = 1;
		else if (!strcmp(argv[i], "--skip-halfmove"))
			skip_halfmove = 1;
		else if (!strcmp(argv[i], "--skip-first")) {
			i++;
			if (!(i < argc))
				break;
			skip_first = strint(argv[i]);
		}
		else if (strncmp(argv[i], "--", 2)) {
			if (!infilename)
				infilename = argv[i];
			else
				outfilename = argv[i];
		}
		else {
			printf("ignoring unknown option: %s\n", argv[i]);
		}
	}
	if (!infilename) {
		fprintf(stderr, "provide a filename\n");
		exit(1);
	}
	FILE *fin = fopen(infilename, "r");
	if (!fin) {
		fprintf(stderr, "failed to open file \"%s\"\n", infilename);
		exit(2);
	}
	FILE *fout = fopen(outfilename, "wb");
	if (!fout) {
		fprintf(stderr, "failed to open file \"%s\"\n", outfilename);
		exit(3);
	}

	option_nnue = 0;
	option_transposition = 0;
	option_history = 0;
	option_endgame = 0;
	option_damp = 0;

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	tables_init();
	search_init();
	moveorder_init();
	position_init();
	endgame_init();
	uint64_t seed = time(NULL);

	size_t total = 0, count;
	char line[BUFSIZ];
	while (fgets(line, sizeof(line), fin))
		if (strstr(line, "[Round"))
			total++;

	long *offset = malloc(total * sizeof(*offset));
	fseek(fin, 0, SEEK_SET);
	count = 0;
	while (fgets(line, sizeof(line), fin))
		if (strstr(line, "[Round"))
			offset[count++] = ftell(fin);

	/* Fisher-Yates shuffle */
	if (shuffle) {
		for (size_t i = total - 1; i > 0; i--) {
			size_t j = xorshift64(&seed) % (i + 1);
			long t = offset[i];
			offset[i] = offset[j];
			offset[j] = t;
		}
	}

	struct position pos;
	fseek(fin, 0, SEEK_SET);
	for (count = 0; count < total; count++) {
		fseek(fin, offset[count], SEEK_SET);
		int result = parse_result(fin);
		printf("collecting data: %lu\r", count + 1);
		start_fen(&pos, fin);
		write_fens(&pos, result, fin, fout);
	}

	free(offset);
	fclose(fin);
	fclose(fout);
}
