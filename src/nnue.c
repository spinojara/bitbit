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

#include "nnue.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <stdlib.h>

#include "bitboard.h"
#include "util.h"
#include "evaluate.h"

#define TRANSFORMER_START (3 * 4 + 177)
#define NETWORK_START (TRANSFORMER_START + 4 + 2 * 256 + 2 * 256 * 64 * 641)

#define K_HALF_DIMENSIONS (256)
#define FT_IN_DIMS (64 * PS_END)
#define FT_OUT_DIMS (K_HALF_DIMENSIONS * 2)

#define SHIFT (6)

typedef int16_t ft_weight_t;
typedef int16_t ft_bias_t;
typedef int8_t weight_t;
typedef int32_t bias_t;
typedef uint16_t mask_t;

static ft_weight_t ft_weights[K_HALF_DIMENSIONS * FT_IN_DIMS];
static ft_bias_t ft_biases[K_HALF_DIMENSIONS];

static weight_t hidden1_weights[32 * FT_OUT_DIMS];
static bias_t hidden1_biases[32];

static weight_t hidden2_weights[32 * 32];
static bias_t hidden2_biases[32];

static weight_t output_weights[1 * 32];
static bias_t output_biases[1];

static inline void update_accumulator(struct position *pos, int16_t (*accumulation)[2][K_HALF_DIMENSIONS], int turn) {
	memcpy((*accumulation)[turn], ft_biases, K_HALF_DIMENSIONS * sizeof(ft_bias_t));
	struct index active_indices[1];
	append_active_indices(pos, active_indices, turn);

	for (int i = 0; i < active_indices->size; i++) {
		unsigned index = active_indices->values[i];
		unsigned offset = K_HALF_DIMENSIONS * index;
		for (int j = 0; j < K_HALF_DIMENSIONS; j++)
			(*accumulation)[turn][j] += ft_weights[offset + j];
	}
}

static inline void transform(struct position *pos, int8_t *output, mask_t *mask) {
	UNUSED(mask);
	int16_t accumulation[1][2][K_HALF_DIMENSIONS], sum;
	update_accumulator(pos, accumulation, 0);
	update_accumulator(pos, accumulation, 1);

	for (int i = 0; i < K_HALF_DIMENSIONS; i++) {
		sum = (*accumulation)[pos->turn][i];
		output[i] = CLAMP(sum, 0, 127);
	}

	for (int i = 0; i < K_HALF_DIMENSIONS; i++) {
		sum = (*accumulation)[1 - pos->turn][i];
		output[K_HALF_DIMENSIONS + i] = CLAMP(sum, 0, 127);
	}
}

static inline void affine_propagate(int8_t *input, int8_t *output, int in_dim, int out_dim,
		const bias_t *biases, const weight_t *weights) {
	int i, j;
	int32_t tmp[out_dim];
	memcpy(tmp, biases, out_dim * sizeof(biases[0]));

	for (i = 0; i < in_dim; i++)
		if (input[i])
			for (j = 0; j < out_dim; j++)
				tmp[j] += input[i] * weights[out_dim * i + j];

	for (i = 0; i < out_dim; i++)
		output[i] = CLAMP(tmp[i] >> SHIFT, 0, 127);
}

static inline int16_t output_layer(int8_t *input, const bias_t *biases, const weight_t *weights) {
	int32_t sum = biases[0];
	for (int i = 0; i < 32; i++)
		sum += weights[i] * input[i];
	return sum;
}

struct data {
	int8_t ft_out[FT_OUT_DIMS];
	int8_t hidden1_out[32];
	int8_t hidden2_out[32];
};

int16_t evaluate_nnue(struct position *pos) {
	struct data buf;

	transform(pos, buf.ft_out, NULL);

	affine_propagate(buf.ft_out, buf.hidden1_out, FT_OUT_DIMS, 32, hidden1_biases, hidden1_weights);

	affine_propagate(buf.hidden1_out, buf.hidden2_out, 32, 32, hidden2_biases, hidden2_weights);

	return output_layer(buf.hidden2_out, output_biases, output_weights) / FV_SCALE;
}

