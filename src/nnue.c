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

#ifdef AVX2
#include <immintrin.h>
#elif SSE4
#include <smmintrin.h>
#elif SSE2
#include <emmintrin.h>
#endif

#if defined(AVX2)
#define SSE4
#define SSE2
#elif defined(SSE4)
#define SSE2
#endif

#if defined(AVX2)

#define VECTOR
#define SIMD_WIDTH 256
#define vec_add(a, b) _mm256_add_epi16(a, b)
#define vec_sub(a, b) _mm256_sub_epi16(a, b)
#define vec_packs(a, b) _mm256_packs_epi16(a, b)
#define vec_sr(a, b) _mm256_srai_epi16(a, b)
#define vec_max(a, b) _mm256_max_epi8(a, b)
#define vec_zero() _mm256_setzero_si256()
typedef __m256i vec_t;

#elif defined(SSE2)

#define VECTOR
#define SIMD_WIDTH 128
#define vec_add(a, b) _mm_add_epi16(a, b)
#define vec_sub(a, b) _mm_sub_epi16(a, b)
#define vec_packs(a, b) _mm_packs_epi16(a, b)
#define vec_sr(a, b) _mm_srai_epi16(a, b)

#if defined(SSE4)
#define vec_max(a, b) _mm_max_epi8(a, b)
#endif

#define vec_zero() _mm_setzero_si128()
typedef __m128i vec_t;

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

extern alignas(64) ft_weight_t builtin_psqt_weights[8 * FT_IN_DIMS];

extern alignas(64) weight_t builtin_hidden1_weights[8][16 * FT_OUT_DIMS];
extern alignas(64) bias_t builtin_hidden1_biases[8][16];

extern alignas(64) weight_t builtin_hidden2_weights[8][32 * 16];
extern alignas(64) bias_t builtin_hidden2_biases[8][32];

extern alignas(64) weight_t builtin_output_weights[8][1 * 32];
extern alignas(64) bias_t builtin_output_biases[8][1];



alignas(64) ft_weight_t file_ft_weights[K_HALF_DIMENSIONS * FT_IN_DIMS];
alignas(64) ft_bias_t file_ft_biases[K_HALF_DIMENSIONS];

alignas(64) ft_weight_t file_psqt_weights[8 * FT_IN_DIMS];

alignas(64) weight_t file_hidden1_weights[8][16 * FT_OUT_DIMS];
alignas(64) bias_t file_hidden1_biases[8][16];

alignas(64) weight_t file_hidden2_weights[8][32 * 16];
alignas(64) bias_t file_hidden2_biases[8][32];

alignas(64) weight_t file_output_weights[8][1 * 32];
alignas(64) bias_t file_output_biases[8][1];



const ft_weight_t *ft_weights;
const ft_bias_t *ft_biases;

ft_weight_t *psqt_weights;

weight_t (*hidden1_weights)[16 * FT_OUT_DIMS];
bias_t (*hidden1_biases)[16];

weight_t (*hidden2_weights)[32 * 16];
bias_t (*hidden2_biases)[32];

weight_t (*output_weights)[1 * 32];
bias_t (*output_biases)[1];

