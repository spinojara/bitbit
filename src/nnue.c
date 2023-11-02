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
#include <stdalign.h>
#include <assert.h>

#include "bitboard.h"
#include "util.h"
#include "evaluate.h"
#include "move.h"
#include "position.h"
#include "option.h"

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

struct data {
	alignas(64) int8_t ft_out[FT_OUT_DIMS];
	alignas(64) int8_t hidden1_out[32];
	alignas(64) int8_t hidden2_out[32];
};

extern alignas(64) ft_weight_t ft_weights[K_HALF_DIMENSIONS * FT_IN_DIMS];
extern alignas(64) ft_bias_t ft_biases[K_HALF_DIMENSIONS];

extern alignas(64) ft_weight_t psqt_weights[1 * FT_IN_DIMS];

extern alignas(64) weight_t hidden1_weights[32 * FT_OUT_DIMS];
extern alignas(64) bias_t hidden1_biases[32];

extern alignas(64) weight_t hidden2_weights[32 * 32];
extern alignas(64) bias_t hidden2_biases[32];

extern alignas(64) weight_t output_weights[1 * 32];
extern alignas(64) bias_t output_biases[1];

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
		output[i] = CLAMP(sum >> FT_SHIFT, 0, 127);
		sum = accumulation[other_color(pos->turn)][i];
		output[K_HALF_DIMENSIONS + i] = CLAMP(sum >> FT_SHIFT, 0, 127);
	}
#endif
}

static inline void affine_propagate(int8_t *input, int8_t *output, int in_dim,
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
	bias_t tmp[32];
	memcpy(tmp, biases, 32 * sizeof(biases[0]));

	for (i = 0; i < in_dim; i++)
		if (input[i])
			for (j = 0; j < 32; j++)
				tmp[j] += input[i] * weights[32 * i + j];

	for (i = 0; i < 32; i++)
		output[i] = CLAMP(tmp[i] >> SHIFT, 0, 127);
#endif
}

static inline int32_t output_layer(int8_t *input, const bias_t *biases, const weight_t *weights) {
#if defined(AVX2)
	__m256i *a = (__m256i *)input;
	__m256i *b = (__m256i *)weights;
	/* gives us 16 16bit integers */
	a[0] = _mm256_maddubs_epi16(a[0], b[0]);
	/* gives us 8 32bit integers */
	a[0] = _mm256_madd_epi16(a[0], _mm256_set1_epi16(1));
	/* sums the first 4 32bit integers with the last 4 32bit integers
	 * giving us 4 32bit integers
	 */
	__m128i sum = _mm_add_epi32(_mm256_castsi256_si128(a[0]), _mm256_extracti128_si256(a[0], 1));
	/* gives us 4 32bit integers, integer 0 and 2 are identical.
	 * So are 1 and 3. We want to sum 0 and 1 or 2 and 3.
	 */
	sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
	/* add integer 0 and 1 */
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

static inline void add_index(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2], int turn) {
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
	psqtaccumulation[turn] += psqt_weights[index];
}

void add_index_slow(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2], int turn) {
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
	psqtaccumulation[turn] += psqt_weights[index];
}

static inline void remove_index(unsigned index, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2], int turn) {
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
	psqtaccumulation[turn] -= psqt_weights[index];
}

