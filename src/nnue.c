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

#include "nnue.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>
#include <assert.h>

#include "bitboard.h"
#include "util.h"
#include "evaluate.h"
#include "move.h"
#include "position.h"
#include "option.h"
#include "io.h"
#include "nnuefile.h"

#ifdef VNNI
#define AVX2
#endif
#ifndef NDEBUG
int nnue_init_done = 0;
#endif

#ifdef AVX2
#include <immintrin.h>
#endif

char pathnnue[4096];
int builtin;

struct data {
	alignas(64) int8_t ft_out[FT_OUT_DIMS];
	alignas(64) int8_t hidden1_out[16];
	alignas(64) int8_t hidden2_out[32];
};

extern alignas(64) ft_weight_t builtin_ft_weights[K_HALF_DIMENSIONS * FT_IN_DIMS];
extern alignas(64) ft_bias_t builtin_ft_biases[K_HALF_DIMENSIONS];

extern alignas(64) ft_weight_t builtin_psqt_weights[FT_IN_DIMS];

extern alignas(64) weight_t builtin_hidden1_weights[16 * FT_OUT_DIMS];
extern alignas(64) bias_t builtin_hidden1_biases[16];

extern alignas(64) weight_t builtin_hidden2_weights[32 * 16];
extern alignas(64) bias_t builtin_hidden2_biases[32];

extern alignas(64) weight_t builtin_output_weights[1 * 32];
extern alignas(64) bias_t builtin_output_biases[1];



alignas(64) ft_weight_t file_ft_weights[K_HALF_DIMENSIONS * FT_IN_DIMS];
alignas(64) ft_bias_t file_ft_biases[K_HALF_DIMENSIONS];

alignas(64) ft_weight_t file_psqt_weights[FT_IN_DIMS];

alignas(64) weight_t file_hidden1_weights[16 * FT_OUT_DIMS];
alignas(64) bias_t file_hidden1_biases[16];

alignas(64) weight_t file_hidden2_weights[32 * 16];
alignas(64) bias_t file_hidden2_biases[32];

alignas(64) weight_t file_output_weights[1 * 32];
alignas(64) bias_t file_output_biases[1];



ft_weight_t *ft_weights;
ft_bias_t *ft_biases;

ft_weight_t *psqt_weights;

weight_t *hidden1_weights;
bias_t *hidden1_biases;

weight_t *hidden2_weights;
bias_t *hidden2_biases;

weight_t *output_weights;
bias_t *output_biases;

#if defined(AVX2) && !defined(NDEBUG)
void print_m256i(__m256i a, int as) {
	int8_t b[32];
	int16_t c[16];
	int32_t d[8];
	switch (as) {
	case 8:
		memcpy(b, &a, 32);
		for (int i = 0; i < 32; i++)
			printf("%3d ", b[i]);
		printf("\n");
		break;
	case 16:
		memcpy(c, &a, 32);
		for (int i = 0; i < 16; i++)
			printf("%3d ", c[i]);
		printf("\n");
		break;
	case 32:
		memcpy(d, &a, 32);
		for (int i = 0; i < 8; i++)
			printf("%3d ", d[i]);
		printf("\n");
		break;
	default:
		printf("unrecognized int size\n");
		break;
	}
}
#endif

/* (AVX2) After transform, the output is in the wrong order. More precisely
 * output[8-15] is swapped with output[16-23] and
 * output[40-47] is swapped with output[48-55] etc.
 */
static inline void transform(const struct position *pos, const int16_t accumulation[2][K_HALF_DIMENSIONS], int8_t *output) {
#if defined(AVX2)
	const int perspective[2] = { pos->turn, other_color(pos->turn) };
	for (int j = 0; j < 2; j++) {
		__m256i *out = (__m256i *)(output + j * K_HALF_DIMENSIONS);
		for (int i = 0; i < K_HALF_DIMENSIONS / 32; i++) {
			__m256i p0 = ((__m256i *)accumulation[perspective[j]])[2 * i];
			__m256i p1 = ((__m256i *)accumulation[perspective[j]])[2 * i + 1];
			out[i] = _mm256_max_epi8(_mm256_packs_epi16(_mm256_srai_epi16(p0, FT_SHIFT), _mm256_srai_epi16(p1, FT_SHIFT)), _mm256_setzero_si256());
		}
	}
#else
	int16_t sum;
	for (int i = 0; i < K_HALF_DIMENSIONS; i++) {
		sum = accumulation[pos->turn][i];
		output[i] = clamp(sum >> FT_SHIFT, 0, 127);
		sum = accumulation[other_color(pos->turn)][i];
		output[K_HALF_DIMENSIONS + i] = clamp(sum >> FT_SHIFT, 0, 127);
	}
#endif
}


