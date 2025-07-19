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

#include "nnuefile.h"

#include <stdio.h>

#include "io.h"

int nnuefile(FILE *f, ft_weight_t *ft_weights,
		ft_bias_t *ft_biases,
		ft_weight_t *psqt_weights,
		weight_t *hidden1_weights,
		bias_t *hidden1_biases,
		weight_t *hidden2_weights,
		bias_t *hidden2_biases,
		weight_t *output_weights,
		bias_t *output_biases) {

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
	for (i = 0; i < 1; i++)
		read_uintx(f, NULL, sizeof(*ft_biases));
	for (i = j = 0; i < (K_HALF_DIMENSIONS + 1) * FT_IN_DIMS; i++) {
		if (i % (K_HALF_DIMENSIONS + 1) >= K_HALF_DIMENSIONS) {
			if (read_uintx(f, &psqt_weights[j++], sizeof(*psqt_weights)))
				return 1;
		}
		else {
			if (read_uintx(f, &ft_weights[i - j], sizeof(*ft_weights)))
				return 1;
		}
	}

	for (i = 0; i < HIDDEN1_OUT_DIMS; i++)
		if (read_uintx(f, &hidden1_biases[i], sizeof(*hidden1_biases)))
			return 1;

	for (i = 0; i < HIDDEN1_OUT_DIMS; i++)
		for (k = 0; k < FT_OUT_DIMS; k++)
			if (read_uintx(f, &hidden1_weights[HIDDEN1_OUT_DIMS * k + i], sizeof(*hidden1_weights)))
				return 1;

	for (i = 0; i < HIDDEN2_OUT_DIMS; i++)
		if (read_uintx(f, &hidden2_biases[i], sizeof(*hidden2_biases)))
			return 1;
	for (i = 0; i < HIDDEN2_OUT_DIMS; i++)
		for (k = 0; k < HIDDEN1_OUT_DIMS; k++)
			if (read_uintx(f, &hidden2_weights[HIDDEN2_OUT_DIMS * k + i], sizeof(*hidden2_weights)))
				return 1;

	if (read_uintx(f, output_biases, sizeof(*output_biases)))
		return 1;
	for (i = 0; i < HIDDEN2_OUT_DIMS; i++)
		if (read_uintx(f, &output_weights[i], sizeof(*output_weights)))
			return 1;

	/* This should be the end of file. */
	return !read_uintx(f, NULL, 1) || !feof(f) || ferror(f);
}