void refresh_accumulator(struct position *pos, int turn) {
	if (!option_nnue)
		return;
	memcpy(pos->accumulation[turn], ft_biases, K_HALF_DIMENSIONS * sizeof(*ft_biases));
	pos->psqtaccumulation[turn] = 0;
	int king_square = ctz(pos->piece[turn][king]);
	king_square = orient_horizontal(turn, king_square);
	uint64_t b;
	int square;
	for (int color = 0; color < 2; color++) {
		for (int piece = 1; piece < 6; piece++) {
			b = pos->piece[color][piece];
			while (b) {
				square = ctz(b);
				int index = make_index(turn, square, colored_piece(piece, color), king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
				b = clear_ls1b(b);
			}
		}
	}
}

/* m cannot be a king move_t */
void do_update_accumulator(struct position *pos, move_t *m, int turn) {
	int source_square = move_from(m);
	int target_square = move_to(m);
	int king_square = orient_horizontal(turn, ctz(pos->piece[turn][king]));
	unsigned index;
	unsigned indext;

	index = make_index(turn, source_square, pos->mailbox[target_square], king_square);
	remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);

	index = make_index(turn, target_square, pos->mailbox[target_square], king_square);
	add_index(index, pos->accumulation, pos->psqtaccumulation, turn);

	if (move_capture(m)) {
		index = make_index(turn, target_square, colored_piece(move_capture(m), pos->turn), king_square);
		remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	}

	if (move_flag(m) == 1) {
		if (pos->turn)
			index = make_index(turn, target_square + 8, white_pawn, king_square);
		else
			index = make_index(turn, target_square - 8, black_pawn, king_square);
		remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	}
	else if (move_flag(m) == 2) {
		if (pos->turn) {
			index = make_index(turn, source_square, pos->mailbox[target_square], king_square);
			indext = make_index(turn, source_square, black_pawn, king_square);
		}
		else {
			index = make_index(turn, source_square, pos->mailbox[target_square], king_square);
			indext = make_index(turn, source_square, white_pawn, king_square);
		}
		add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
		remove_index(indext, pos->accumulation, pos->psqtaccumulation, turn);
	}
}

void undo_update_accumulator(struct position *pos, move_t *m, int turn) {
	int source_square = move_from(m);
	int target_square = move_to(m);
	int king_square = orient_horizontal(turn, ctz(pos->piece[turn][king]));
	unsigned index;
	unsigned indext;
	
	index = make_index(turn, target_square, pos->mailbox[source_square], king_square);
	remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);

	index = make_index(turn, source_square, pos->mailbox[source_square], king_square);
	add_index(index, pos->accumulation, pos->psqtaccumulation, turn);

	if (move_capture(m)) {
		index = make_index(turn, target_square, colored_piece(move_capture(m), other_color(pos->turn)), king_square);
		add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	}

	if (move_flag(m) == 1) {
		if (pos->turn)
			index = make_index(turn, target_square - 8, black_pawn, king_square);
		else
			index = make_index(turn, target_square + 8, white_pawn, king_square);
		add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
	}
	else if (move_flag(m) == 2) {
		if (pos->turn) {
			index = make_index(turn, target_square, move_promote(m) + 2, king_square);
			indext = make_index(turn, target_square, white_pawn, king_square);
		}
		else {
			index = make_index(turn, target_square, move_promote(m) + 8, king_square);
			indext = make_index(turn, target_square, black_pawn, king_square);
		}
		remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);
		add_index(indext, pos->accumulation, pos->psqtaccumulation, turn);
	}
}

/* should be called after do_move */
void do_accumulator(struct position *pos, move_t *m) {
	assert(*m);
	assert(!pos->mailbox[move_from(m)]);
	assert(pos->mailbox[move_to(m)]);
	if (!option_nnue)
		return;
	int target_square = move_to(m);
	if (uncolored_piece(pos->mailbox[target_square]) == king) {
		refresh_accumulator(pos, other_color(pos->turn));
		if (move_capture(m)) {
			unsigned index;
			int turn = pos->turn;
			int king_square = orient_horizontal(turn, ctz(pos->piece[turn][king]));
			index = make_index(turn, target_square, colored_piece(move_capture(m), turn), king_square);
			remove_index(index, pos->accumulation, pos->psqtaccumulation, turn);
		}
		else if (move_flag(m) == 3) {
			unsigned index;
			int turn = pos->turn;
			int king_square = orient_horizontal(turn, ctz(pos->piece[turn][king]));
			switch (target_square) {
			case g1:
				index = make_index(black, h1, white_rook, king_square);
				remove_index(index, pos->accumulation, pos->psqtaccumulation, black);
				index = make_index(black, f1, white_rook, king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, black);
				break;
			case c1:
				index = make_index(black, a1, white_rook, king_square);
				remove_index(index, pos->accumulation, pos->psqtaccumulation, black);
				index = make_index(black, d1, white_rook, king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, black);
				break;
			case g8:
				index = make_index(white, h8, black_rook, king_square);
				remove_index(index, pos->accumulation, pos->psqtaccumulation, white);
				index = make_index(white, f8, black_rook, king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, white);
				break;
			case c8:
				index = make_index(white, a8, black_rook, king_square);
				remove_index(index, pos->accumulation, pos->psqtaccumulation, white);
				index = make_index(white, d8, black_rook, king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, white);
				break;
			}
		}
	}
	else {
		do_update_accumulator(pos, m, black);
		do_update_accumulator(pos, m, white);
	}
}