static inline void transform(const struct position *pos, const int16_t accumulation[2][K_HALF_DIMENSIONS], int8_t *output) {
#if defined(AVX2) || defined(SSE4)
	const vec_t zero = vec_zero();
	const int perspective[2] = { pos->turn, other_color(pos->turn) };
	for (int j = 0; j < 2; j++) {
		vec_t *out = (vec_t *)(output + j * K_HALF_DIMENSIONS);
		for (int i = 0; i < 8 * K_HALF_DIMENSIONS / SIMD_WIDTH; i++) {
			vec_t p0 = ((vec_t *)accumulation[perspective[j]])[2 * i];
			vec_t p1 = ((vec_t *)accumulation[perspective[j]])[2 * i + 1];
			out[i] = vec_max(vec_packs(vec_sr(p0, FT_SHIFT), vec_sr(p1, FT_SHIFT)), zero);
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

static inline void affine_propagate1(int8_t *input, int8_t *output,
		const bias_t *biases, const weight_t *weights) {
#if defined(AVX2)
	const __m256i zero = _mm256_setzero_si256();
	__m256i out0 = ((__m256i *)biases)[0];
	__m256i out1 = ((__m256i *)biases)[1];
	__m256i out2 = ((__m256i *)biases)[2];
	__m256i out3 = ((__m256i *)biases)[3];
	__m256i first, second;

	for (int i = 0; i < in_dim; i += 2) {
		first = ((__m256i *)weights)[i];
		second = ((__m256i *)weights)[i + 1];
		uint16_t factor = input[i];
		factor |= input[i + 1] << 8;

		__m256i mul = _mm256_set1_epi16(factor), prod, signs;
		prod = _mm256_maddubs_epi16(mul, _mm256_unpacklo_epi8(first, second));
		signs = _mm256_cmpgt_epi16(zero, prod);
		out0 = _mm256_add_epi32(out0, _mm256_unpacklo_epi16(prod, signs));
		out1 = _mm256_add_epi32(out1, _mm256_unpackhi_epi16(prod, signs));
		prod = _mm256_maddubs_epi16(mul, _mm256_unpackhi_epi8(first, second));
		signs = _mm256_cmpgt_epi16(zero, prod);
		out2 = _mm256_add_epi32(out2, _mm256_unpacklo_epi16(prod, signs));
		out3 = _mm256_add_epi32(out3, _mm256_unpackhi_epi16(prod, signs));
	}

	out0 = _mm256_srai_epi16(_mm256_packs_epi32(out0, out1), SHIFT);
	out2 = _mm256_srai_epi16(_mm256_packs_epi32(out2, out3), SHIFT);

	*(__m256i *)output = _mm256_max_epi8(_mm256_packs_epi16(out0, out2), zero);
#elif defined(SSE4)
	const __m128i zero[2] = { 0 };
	__m128i out0 = ((__m128i *)biases)[0];
	__m128i out1 = ((__m128i *)biases)[1];
	__m128i out2 = ((__m128i *)biases)[2];
	__m128i out3 = ((__m128i *)biases)[3];
	__m128i out4 = ((__m128i *)biases)[4];
	__m128i out5 = ((__m128i *)biases)[5];
	__m128i out6 = ((__m128i *)biases)[6];
	__m128i out7 = ((__m128i *)biases)[7];
	const __m128i *first, *second;
	for (int i = 0; i < in_dim; i += 2) {
		first = &((__m128i *)weights)[2 * i];
		second = &((__m128i *)weights)[2 * (i + 1)];
		uint16_t factor = input[i];
		factor |= input[i + 1] << 8;

		__m128i mul = _mm_set1_epi16(factor), prod, signs;
		prod = _mm_maddubs_epi16(mul, _mm_unpacklo_epi8(first[0], second[0]));
		signs = _mm_cmpgt_epi16(zero[0], prod);
		out0 = _mm_add_epi32(out0, _mm_unpacklo_epi16(prod, signs));
		out1 = _mm_add_epi32(out1, _mm_unpackhi_epi16(prod, signs));
		prod = _mm_maddubs_epi16(mul, _mm_unpackhi_epi8(first[0], second[0]));
		signs = _mm_cmpgt_epi16(zero[0], prod);
		out2 = _mm_add_epi32(out2, _mm_unpacklo_epi16(prod, signs));
		out3 = _mm_add_epi32(out3, _mm_unpackhi_epi16(prod, signs));
		prod = _mm_maddubs_epi16(mul, _mm_unpacklo_epi8(first[1], second[1]));
		signs = _mm_cmpgt_epi16(zero[0], prod);
		out4 = _mm_add_epi32(out4, _mm_unpacklo_epi16(prod, signs));
		out5 = _mm_add_epi32(out5, _mm_unpackhi_epi16(prod, signs));
		prod = _mm_maddubs_epi16(mul, _mm_unpackhi_epi8(first[1], second[1]));
		signs = _mm_cmpgt_epi16(zero[0], prod);
		out6 = _mm_add_epi32(out6, _mm_unpacklo_epi16(prod, signs));
		out7 = _mm_add_epi32(out7, _mm_unpackhi_epi16(prod, signs));
	}
	
	out0 = _mm_srai_epi16(_mm_packs_epi32(out0, out1), SHIFT);
	out2 = _mm_srai_epi16(_mm_packs_epi32(out2, out3), SHIFT);
	out4 = _mm_srai_epi16(_mm_packs_epi32(out4, out5), SHIFT);
	out6 = _mm_srai_epi16(_mm_packs_epi32(out6, out7), SHIFT);

	((__m128i *)output)[0] = _mm_max_epi8(_mm_packs_epi16(out0, out2), zero[0]);
	((__m128i *)output)[1] = _mm_max_epi8(_mm_packs_epi16(out4, out6), zero[0]);
#else
	int i, j;
	bias_t tmp[16];
	memcpy(tmp, biases, 16 * sizeof(biases[0]));

	for (i = 0; i < FT_OUT_DIMS; i++)
		if (input[i])
			for (j = 0; j < 16; j++)
				tmp[j] += input[i] * weights[16 * i + j];

	for (i = 0; i < 16; i++)
		output[i] = clamp(tmp[i] >> SHIFT, 0, 127);
#endif
}

static inline void affine_propagate2(int8_t *input, int8_t *output,
		const bias_t *biases, const weight_t *weights) {
	int i, j;
	bias_t tmp[32];
	memcpy(tmp, biases, 32 * sizeof(biases[0]));

	for (i = 0; i < 16; i++)
		if (input[i])
			for (j = 0; j < 32; j++)
				tmp[j] += input[i] * weights[32 * i + j];

	for (i = 0; i < 32; i++)
		output[i] = clamp(tmp[i] >> SHIFT, 0, 127);
}
static inline int32_t output_layer(int8_t *input, const bias_t *biases, const weight_t *weights) {
#if defined(AVX2)
	__m256i *a = (__m256i *)input;
	__m256i *b = (__m256i *)weights;
	/* Gives us 16 16bit integers. */
	a[0] = _mm256_maddubs_epi16(a[0], b[0]);
	/* Gives us 8 32bit integers. */
	a[0] = _mm256_madd_epi16(a[0], _mm256_set1_epi16(1));
	/* Sums the first 4 32bit integers with the last 4 32bit integers
	 * giving us 4 32bit integers.
	 */
	__m128i sum = _mm_add_epi32(_mm256_castsi256_si128(a[0]), _mm256_extracti128_si256(a[0], 1));
	/* Gives us 4 32bit integers, integer 0 and 2 are identical.
	 * So are 1 and 3. We want to sum 0 and 1 or 2 and 3.
	 */
	sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
	/* Add integer 0 and 1. */
	return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
#elif defined(SSE4)
	__m128i *a = (__m128i *)input;
	__m128i *b = (__m128i *)weights;
	a[0] = _mm_maddubs_epi16(a[0], b[0]);
	a[1] = _mm_maddubs_epi16(a[1], b[1]);
	__m128i sum = _mm_add_epi16(a[0], a[1]);
	sum = _mm_madd_epi16(sum, _mm_set1_epi16(1));
	sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
	return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
#else
	int32_t sum = biases[0];
	for (int i = 0; i < 32; i++)
		sum += weights[i] * input[i];
	return sum;
#endif
}

static inline void add_index(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2][8], int turn) {
	const unsigned offset = K_HALF_DIMENSIONS * index;
#if defined(VECTOR)
	for (int j = 0; j < K_HALF_DIMENSIONS; j += SIMD_WIDTH / 16) {
		vec_t *a = (vec_t *)(accumulation[turn] + j);
		vec_t *b = (vec_t *)(ft_weights + offset + j);
		a[0] = vec_add(a[0], b[0]);
	}
#else
	for (int j = 0; j < K_HALF_DIMENSIONS; j++)
		accumulation[turn][j] += ft_weights[offset + j];
#endif
	for (int j = 0; j < 8; j++)
		psqtaccumulation[turn][j] += psqt_weights[8 * index + j];
}

void add_index_slow(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2][8], int turn) {
	const unsigned offset = K_HALF_DIMENSIONS * index;
#if defined(VECTOR)
	for (int j = 0; j < K_HALF_DIMENSIONS; j += SIMD_WIDTH / 16) {
		vec_t *a = (vec_t *)(accumulation[turn] + j);
		vec_t *b = (vec_t *)(ft_weights + offset + j);
		a[0] = vec_add(a[0], b[0]);
	}
#else
	for (int j = 0; j < K_HALF_DIMENSIONS; j++)
		accumulation[turn][j] += ft_weights[offset + j];
#endif
	for (int j = 0; j < 8; j++)
		psqtaccumulation[turn][j] += psqt_weights[8 * index + j];
}

static inline void remove_index(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2][8], int turn) {
	const unsigned offset = K_HALF_DIMENSIONS * index;
#if defined(VECTOR)
	for (int j = 0; j < K_HALF_DIMENSIONS; j += SIMD_WIDTH / 16) {
		vec_t *a = (vec_t *)(accumulation[turn] + j);
		vec_t *b = (vec_t *)(ft_weights + offset + j);
		a[0] = vec_sub(a[0], b[0]);
	}
#else
	for (int j = 0; j < K_HALF_DIMENSIONS; j++)
		accumulation[turn][j] -= ft_weights[offset + j];
#endif
	for (int j = 0; j < 8; j++)
		psqtaccumulation[turn][j] -= psqt_weights[8 * index + j];
}

