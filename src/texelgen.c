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
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "bitboard.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "position.h"
#include "move.h"

float parse_result(FILE *f) {
	char line[BUFSIZ];
	while (fgets(line, sizeof(line), f)) {
		if (strstr(line, "[Result")) {
			if (strstr(line, "1-0"))
				return 1;
			else if (strstr(line, "0-1"))
				return 0;
			else
				return 0.5;
		}
	}
	return -1;
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

void write_fens(struct position *pos, float result, FILE *f, int fd) {
	char *ptr[2], line[BUFSIZ];
	move m;
	int flag = 0;
	while ((ptr[0] = fgets(line, sizeof(line), f))) {
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
			if ((m = string_to_move(pos, ptr[0]))) {
				if (write(fd, pos, sizeof(struct compressedposition)) == -1)
					printf("WRITE ERROR\n");
				if (write(fd, &result, sizeof(result)) == -1)
					printf("WRITE ERROR\n");
				do_move(pos, &m);
			}
			/* break if mate has been found */
			else if (strchr(ptr[0], 'M')) {
				return;
			}
			ptr[0] = ptr[1] + 1;
		}
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("provide a filename\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "r");
	if (!f) {
		printf("could not open %s\n", argv[1]);
		return 2;
	}
	int fd = open("texel.bin", O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (fd == -1) {
		printf("could not open texel.bin\n");
		return 3;
	}

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	position_init();

	struct position pos[1];
	float result;
	while ((result = parse_result(f)) != -1) {
		start_fen(pos, f);
		write_fens(pos, result, f, fd);
	}

	fclose(f);
	close(fd);
}