/* (AVX2) After affine_propagate1024to16, the output is in the right order. */
static inline void affine_propagate1024to16(int8_t *input, int8_t *output,
		const bias_t *biases, const weight_t *weights) {
#if defined(AVX2)
	__m256i out0 = ((__m256i *)biases)[0];
	__m256i out1 = ((__m256i *)biases)[1];
	__m128i out;
	__m256i weight, in, signs;

	for (int i = 0; i < FT_OUT_DIMS / 2; i++) {
		weight = ((__m256i *)weights)[i];
		in = _mm256_set1_epi16(input[2 * i] | (input[2 * i + 1] << 8));

		weight = _mm256_maddubs_epi16(in, weight);
		signs = _mm256_cmpgt_epi16(_mm256_setzero_si256(), weight);
		out0 = _mm256_add_epi32(out0, _mm256_unpacklo_epi16(weight, signs));
		out1 = _mm256_add_epi32(out1, _mm256_unpackhi_epi16(weight, signs));
	}

	out0 = _mm256_srai_epi16(_mm256_packs_epi32(out0, out1), SHIFT);
	out = _mm_packs_epi16(_mm256_castsi256_si128(out0), _mm256_extracti128_si256(out0, 1));
	*(__m128i *)output = _mm_max_epi8(out, _mm_setzero_si128());
#else
	int i, j;
	bias_t tmp[16];
	memcpy(tmp, biases, 16 * sizeof(biases[0]));

	for (i = 0; i < FT_OUT_DIMS; i++)
		if (input[i])
			for (j = 0; j < 16; j++)
				tmp[j] += input[i] * weights[16 * i + j];

	for (j = 0; j < 16; j++)
		output[j] = clamp(tmp[j] >> SHIFT, 0, 127);
#endif
}

/* (AVX2) After affine_propagate16to32, the output is in the wrong order.
 * More precisely output[8-15] is swapped with output[16-23].
 */
static inline void affine_propagate16to32(int8_t *input, int8_t *output,
		const bias_t *biases, const weight_t *weights) {
#if defined(AVX2)
	__m256i out0 = ((__m256i *)biases)[0];
	__m256i out1 = ((__m256i *)biases)[1];
	__m256i out2 = ((__m256i *)biases)[2];
	__m256i out3 = ((__m256i *)biases)[3];
	__m256i weight0, weight1, in, signs;

	for (int i = 0; i < 16; i += 2) {
		weight0 = ((__m256i *)weights)[i];
		weight1 = ((__m256i *)weights)[i + 1];
		in = _mm256_set1_epi16(input[i] | (input[i + 1] << 8));

		weight0 = _mm256_maddubs_epi16(in, weight0);
		signs = _mm256_cmpgt_epi16(_mm256_setzero_si256(), weight0);
		out0 = _mm256_add_epi32(out0, _mm256_unpacklo_epi16(weight0, signs));
		out1 = _mm256_add_epi32(out1, _mm256_unpackhi_epi16(weight0, signs));

		weight1 = _mm256_maddubs_epi16(in, weight1);
		signs = _mm256_cmpgt_epi16(_mm256_setzero_si256(), weight1);
		out2 = _mm256_add_epi32(out2, _mm256_unpacklo_epi16(weight1, signs));
		out3 = _mm256_add_epi32(out3, _mm256_unpackhi_epi16(weight1, signs));
	}

	out0 = _mm256_srai_epi16(_mm256_packs_epi32(out0, out1), SHIFT);
	out2 = _mm256_srai_epi16(_mm256_packs_epi32(out2, out3), SHIFT);
	out0 = _mm256_packs_epi16(out0, out2);
	*(__m256i *)output = _mm256_max_epi8(out0, _mm256_setzero_si256());
#else
	int i, j;
	bias_t tmp[32];
	memcpy(tmp, biases, 32 * sizeof(biases[0]));

	for (i = 0; i < 16; i++)
		if (input[i])
			for (j = 0; j < 32; j++)
				tmp[j] += input[i] * weights[32 * i + j];

	for (j = 0; j < 32; j++)
		output[j] = clamp(tmp[j] >> SHIFT, 0, 127);
#endif
}