/* should be called after undo_move */
void undo_accumulator(struct position *pos, move_t *m) {
	assert(*m);
	assert(pos->mailbox[move_from(m)]);
	if (!option_nnue)
		return;
	int source_square = move_from(m);
	int target_square = move_to(m);
	if (uncolored_piece(pos->mailbox[source_square]) == king) {
		refresh_accumulator(pos, pos->turn);
		if (move_capture(m)) {
			unsigned index;
			int turn = other_color(pos->turn);
			int king_square = orient_horizontal(turn, ctz(pos->piece[turn][king]));
			index = make_index(turn, target_square, colored_piece(move_capture(m), turn), king_square);
			add_index(index, pos->accumulation, pos->psqtaccumulation, turn);
		}
		else if (move_flag(m) == 3) {
			unsigned index;
			int turn = other_color(pos->turn);
			int king_square = orient_horizontal(turn, ctz(pos->piece[turn][king]));
			switch (target_square) {
			case g1:
				index = make_index(black, h1, white_rook, king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, black);
				index = make_index(black, f1, white_rook, king_square);
				remove_index(index, pos->accumulation, pos->psqtaccumulation, black);
				break;
			case c1:
				index = make_index(black, a1, white_rook, king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, black);
				index = make_index(black, d1, white_rook, king_square);
				remove_index(index, pos->accumulation, pos->psqtaccumulation, black);
				break;
			case g8:
				index = make_index(white, h8, black_rook, king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, white);
				index = make_index(white, f8, black_rook, king_square);
				remove_index(index, pos->accumulation, pos->psqtaccumulation, white);
				break;
			case c8:
				index = make_index(white, a8, black_rook, king_square);
				add_index(index, pos->accumulation, pos->psqtaccumulation, white);
				index = make_index(white, d8, black_rook, king_square);
				remove_index(index, pos->accumulation, pos->psqtaccumulation, white);
				break;
			}
		}
	}
	else {
		undo_update_accumulator(pos, m, black);
		undo_update_accumulator(pos, m, white);
	}
}

int32_t evaluate_accumulator(const struct position *pos) {
	struct data buf;
	transform(pos, pos->accumulation, buf.ft_out);
	affine_propagate(buf.ft_out, buf.hidden1_out, FT_OUT_DIMS, hidden1_biases, hidden1_weights);
	affine_propagate(buf.hidden1_out, buf.hidden2_out, 32, hidden2_biases, hidden2_weights);
	int32_t psqt = (pos->psqtaccumulation[pos->turn] - pos->psqtaccumulation[other_color(pos->turn)]) / 2;
	return (output_layer(buf.hidden2_out, output_biases, output_weights)) / FV_SCALE + psqt;
}

int32_t evaluate_nnue(struct position *pos) {
	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);
	return evaluate_accumulator(pos);
}

void permute_biases(bias_t *biases) {
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
	permute_biases(hidden1_biases);
	permute_biases(hidden2_biases);
	permute_weights(hidden1_weights);
}
