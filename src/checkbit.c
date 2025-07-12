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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "position.h"
#include "move.h"
#include "io.h"
#include "evaluate.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "bitboard.h"
#include "position.h"
#include "util.h"

int ends_with(const char *string, const char *end) {
	unsigned len = strlen(end);
	if (len > strlen(string))
		return 1;

	return !strcmp(string + (strlen(string) - len), end);
}

int main(int argc, char **argv) {
	if (argc < 2 || argc >= 4) {
		fprintf(stderr, "usage: %s [--shuffle] file\n", argv[0]);
		return 1;
	}
	int shuffle = 0;
	int i = 1;
	if (argc == 3) {
		if (!strcmp(argv[1], "--shuffle")) {
			shuffle = 1;
			i = 2;
		}
		else if (!strcmp(argv[2], "--shuffle")) {
			shuffle = 1;
			i = 1;
		}
	}
	if (!ends_with(argv[i], ".bit"))
		return 11;
	FILE *f = fopen(argv[i], "rb");
	if (!f) {
		fprintf(stderr, "error: failed to open file '%s'\n", argv[i]);
		return 2;
	}

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	position_init();

	struct position pos = { 0 };
	char result;
	int32_t eval;
	move_t move;
	unsigned char flag;

	int first = 1;

	size_t games = 0;
	uint64_t *start = NULL;
	uint64_t *end = NULL;

	while (1) {
		int r;
		if ((r = read_move(f, &move))) {
			if (r == 2 && feof(f))
				break;
			else
				return 3;
		}

		if (move) {
			if (first)
				return 9;
			struct pstate ps;
			pstate_init(&pos, &ps);
			char movestr[16];
			char fen[128];
			if (!pseudo_legal(&pos, &ps, &move) || !legal(&pos, &ps, &move)) {
				fprintf(stderr, "error: illegal move %s for position %s\n", move_str_algebraic(movestr, &move), pos_to_fen(fen, &pos));
				return 4;
			}
			do_move(&pos, &move);
		}
		else {
			if (shuffle) {
				games++;
				start = realloc(start, sizeof(*start) * games);
				end = realloc(end, sizeof(*end) * games);
				start[games - 1] = ftell(f) - 2;
				if (games >= 2)
					end[games - 2] = start[games - 1];
			}
			if (read_position(f, &pos))
				return 5;
			if (!pos_is_ok(&pos))
				return 6;
			if (read_result(f, &result) || (result != RESULT_LOSS && result != RESULT_DRAW && result != RESULT_WIN && result != RESULT_UNKNOWN))
				return 7;
		}
		first = 0;

		if (read_eval(f, &eval) || (eval != VALUE_NONE && (eval < -VALUE_INFINITE || eval > VALUE_INFINITE)))
			return 8;
		if (read_flag(f, &flag))
			return 10;
	}

	if (!shuffle)
		return 0;

	if (games >= 1)
		end[games - 1] = ftell(f);

	uint64_t seed = time(NULL);
	for (size_t k = games - 1; k > 0; k--) {
		size_t j = xorshift64(&seed) % (k + 1);
		long t;
		t = start[k];
		start[k] = start[j];
		start[j] = t;
		t = end[k];
		end[k] = end[j];
		end[j] = t;
	}

	char *str = malloc(strlen(argv[i]) + 10);
	if (!str) {
		fprintf(stderr, "error: malloc\n");
		return 12;
	}

	strcpy(str, argv[i]);
	str[strlen(str) - 4] = '\0';
	strcat(str, ".shuffled.bit");
	FILE *g = fopen(str, "wbx");
	if (!g) {
		fprintf(stderr, "error: file '%s' exists\n", str);
		return 13;
	}

	for (size_t k = 0; k < games; k++) {
		char *bytes = malloc(end[k] - start[k]);
		if (!bytes) {
			fprintf(stderr, "error: malloc\n");
			return 13;
		}

		if (fseek(f, start[k], SEEK_SET)) {
			fprintf(stderr, "error: fseek\n");
			return 14;
		}
		if (fread(bytes, 1, end[k] - start[k], f) != end[k] - start[k]) {
			fprintf(stderr, "error: fread\n");
			return 15;
		}
		if (fwrite(bytes, 1, end[k] - start[k], g) != end[k] - start[k]) {
			fprintf(stderr, "error: fwrite\n");
			return 16;
		}

		free(bytes);
	}

	fclose(g);
	return 0;
}