static inline int32_t output_layer(int8_t *input, const bias_t *biases, const weight_t *weights) {
#if defined(AVX2)
	__m256i in = *(__m256i *)input;
	__m256i weight = *(__m256i *)weights;
	__m256i out;
#if defined(VNNI)
	out = _mm256_dpbusd_epi32(_mm256_setzero_si256(), in, weight);
#else
	out = _mm256_maddubs_epi16(in, weight);
	out = _mm256_madd_epi16(out, _mm256_set1_epi16(1));
#endif
	__m128i sum = _mm_add_epi32(_mm256_castsi256_si128(out), _mm256_extracti128_si256(out, 1));
	sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
	return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
#else
	int32_t sum = biases[0];
	for (int i = 0; i < 32; i++)
		sum += weights[i] * input[i];
	return sum;
#endif
}

static inline void add_index(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2], int turn) {
	assert(nnue_init_done);
	const unsigned offset = K_HALF_DIMENSIONS * index;
#if defined(AVX2)
	for (int j = 0; j < K_HALF_DIMENSIONS; j += 16) {
		__m256i *a = (__m256i *)(accumulation[turn] + j);
		__m256i b = *(__m256i *)(ft_weights + offset + j);
		*a = _mm256_add_epi16(*a, b);
	}
#else
	for (int j = 0; j < K_HALF_DIMENSIONS; j++)
		accumulation[turn][j] += ft_weights[offset + j];
#endif
	psqtaccumulation[turn] += psqt_weights[index];
}

void add_index_slow(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2], int turn) {
	assert(nnue_init_done);
	const unsigned offset = K_HALF_DIMENSIONS * index;
	for (int j = 0; j < K_HALF_DIMENSIONS; j++)
		accumulation[turn][j] += ft_weights[offset + j];
	psqtaccumulation[turn] += psqt_weights[index];
}

static inline void remove_index(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2], int turn) {
	assert(nnue_init_done);
	const unsigned offset = K_HALF_DIMENSIONS * index;
#if defined(AVX2)
	for (int j = 0; j < K_HALF_DIMENSIONS; j += 16) {
		__m256i *a = (__m256i *)(accumulation[turn] + j);
		__m256i b = *(__m256i *)(ft_weights + offset + j);
		*a = _mm256_sub_epi16(*a, b);
	}
#else
	for (int j = 0; j < K_HALF_DIMENSIONS; j++)
		accumulation[turn][j] -= ft_weights[offset + j];
#endif
	psqtaccumulation[turn] -= psqt_weights[index];
}