void refresh_accumulator(struct position *pos, int turn) {
	if (!option_nnue)
		return;
	memcpy(pos->accumulation[turn], ft_biases, K_HALF_DIMENSIONS * sizeof(*ft_biases));
	memset(pos->psqtaccumulation[turn], 0, 8 * sizeof(*pos->psqtaccumulation[turn]));
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

/* m cannot be a king move. */
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
	int bucket = get_bucket(pos);

	struct data buf;
	transform(pos, pos->accumulation, buf.ft_out);
	affine_propagate1(buf.ft_out, buf.hidden1_out, hidden1_biases[bucket], hidden1_weights[bucket]);
	affine_propagate2(buf.hidden1_out, buf.hidden2_out, hidden2_biases[bucket], hidden2_weights[bucket]);
	int32_t psqt = (pos->psqtaccumulation[pos->turn][bucket] - pos->psqtaccumulation[other_color(pos->turn)][bucket]) / 2;
	return (output_layer(buf.hidden2_out, output_biases[bucket], output_weights[bucket])) / FV_SCALE + psqt;
}

int32_t evaluate_nnue(struct position *pos) {
	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);
	return evaluate_accumulator(pos);
}

void permute_biases(bias_t *biases, int dims) {
#if defined(AVX2)
	__m128i *b = (__m128i *)biases;
	__m128i tmp[8];
	tmp[0] = b[0];
	tmp[1] = b[4];
	tmp[2] = b[1];
	tmp[3] = b[5];
	tmp[4] = b[2];
	tmp[5] = b[6];
	tmp[6] = b[3];
	tmp[7] = b[7];
	memcpy(b, tmp, 8 * sizeof(__m128i));
#else
	UNUSED(biases);
#endif
}

