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

#include "nnuefile.h"

#include <stdio.h>

#include "io.h"

int nnuefile(FILE *f, ft_weight_t *ft_weights,
		ft_bias_t *ft_biases,
		ft_weight_t *psqt_weights,
		weight_t (*hidden1_weights)[16 * FT_OUT_DIMS],
		bias_t (*hidden1_biases)[16],
		weight_t (*hidden2_weights)[32 * 16],
		bias_t (*hidden2_biases)[32],
		weight_t (*output_weights)[1 * 32],
		bias_t (*output_biases)[1]) {

	uint16_t version;
	if (read_uintx(f, &version, sizeof(version)))
		return 1;

	if (version != VERSION_NNUE)
		return 2;

	int i, j, k;
	for (i = 0; i < K_HALF_DIMENSIONS; i++) {
		if (read_uintx(f, &ft_biases[i], sizeof(*ft_biases)))
			return 1;
	}
	for (i = 0; i < 8; i++)
		read_uintx(f, NULL, sizeof(*ft_biases));
	for (i = j = 0; i < (K_HALF_DIMENSIONS + 8) * FT_IN_DIMS; i++) {
		if (i % (K_HALF_DIMENSIONS + 8) >= K_HALF_DIMENSIONS) {
			if (read_uintx(f, &psqt_weights[j++], sizeof(*psqt_weights)))
				return 1;
		}
		else {
			if (read_uintx(f, &ft_weights[i - j], sizeof(*ft_weights)))
				return 1;
		}
	}

	for (j = 0; j < 8; j++)
		for (i = 0; i < 16; i++)
			if (read_uintx(f, &hidden1_biases[j][i], sizeof(**hidden1_biases)))
				return 1;

	for (j = 0; j < 8; j++)
		for (i = 0; i < 16; i++)
			for (k = 0; k < FT_OUT_DIMS; k++)
				if (read_uintx(f, &hidden1_weights[j][16 * k + i], sizeof(**hidden1_weights)))
					return 1;

	for (j = 0; j < 8; j++)
		for (i = 0; i < 32; i++)
			if (read_uintx(f, &hidden2_biases[j][i], sizeof(**hidden2_biases)))
				return 1;
	for (j = 0; j < 8; j++)
		for (i = 0; i < 32; i++)
			for (k = 0; k < 16; k++)
				if (read_uintx(f, &hidden2_weights[j][32 * k + i], sizeof(**hidden2_weights)))
					return 1;

	for (j = 0; j < 8; j++)
		if (read_uintx(f, output_biases[j], sizeof(**output_biases)))
			return 1;
	for (j = 0; j < 8; j++)
		for (i = 0; i < 32; i++)
			if (read_uintx(f, &output_weights[j][i], sizeof(**output_weights)))
				return 1;

	/* This should be the end of file. */
	return !read_uintx(f, NULL, 1) || !feof(f) || ferror(f);
}
