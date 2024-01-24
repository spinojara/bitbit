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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "nnue.h"
#include "util.h"

typedef int16_t ft_bias_t;
typedef int16_t ft_weight_t;

static ft_weight_t ft_weights[(K_HALF_DIMENSIONS + 1) * FT_IN_DIMS];

void read_ft_weights(char *filename) {
	FILE *f = fopen(filename, "rb");
	if (!f) {
		printf("could not open file\n");
		exit(1);
	}
	for (int i = 0; i < K_HALF_DIMENSIONS + 1; i++)
		read_le_uint(f, sizeof(ft_bias_t));
	for (int i = 0; i < (K_HALF_DIMENSIONS + 1) * FT_IN_DIMS; i++)
		ft_weights[i] = (ft_weight_t)read_le_uint(f, sizeof(*ft_weights));
	fclose(f);
}

void image_ft(int32_t *image) {
	for (int y = 0; y < 2560; y++) {
		for (int x = 0; x < 4096; x++) {
			int rex = x / (8 * 16);
			int rey = y / (8 * 40);
			int ren = rex + 32 * rey;
			
			int sqx = (x % (8 * 16)) / 8;
			int sqy = (y % (8 * 40)) / 8;

			int ksqx = x % 8;
			int ksqy = y % 8;

			int king_square = ksqx + 8 * (7 - ksqy);
			int square = (sqx % 8) + 8 * (7 - (sqy % 8));
			int turn = sqx < 8;
			int piece = 1 + (sqy / 8) + 6 * (1 - turn);
			int index = make_index(turn, square, piece, king_square);

			image[x + 4096 * y] = ABS(ft_weights[ren + (K_HALF_DIMENSIONS + 1) * index]);
		}
	}
}

void image_psqt(int32_t *image, int piece) {
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			int square = x + 8 * (7 - y);
			
			int32_t value = 0;
			int num = 0;
			for (int turn = 0; turn <= 1; turn++) {
				for (int king_square = 0; king_square < 64; king_square++) {
					if (king_square == square)
						continue;
					num++;
					int index = make_index(turn, square, piece, king_square);
					value += (2 * turn - 1) * ft_weights[256 + (K_HALF_DIMENSIONS + 1) * index];
				}
			}

			if (piece == PAWN && (y == 0 || y == 7)) {
				value = 0;
				num = 1;
			}

			image[x + 8 * y] = value / num;
		}
	}
}