void refresh_accumulator(struct position *pos, int turn) {
	if (!option_nnue)
		return;
	assert(nnue_init_done);
	memcpy(pos->accumulation[turn], ft_biases, K_HALF_DIMENSIONS * sizeof(*ft_biases));
	pos->psqtaccumulation[turn] = 0;
	int king_square = ctz(pos->piece[turn][KING]);
	king_square = orient(turn, king_square, king_square);
	uint64_t b;
	int square;
	int total = 0;
	for (int color = 0; color < 2; color++) {
		for (int piece = PAWN; piece < KING; piece++) {
			b = pos->piece[color][piece];
			while (b) {
				square = ctz(b);
				int index = make_index(turn, square, colored_piece(piece, color), king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
				total++;
				b = clear_ls1b(b);
			}
		}
	}
	int index = make_index(turn, ctz(pos->piece[other_color(turn)][KING]), colored_piece(KING, other_color(turn)), king_square);
	add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	total++;
}

/* m cannot be a king move of the side <turn>. */
void do_update_accumulator(struct position *pos, move_t *move, int turn) {
	int source_square = move_from(move);
	int target_square = move_to(move);
	int king_square = ctz(pos->piece[turn][KING]);
	king_square = orient(turn, king_square, king_square);
	unsigned index;
	unsigned indext;

	index = make_index(turn, source_square, pos->mailbox[target_square], king_square);
	remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);

	index = make_index(turn, target_square, pos->mailbox[target_square], king_square);
	add_index(index, pos->accumulation, pos->psqtaccumulation, turn);

	if (move_capture(move) && move_flag(move) != MOVE_EN_PASSANT) {
		index = make_index(turn, target_square, colored_piece(move_capture(move), pos->turn), king_square);
		remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	}

	if (move_flag(move) == MOVE_EN_PASSANT) {
		if (pos->turn)
			index = make_index(turn, target_square + 8, WHITE_PAWN, king_square);
		else
			index = make_index(turn, target_square - 8, BLACK_PAWN, king_square);
		remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	}
	else if (move_flag(move) == MOVE_PROMOTION) {
		if (pos->turn) {
			index = make_index(turn, source_square, pos->mailbox[target_square], king_square);
			indext = make_index(turn, source_square, BLACK_PAWN, king_square);
		}
		else {
			index = make_index(turn, source_square, pos->mailbox[target_square], king_square);
			indext = make_index(turn, source_square, WHITE_PAWN, king_square);
		}
		add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
		remove_index(indext, pos->accumulation, pos->psqtaccumulation, turn);
	}
	else if (move_flag(move) == MOVE_CASTLE) {
		switch (target_square) {
		case g1:
			index = make_index(BLACK, h1, WHITE_ROOK, king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, BLACK);

			index = make_index(BLACK, f1, WHITE_ROOK, king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, BLACK);
			break;
		case c1:
			index = make_index(BLACK, a1, WHITE_ROOK, king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, BLACK);

			index = make_index(BLACK, d1, WHITE_ROOK, king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, BLACK);
			break;
		case g8:
			index = make_index(WHITE, h8, BLACK_ROOK, king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, WHITE);

			index = make_index(WHITE, f8, BLACK_ROOK, king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, WHITE);
			break;
		case c8:
			index = make_index(WHITE, a8, BLACK_ROOK, king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, WHITE);

			index = make_index(WHITE, d8, BLACK_ROOK, king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, WHITE);
			break;
		}
	}
}

void undo_update_accumulator(struct position *pos, move_t *move, int turn) {
	int source_square = move_from(move);
	int target_square = move_to(move);
	int king_square = ctz(pos->piece[turn][KING]);
	king_square = orient(turn, king_square, king_square);
	unsigned index;
	unsigned indext;

	index = make_index(turn, target_square, pos->mailbox[source_square], king_square);
	remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);

	index = make_index(turn, source_square, pos->mailbox[source_square], king_square);
	add_index(index, pos->accumulation, pos->psqtaccumulation, turn);

	if (move_capture(move) && move_flag(move) != MOVE_EN_PASSANT) {
		index = make_index(turn, target_square, colored_piece(move_capture(move), other_color(pos->turn)), king_square);
		add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	}

	if (move_flag(move) == MOVE_EN_PASSANT) {
		if (pos->turn)
			index = make_index(turn, target_square - 8, BLACK_PAWN, king_square);
		else
			index = make_index(turn, target_square + 8, WHITE_PAWN, king_square);
		add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	}
	else if (move_flag(move) == MOVE_PROMOTION) {
		if (pos->turn) {
			index = make_index(turn, target_square, move_promote(move) + 2, king_square);
			indext = make_index(turn, target_square, WHITE_PAWN, king_square);
		}
		else {
			index = make_index(turn, target_square, move_promote(move) + 8, king_square);
			indext = make_index(turn, target_square, BLACK_PAWN, king_square);
		}
		remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);
		add_index(indext, pos->accumulation, pos->psqtaccumulation, turn);
	}
	else if (move_flag(move) == MOVE_CASTLE) {
		switch (target_square) {
		case g1:
			index = make_index(BLACK, h1, WHITE_ROOK, king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, BLACK);

			index = make_index(BLACK, f1, WHITE_ROOK, king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, BLACK);
			break;
		case c1:
			index = make_index(BLACK, a1, WHITE_ROOK, king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, BLACK);

			index = make_index(BLACK, d1, WHITE_ROOK, king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, BLACK);
			break;
		case g8:
			index = make_index(WHITE, h8, BLACK_ROOK, king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, WHITE);

			index = make_index(WHITE, f8, BLACK_ROOK, king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, WHITE);
			break;
		case c8:
			index = make_index(WHITE, a8, BLACK_ROOK, king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, WHITE);

			index = make_index(WHITE, d8, BLACK_ROOK, king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, WHITE);
			break;
		}
	}
}