void permute_weights(weight_t *weights) {
#if defined(AVX2)
	__m256i *w = (__m256i *)weights;
	__m256i tmp[FT_OUT_DIMS];
	for (int i = 0; i < FT_OUT_DIMS; i++) {
		int j = i % 32;
		if (j < 8) {
			tmp[i] = w[i];
		}
		else if (j < 16) {
			tmp[i] = w[i + 8];
		}
		else if (j < 24) {
			tmp[i] = w[i - 8];
		}
		else {
			tmp[i] = w[i];
		}
	}
	memcpy(w, tmp, sizeof(tmp));
#else
	UNUSED(weights);
#endif
}

void nnue_init(void) {
	//permute_biases(builtin_hidden1_biases);
	//permute_biases(builtin_hidden2_biases);
	//permute_weights(builtin_hidden1_weights);
	builtin_nnue();
}

int read_hidden_weights(weight_t *w, int dims, FILE *f) {
	for (int i = 0; i < 32; i++)
		for (int j = 0; j < dims; j++)
			if (read_uintx(f, &w[j * 32 + i], sizeof(*w)))
				return 1;
	return 0;
}

int read_output_weights(weight_t *w, FILE *f) {
	for (int i = 0; i < 32; i++)
		if (read_uintx(f, &w[i], sizeof(*w)))
			return 1;
	return 0;
}