void read_hidden_weights(weight_t *w, int dims, FILE *f) {
	for (int i = 0; i < 32; i++)
		for (int j = 0; j < dims; j++)
			w[j * 32 + i] = read_le_uint8(f);
}

void read_output_weights(weight_t *w, FILE *f) {
	for (int i = 0; i < 32; i++)
		w[i] = read_le_uint8(f);
}

void weights_init(FILE *f) {
	memset(ft_weights, 0, sizeof(ft_weights));
	memset(ft_biases, 0, sizeof(ft_biases));
	memset(hidden1_weights, 0, sizeof(hidden1_weights));
	memset(hidden1_biases, 0, sizeof(hidden1_biases));
	memset(hidden2_weights, 0, sizeof(hidden2_weights));
	memset(hidden2_biases, 0, sizeof(hidden2_biases));
	memset(output_weights, 0, sizeof(output_weights));
	memset(output_biases, 0, sizeof(output_biases));
	
	fseek(f, TRANSFORMER_START, SEEK_SET);
	/* skip 4 bytes */
	read_le_uint32(f);

	int i;
	for (i = 0; i < K_HALF_DIMENSIONS; i++)
		ft_biases[i] = read_le_uint16(f);
	for (i = 0; i < K_HALF_DIMENSIONS * FT_IN_DIMS; i++)
		ft_weights[i] = read_le_uint16(f);

	/* skip 4 bytes */
	read_le_uint32(f);

	for (i = 0; i < 32; i++)
		hidden1_biases[i] = read_le_uint32(f);
	read_hidden_weights(hidden1_weights, FT_OUT_DIMS, f);

	for (i = 0; i < 32; i++)
		hidden2_biases[i] = read_le_uint32(f);
	read_hidden_weights(hidden2_weights, 32, f);

	output_biases[0] = read_le_uint32(f);
	read_output_weights(output_weights, f);
}

void ft_print_min_max(ft_weight_t *weights, ft_bias_t *biases, int in1, int in2) {
	int max = -1000000;
	int min = 1000000;
	for (int i = 0; i < in1 * in2; i++) {
		if (weights[i] > max)
			max = weights[i];
		if (weights[i] < min)
			min = weights[i];
	}
	printf("weights: %i <= %i\n", min, max);

	max = -1000000;
	min = 1000000;
	for (int i = 0; i < in1; i++) {
		if (biases[i] > max)
			max = biases[i];
		if (biases[i] < min)
			min = biases[i];
	}
	printf("biases: %i <= %i\n", min, max);
}

void print_min_max(weight_t *weights, bias_t *biases, int in1, int in2) {
	int max = -1000000;
	int min = 1000000;
	for (int i = 0; i < in1 * in2; i++) {
		if (weights[i] > max)
			max = weights[i];
		if (weights[i] < min)
			min = weights[i];
	}
	printf("weights: %i <= %i\n", min, max);

	max = -1000000;
	min = 1000000;
	for (int i = 0; i < in1; i++) {
		if (biases[i] > max)
			max = biases[i];
		if (biases[i] < min)
			min = biases[i];
	}
	printf("biases: %i <= %i\n", min, max);
}

int nnue_init(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);
	FILE *f = fopen("nn-eba324f53044.nnue", "rb");
	//FILE *f = fopen("nn-ad9b42354671.nnue", "rb");
	if (!f)
		return 1;
	printf("\n");
	weights_init(f);
	fclose(f);
	ft_print_min_max(ft_weights, ft_biases, K_HALF_DIMENSIONS, FT_IN_DIMS);
	print_min_max(hidden1_weights, hidden1_biases, 32, FT_OUT_DIMS);
	print_min_max(hidden2_weights, hidden2_biases, 32, 32);
	print_min_max(output_weights, output_biases, 1, 32);


	struct position pos[1];
	char *fen[] = { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", "0", "1", };
	pos_from_fen(pos, SIZE(fen), fen);
	print_position(pos, 0); 
	printf("\n%i\n", evaluate_nnue(pos) / 16);
	return 0;
}