/* Should be called after do_move. */
void do_accumulator(struct position *pos, move_t *move) {
	assert(*move);
	assert(!pos->mailbox[move_from(move)]);
	assert(pos->mailbox[move_to(move)]);
	if (!option_nnue)
		return;
	assert(nnue_init_done);
	int target_square = move_to(move);
	if (uncolored_piece(pos->mailbox[target_square]) == KING)
		refresh_accumulator(pos, other_color(pos->turn));
	else
		do_update_accumulator(pos, move, other_color(pos->turn));
	do_update_accumulator(pos, move, pos->turn);
#ifndef NDEBUG
	int16_t accumulation[2][K_HALF_DIMENSIONS];
	memcpy(accumulation, pos->accumulation, 2 * K_HALF_DIMENSIONS * sizeof(int16_t));

	int32_t psqtaccumulation[2][8];
	memcpy(psqtaccumulation, pos->psqtaccumulation, 2 * 8 * sizeof(int16_t));

	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < K_HALF_DIMENSIONS; j++) {
			if (accumulation[i][j] != pos->accumulation[i][j]) {
				printf("ERROR DO ACCUMULATION[%d][%d]\n", i, j);
				print_position(pos);
				print_move(move);
				printf("\n");
				exit(1);
			}
		}
	}


	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 8; j++) {
			if (accumulation[i][j] != pos->accumulation[i][j]) {
				printf("ERROR DO PSQT[%d][%d]\n", i, j);
				print_position(pos);
				print_move(move);
				printf("\n");
				exit(1);
			}
		}
	}
#endif
}

/* Should be called after undo_move. */
void undo_accumulator(struct position *pos, move_t *move) {
	assert(*move);
	assert(pos->mailbox[move_from(move)]);
	if (!option_nnue)
		return;
	assert(nnue_init_done);
	int source_square = move_from(move);
	if (uncolored_piece(pos->mailbox[source_square]) == KING)
		refresh_accumulator(pos, pos->turn);
	else
		undo_update_accumulator(pos, move, pos->turn);
	undo_update_accumulator(pos, move, other_color(pos->turn));
#ifndef NDEBUG
	int16_t accumulation[2][K_HALF_DIMENSIONS];
	memcpy(accumulation, pos->accumulation, 2 * K_HALF_DIMENSIONS * sizeof(int16_t));

	int32_t psqtaccumulation[2][8];
	memcpy(psqtaccumulation, pos->psqtaccumulation, 2 * 8 * sizeof(int16_t));

	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < K_HALF_DIMENSIONS; j++) {
			if (accumulation[i][j] != pos->accumulation[i][j]) {
				printf("ERROR UNDO ACCUMULATION[%d][%d]\n", i, j);
				print_position(pos);
				print_move(move);
				printf("\n");
				exit(1);
			}
		}
	}


	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 8; j++) {
			if (accumulation[i][j] != pos->accumulation[i][j]) {
				printf("ERROR UNDO PSQT[%d][%d]\n", i, j);
				print_position(pos);
				print_move(move);
				printf("\n");
				exit(1);
			}
		}
	}
#endif
}

int32_t evaluate_accumulator(const struct position *pos) {
	assert(nnue_init_done);
	struct data buf;
	transform(pos, pos->accumulation, buf.ft_out);
	affine_propagate1024to16(buf.ft_out, buf.hidden1_out, hidden1_biases, hidden1_weights);
	affine_propagate16to32(buf.hidden1_out, buf.hidden2_out, hidden2_biases, hidden2_weights);
	int32_t psqt = (pos->psqtaccumulation[pos->turn] - pos->psqtaccumulation[other_color(pos->turn)]) / 2;
	return (output_layer(buf.hidden2_out, output_biases, output_weights)) / FV_SCALE + psqt;
}

int32_t evaluate_nnue(struct position *pos) {
	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);
	return evaluate_accumulator(pos);
}

#if defined(AVX2)
void swap_cols(weight_t *weights, int rows, int col1, int col2) {
	for (int row = 0; row < rows; row++) {
		weight_t t = weights[rows * col1 + row];
		weights[rows * col1 + row] = weights[rows * col2 + row];
		weights[rows * col2 + row] = t;
	}
}