void file_nnue(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "error: failed to open file %s\n", path);
		goto error;
	}
	int i, j, k;
	for (i = 0; i < K_HALF_DIMENSIONS; i++) {
		if (read_uintx(f, &file_ft_biases[i], sizeof(*file_ft_biases)))
			goto error;
	}
	for (i = 0; i < 8; i++)
		read_uintx(f, NULL, sizeof(*file_ft_biases));
	for (i = j = 0; i < (K_HALF_DIMENSIONS + 8) * FT_IN_DIMS; i++) {
		if (i % (K_HALF_DIMENSIONS + 8) >= K_HALF_DIMENSIONS) {
			if (read_uintx(f, &file_psqt_weights[j++], sizeof(*file_psqt_weights)))
				goto error;
		}
		else {
			if (read_uintx(f, &file_ft_weights[i - j], sizeof(*file_ft_weights)))
				goto error;
		}
	}

	for (j = 0; j < 8; j++)
		for (i = 0; i < 16; i++)
			if (read_uintx(f, &file_hidden1_biases[j][i], sizeof(**file_hidden1_biases)))
				goto error;

	for (j = 0; j < 8; j++)
		for (i = 0; i < 16; i++)
			for (k = 0; k < FT_OUT_DIMS; k++)
				if (read_uintx(f, &file_hidden1_weights[j][16 * k + i], sizeof(**file_hidden1_weights)))
					goto error;

	for (j = 0; j < 8; j++)
		for (i = 0; i < 32; i++)
			if (read_uintx(f, &file_hidden2_biases[j][i], sizeof(**file_hidden2_biases)))
				goto error;
	for (j = 0; j < 8; j++)
		for (i = 0; i < 32; i++)
			for (k = 0; k < 16; k++)
				if (read_uintx(f, &file_hidden2_weights[j][32 * k + i], sizeof(**file_hidden2_weights)))
					goto error;

	for (j = 0; j < 8; j++)
		if (read_uintx(f, file_output_biases[j], sizeof(**file_output_biases)))
			goto error;
	for (j = 0; j < 8; j++)
		for (i = 0; i < 32; i++)
			if (read_uintx(f, &file_output_weights[j][i], sizeof(**file_output_weights)))
				goto error;


	//permute_biases(file_hidden1_biases);
	//permute_biases(file_hidden2_biases);
	//permute_weights(file_hidden1_weights);
	long current = ftell(f);
	fseek(f, 0, SEEK_END);
	printf("BYTES LEFT: %ld\n", ftell(f) - current);
	if (fclose(f))
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
}