void swap4_cols(weight_t *weights, int rows, int col1, int col2) {
	for (int col = 4 * col1; col < 4 * (col1 + 1); col++)
		swap_cols(weights, rows, col, col + 4 * (col2 - col1));
}

void permute_cols(weight_t *weights, int rows, int col1, int col2) {
	weight_t *tmp = malloc(2 * rows * sizeof(*tmp));

	for (int row = 0; row < rows; row++) {
		tmp[2 * row]     = weights[rows * col1 + row];
		tmp[2 * row + 1] = weights[rows * col2 + row];
	}

	memcpy(&weights[rows * col1], &tmp[0], rows * sizeof(*tmp));
	memcpy(&weights[rows * col2], &tmp[rows], rows * sizeof(*tmp));

	free(tmp);
}

void swap_biases(bias_t *biases, int row1, int row2) {
	bias_t t = biases[row1];
	biases[row1] = biases[row2];
	biases[row2] = t;
}

void swap4_biases(bias_t *biases, int row1, int row2) {
	for (int row = 4 * row1; row < 4 * (row1 + 1); row++)
		swap_biases(biases, row, row + 4 * (row2 - row1));
}
#endif

void permute_weights(void) {
#if defined(AVX2)
	for (int col = 0; col < FT_OUT_DIMS; col++)
		if (8 <= col % 32 && col % 32 < 16)
			swap_cols(hidden1_weights, 16, col, col + 8);

	for (int col = 8; col < 16; col++)
		swap_cols(output_weights, 1, col, col + 8);

	for (int col = 0; col < FT_OUT_DIMS; col += 2)
		permute_cols(hidden1_weights, 16, col, col + 1);

	for (int col = 0; col < 16; col += 2)
		permute_cols(hidden2_weights, 32, col, col + 1);

	swap4_biases(hidden1_biases, 1, 2);

	swap4_biases(hidden2_biases, 1, 2);
	swap4_biases(hidden2_biases, 5, 6);
#endif
}

void nnue_init(void) {
	builtin_nnue();
	permute_weights();
#ifndef NDEBUG
	nnue_init_done = 1;
#endif
}

void file_nnue(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "error: failed to open file %s\n", path);
		goto error;
	}

	if (nnuefile(f, file_ft_weights, file_ft_biases,
			file_psqt_weights, file_hidden1_weights,
			file_hidden1_biases, file_hidden2_weights,
			file_hidden2_biases, file_output_weights,
			file_output_biases) || fclose(f))
		goto error;

	ft_weights = file_ft_weights;
	ft_biases = file_ft_biases;

	psqt_weights = file_psqt_weights;

	hidden1_weights = file_hidden1_weights;
	hidden1_biases = file_hidden1_biases;

	hidden2_weights = file_hidden2_weights;
	hidden2_biases = file_hidden2_biases;

	output_weights = file_output_weights;
	output_biases = file_output_biases;

	builtin = 0;
	strncpy(pathnnue, path, sizeof(pathnnue));
	pathnnue[sizeof(pathnnue) - 1] = '\0';

	permute_weights();

#ifndef NDEBUG
	nnue_init_done = 1;
#endif
	return;
error:
	if (f) {
		fclose(f);
		fprintf(stderr, "error: failed to read file %s\n", path);
	}
	builtin_nnue();
}

void print_nnue_info(void) {
	printf("info string evaluation ");
	if (option_nnue) {
		if (option_pure_nnue)
			printf("pure ");
		printf("nnue ");
		if (builtin)
			printf("built in");
		else
			printf("file <%s>", pathnnue);
	}
	else
		printf("classical");
	printf("\n");
}

void builtin_nnue(void) {
	ft_weights = builtin_ft_weights;
	ft_biases = builtin_ft_biases;

	psqt_weights = builtin_psqt_weights;

	hidden1_weights = builtin_hidden1_weights;
	hidden1_biases = builtin_hidden1_biases;

	hidden2_weights = builtin_hidden2_weights;
	hidden2_biases = builtin_hidden2_biases;

	output_weights = builtin_output_weights;
	output_biases = builtin_output_biases;

	builtin = 1;

#ifndef NDEBUG
	nnue_init_done = 1;
#endif
}

const char *simd =
#if defined(AVX2)
"avx2"
#elif defined(VNNI)
"vnni"
#elif defined(SSE4)
"sse4"
#elif defined(SSE2)
"sse2"
#else
"none"
#endif
;
