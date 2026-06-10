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

#include "nnue.h"

#include <inttypes.h>
#include <stdio.h>
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

struct ftcache {
	alignas(64) int16_t accumulation[K_HALF_DIMENSIONS];
	int32_t psqtaccumulation[PSQT_BUCKETS];
	uint64_t piece[2][7];
	int set;
};

struct ftcache ftcache[2][64];

void reset_ftcache(void) {
	for (int color = 0; color < 2; color++)
		for (int sq = 0; sq < 64; sq++)
			ftcache[color][sq].set = 0;
}

struct data {
	alignas(64) int8_t ft_out[FT_OUT_DIMS];
	alignas(64) int8_t hidden1_out[16];
	alignas(64) int8_t hidden2_out[32];
};

extern alignas(64) ft_weight_t builtin_ft_weights[K_HALF_DIMENSIONS * FT_IN_DIMS];
extern alignas(64) ft_bias_t builtin_ft_biases[K_HALF_DIMENSIONS];

extern alignas(64) ft_weight_t builtin_psqt_weights[FT_IN_DIMS * PSQT_BUCKETS];

extern alignas(64) weight_t builtin_hidden1_weights[HIDDEN1_OUT_DIMS * FT_OUT_DIMS];
extern alignas(64) bias_t builtin_hidden1_biases[HIDDEN1_OUT_DIMS];

extern alignas(64) weight_t builtin_hidden2_weights[HIDDEN2_OUT_DIMS * HIDDEN1_OUT_DIMS];
extern alignas(64) bias_t builtin_hidden2_biases[HIDDEN2_OUT_DIMS];

extern alignas(64) weight_t builtin_output_weights[1 * HIDDEN2_OUT_DIMS];
extern alignas(64) bias_t builtin_output_biases[1];



alignas(64) ft_weight_t file_ft_weights[K_HALF_DIMENSIONS * FT_IN_DIMS];
alignas(64) ft_bias_t file_ft_biases[K_HALF_DIMENSIONS];

alignas(64) ft_weight_t file_psqt_weights[FT_IN_DIMS * PSQT_BUCKETS];

alignas(64) weight_t file_hidden1_weights[HIDDEN1_OUT_DIMS * FT_OUT_DIMS];
alignas(64) bias_t file_hidden1_biases[HIDDEN1_OUT_DIMS];

alignas(64) weight_t file_hidden2_weights[HIDDEN2_OUT_DIMS * HIDDEN1_OUT_DIMS];
alignas(64) bias_t file_hidden2_biases[HIDDEN2_OUT_DIMS];

alignas(64) weight_t file_output_weights[1 * HIDDEN2_OUT_DIMS];
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

static inline void affine_propagate_hidden1(const int8_t *input, int8_t *output,
		const bias_t *biases, const weight_t *weights) {
#if defined(AVX2)
	__m256i out0 = ((const __m256i *)biases)[0];
	__m256i out1 = ((const __m256i *)biases)[1];
#if !defined(VNNI)
	const __m256i ones = _mm256_set1_epi16(1);
#endif

	for (int i = 0; i < FT_OUT_DIMS / 4; i++) {
		int32_t quad;
		memcpy(&quad, input + 4 * i, sizeof(quad));
		__m256i in = _mm256_set1_epi32(quad);
		__m256i weight0 = ((const __m256i *)weights)[2 * i];
		__m256i weight1 = ((const __m256i *)weights)[2 * i + 1];
#if defined(VNNI)
		out0 = _mm256_dpbusd_epi32(out0, in, weight0);
		out1 = _mm256_dpbusd_epi32(out1, in, weight1);
#else
		out0 = _mm256_add_epi32(out0, _mm256_madd_epi16(_mm256_maddubs_epi16(in, weight0), ones));
		out1 = _mm256_add_epi32(out1, _mm256_madd_epi16(_mm256_maddubs_epi16(in, weight1), ones));
#endif
	}

	/* 16-bit words are in the order 0-3, 8-11, 4-7, 12-15. */
	__m256i out01 = _mm256_srai_epi16(_mm256_packs_epi32(out0, out1), SHIFT);
	/* 32-bit lanes are in the order 0-3, 8-11, 4-7, 12-15. */
	__m128i out = _mm_packs_epi16(_mm256_castsi256_si128(out01), _mm256_extracti128_si256(out01, 1));
	out = _mm_shuffle_epi32(out, 0xd8);
	*(__m128i *)output = _mm_max_epi8(out, _mm_setzero_si128());
#else
	int i, j;
	bias_t tmp[HIDDEN1_OUT_DIMS];
	memcpy(tmp, biases, HIDDEN1_OUT_DIMS * sizeof(biases[0]));

	for (i = 0; i < FT_OUT_DIMS; i++)
		if (input[i])
			for (j = 0; j < HIDDEN1_OUT_DIMS; j++)
				tmp[j] += input[i] * weights[HIDDEN1_OUT_DIMS * i + j];

	for (j = 0; j < HIDDEN1_OUT_DIMS; j++)
		output[j] = clamp(tmp[j] >> SHIFT, 0, 127);
#endif
}

static inline void affine_propagate_hidden2(const int8_t *input, int8_t *output,
		const bias_t *biases, const weight_t *weights) {
#if defined(AVX2)
	__m256i out0 = ((const __m256i *)biases)[0];
	__m256i out1 = ((const __m256i *)biases)[1];
	__m256i out2 = ((const __m256i *)biases)[2];
	__m256i out3 = ((const __m256i *)biases)[3];
#if !defined(VNNI)
	const __m256i ones = _mm256_set1_epi16(1);
#endif

	for (int i = 0; i < HIDDEN1_OUT_DIMS / 4; i++) {
		int32_t quad;
		memcpy(&quad, input + 4 * i, sizeof(quad));
		__m256i in = _mm256_set1_epi32(quad);
		__m256i weight0 = ((const __m256i *)weights)[4 * i];
		__m256i weight1 = ((const __m256i *)weights)[4 * i + 1];
		__m256i weight2 = ((const __m256i *)weights)[4 * i + 2];
		__m256i weight3 = ((const __m256i *)weights)[4 * i + 3];
#if defined(VNNI)
		out0 = _mm256_dpbusd_epi32(out0, in, weight0);
		out1 = _mm256_dpbusd_epi32(out1, in, weight1);
		out2 = _mm256_dpbusd_epi32(out2, in, weight2);
		out3 = _mm256_dpbusd_epi32(out3, in, weight3);
#else
		out0 = _mm256_add_epi32(out0, _mm256_madd_epi16(_mm256_maddubs_epi16(in, weight0), ones));
		out1 = _mm256_add_epi32(out1, _mm256_madd_epi16(_mm256_maddubs_epi16(in, weight1), ones));
		out2 = _mm256_add_epi32(out2, _mm256_madd_epi16(_mm256_maddubs_epi16(in, weight2), ones));
		out3 = _mm256_add_epi32(out3, _mm256_madd_epi16(_mm256_maddubs_epi16(in, weight3), ones));
#endif
	}

	__m256i out01 = _mm256_srai_epi16(_mm256_packs_epi32(out0, out1), SHIFT);
	__m256i out23 = _mm256_srai_epi16(_mm256_packs_epi32(out2, out3), SHIFT);
	/* 32-bit lanes are in the order 0-3, 8-11, 16-19, 24-27, 4-7, 12-15, 20-23, 28-31. */
	__m256i out = _mm256_packs_epi16(out01, out23);
	out = _mm256_permutevar8x32_epi32(out, _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7));
	*(__m256i *)output = _mm256_max_epi8(out, _mm256_setzero_si256());
#else
	int i, j;
	bias_t tmp[HIDDEN2_OUT_DIMS];
	memcpy(tmp, biases, HIDDEN2_OUT_DIMS * sizeof(biases[0]));

	for (i = 0; i < HIDDEN1_OUT_DIMS; i++)
		if (input[i])
			for (j = 0; j < HIDDEN2_OUT_DIMS; j++)
				tmp[j] += input[i] * weights[HIDDEN2_OUT_DIMS * i + j];

	for (j = 0; j < HIDDEN2_OUT_DIMS; j++)
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
	for (int i = 0; i < HIDDEN2_OUT_DIMS; i++)
		sum += weights[i] * input[i];
	return sum;
#endif
}

static inline void add_index(unsigned index, int16_t accumulation[K_HALF_DIMENSIONS], int32_t psqtaccumulation[PSQT_BUCKETS]) {
	assert(nnue_init_done);
	const unsigned offset = K_HALF_DIMENSIONS * index;
#if defined(AVX2)
	for (int j = 0; j < K_HALF_DIMENSIONS; j += 16) {
		__m256i *a = (__m256i *)(accumulation + j);
		__m256i b = *(__m256i *)(ft_weights + offset + j);
		*a = _mm256_add_epi16(*a, b);
	}
#else
	for (int j = 0; j < K_HALF_DIMENSIONS; j++)
		accumulation[j] += ft_weights[offset + j];
#endif
	for (int j = 0; j < PSQT_BUCKETS; j++)
		psqtaccumulation[j] += psqt_weights[PSQT_BUCKETS * index + j];
}

void add_index_slow(unsigned index, int16_t accumulation[K_HALF_DIMENSIONS], int32_t psqtaccumulation[PSQT_BUCKETS]) {
	add_index(index, accumulation, psqtaccumulation);
}

static inline void remove_index(unsigned index, int16_t accumulation[K_HALF_DIMENSIONS], int32_t psqtaccumulation[PSQT_BUCKETS]) {
	assert(nnue_init_done);
	const unsigned offset = K_HALF_DIMENSIONS * index;
#if defined(AVX2)
	for (int j = 0; j < K_HALF_DIMENSIONS; j += 16) {
		__m256i *a = (__m256i *)(accumulation + j);
		__m256i b = *(__m256i *)(ft_weights + offset + j);
		*a = _mm256_sub_epi16(*a, b);
	}
#else
	for (int j = 0; j < K_HALF_DIMENSIONS; j++)
		accumulation[j] -= ft_weights[offset + j];
#endif
	for (int j = 0; j < PSQT_BUCKETS; j++)
		psqtaccumulation[j] -= psqt_weights[PSQT_BUCKETS * index + j];
}

void refresh_accumulator(struct position *pos, int turn) {
	assert(nnue_init_done);
	int king_square = ctz(pos->piece[turn][KING]);
	struct ftcache *entry = &ftcache[turn][orient_horizontal(turn, king_square)];
	if (!entry->set) {
		memcpy(entry->accumulation, ft_biases, K_HALF_DIMENSIONS * sizeof(*ft_biases));
		memset(entry->psqtaccumulation, 0, PSQT_BUCKETS * sizeof(*pos->psqtaccumulation[turn]));
		memset(entry->piece, 0, sizeof(entry->piece));
		entry->set = 1;
	}
	uint64_t b;
	int square;
	for (int color = 0; color < 2; color++) {
		for (int piece = PAWN; piece <= KING; piece++) {
			if (piece == KING && color == turn)
				continue;
			b = pos->piece[color][piece] & ~entry->piece[color][piece];
			while (b) {
				square = ctz(b);
				int index = make_index(turn, square, colored_piece(piece, color), king_square);
				add_index(index, entry->accumulation, entry->psqtaccumulation);
				b = clear_ls1b(b);
			}
			b = ~pos->piece[color][piece] & entry->piece[color][piece];
			while (b) {
				square = ctz(b);
				int index = make_index(turn, square, colored_piece(piece, color), king_square);
				remove_index(index, entry->accumulation, entry->psqtaccumulation);
				b = clear_ls1b(b);
			}

			entry->piece[color][piece] = pos->piece[color][piece];
		}
	}
	memcpy(pos->accumulation[turn], entry->accumulation, K_HALF_DIMENSIONS * sizeof(*entry->accumulation));
	memcpy(pos->psqtaccumulation[turn], entry->psqtaccumulation, PSQT_BUCKETS * sizeof(*entry->psqtaccumulation));
}

/* m cannot be a king move of the side <turn>. */
void do_update_accumulator(struct position *pos, move_t *move, int turn) {
	int source_square = move_from(move);
	int target_square = move_to(move);
	int king_square = ctz(pos->piece[turn][KING]);
	unsigned index;
	unsigned indext;

	index = make_index(turn, source_square, pos->mailbox[target_square], king_square);
	remove_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);

	index = make_index(turn, target_square, pos->mailbox[target_square], king_square);
	add_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);

	if (move_capture(move) && move_flag(move) != MOVE_EN_PASSANT) {
		index = make_index(turn, target_square, colored_piece(move_capture(move), pos->turn), king_square);
		remove_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);
	}

	if (move_flag(move) == MOVE_EN_PASSANT) {
		if (pos->turn)
			index = make_index(turn, target_square + 8, WHITE_PAWN, king_square);
		else
			index = make_index(turn, target_square - 8, BLACK_PAWN, king_square);
		remove_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);
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
		add_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);
		remove_index(indext, pos->accumulation[turn], pos->psqtaccumulation[turn]);
	}
	else if (move_flag(move) == MOVE_CASTLE) {
		switch (target_square) {
		case g1:
			index = make_index(BLACK, h1, WHITE_ROOK, king_square);
			remove_index(index, pos->accumulation[BLACK], pos->psqtaccumulation[BLACK]);

			index = make_index(BLACK, f1, WHITE_ROOK, king_square);
			add_index(index, pos->accumulation[BLACK], pos->psqtaccumulation[BLACK]);
			break;
		case c1:
			index = make_index(BLACK, a1, WHITE_ROOK, king_square);
			remove_index(index, pos->accumulation[BLACK], pos->psqtaccumulation[BLACK]);

			index = make_index(BLACK, d1, WHITE_ROOK, king_square);
			add_index(index, pos->accumulation[BLACK], pos->psqtaccumulation[BLACK]);
			break;
		case g8:
			index = make_index(WHITE, h8, BLACK_ROOK, king_square);
			remove_index(index, pos->accumulation[WHITE], pos->psqtaccumulation[WHITE]);

			index = make_index(WHITE, f8, BLACK_ROOK, king_square);
			add_index(index, pos->accumulation[WHITE], pos->psqtaccumulation[WHITE]);
			break;
		case c8:
			index = make_index(WHITE, a8, BLACK_ROOK, king_square);
			remove_index(index, pos->accumulation[WHITE], pos->psqtaccumulation[WHITE]);

			index = make_index(WHITE, d8, BLACK_ROOK, king_square);
			add_index(index, pos->accumulation[WHITE], pos->psqtaccumulation[WHITE]);
			break;
		}
	}
}

void undo_update_accumulator(struct position *pos, move_t *move, int turn) {
	int source_square = move_from(move);
	int target_square = move_to(move);
	int king_square = ctz(pos->piece[turn][KING]);
	unsigned index;
	unsigned indext;

	index = make_index(turn, target_square, pos->mailbox[source_square], king_square);
	remove_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);

	index = make_index(turn, source_square, pos->mailbox[source_square], king_square);
	add_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);

	if (move_capture(move) && move_flag(move) != MOVE_EN_PASSANT) {
		index = make_index(turn, target_square, colored_piece(move_capture(move), other_color(pos->turn)), king_square);
		add_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);
	}

	if (move_flag(move) == MOVE_EN_PASSANT) {
		if (pos->turn)
			index = make_index(turn, target_square - 8, BLACK_PAWN, king_square);
		else
			index = make_index(turn, target_square + 8, WHITE_PAWN, king_square);
		add_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);
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
		remove_index(index, pos->accumulation[turn], pos->psqtaccumulation[turn]);
		add_index(indext, pos->accumulation[turn], pos->psqtaccumulation[turn]);
	}
	else if (move_flag(move) == MOVE_CASTLE) {
		switch (target_square) {
		case g1:
			index = make_index(BLACK, h1, WHITE_ROOK, king_square);
			add_index(index, pos->accumulation[BLACK], pos->psqtaccumulation[BLACK]);

			index = make_index(BLACK, f1, WHITE_ROOK, king_square);
			remove_index(index, pos->accumulation[BLACK], pos->psqtaccumulation[BLACK]);
			break;
		case c1:
			index = make_index(BLACK, a1, WHITE_ROOK, king_square);
			add_index(index, pos->accumulation[BLACK], pos->psqtaccumulation[BLACK]);

			index = make_index(BLACK, d1, WHITE_ROOK, king_square);
			remove_index(index, pos->accumulation[BLACK], pos->psqtaccumulation[BLACK]);
			break;
		case g8:
			index = make_index(WHITE, h8, BLACK_ROOK, king_square);
			add_index(index, pos->accumulation[WHITE], pos->psqtaccumulation[WHITE]);

			index = make_index(WHITE, f8, BLACK_ROOK, king_square);
			remove_index(index, pos->accumulation[WHITE], pos->psqtaccumulation[WHITE]);
			break;
		case c8:
			index = make_index(WHITE, a8, BLACK_ROOK, king_square);
			add_index(index, pos->accumulation[WHITE], pos->psqtaccumulation[WHITE]);

			index = make_index(WHITE, d8, BLACK_ROOK, king_square);
			remove_index(index, pos->accumulation[WHITE], pos->psqtaccumulation[WHITE]);
			break;
		}
	}
}

/* Should be called after do_move. */
void do_accumulator(struct position *pos, move_t *move) {
	assert(*move);
	assert(!pos->mailbox[move_from(move)]);
	assert(pos->mailbox[move_to(move)]);
	assert(nnue_init_done);
	int target_square = move_to(move);
	if (uncolored_piece(pos->mailbox[target_square]) == KING)
		refresh_accumulator(pos, other_color(pos->turn));
	else
		do_update_accumulator(pos, move, other_color(pos->turn));
	do_update_accumulator(pos, move, pos->turn);
#if !defined(NDEBUG) && defined(FULLDEBUG)
	int16_t accumulation[2][K_HALF_DIMENSIONS];
	memcpy(accumulation, pos->accumulation, 2 * K_HALF_DIMENSIONS * sizeof(**accumulation));

	int32_t psqtaccumulation[2][PSQT_BUCKETS];
	memcpy(psqtaccumulation, pos->psqtaccumulation, 2 * PSQT_BUCKETS * sizeof(**psqtaccumulation));

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
		for (int j = 0; j < PSQT_BUCKETS; j++) {
			if (psqtaccumulation[i][j] != pos->psqtaccumulation[i][j]) {
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
	assert(nnue_init_done);
	int source_square = move_from(move);
	if (uncolored_piece(pos->mailbox[source_square]) == KING)
		refresh_accumulator(pos, pos->turn);
	else
		undo_update_accumulator(pos, move, pos->turn);
	undo_update_accumulator(pos, move, other_color(pos->turn));
#if !defined(NDEBUG) && defined(FULLDEBUG)
	int16_t accumulation[2][K_HALF_DIMENSIONS];
	memcpy(accumulation, pos->accumulation, 2 * K_HALF_DIMENSIONS * sizeof(**accumulation));

	int32_t psqtaccumulation[2][PSQT_BUCKETS];
	memcpy(psqtaccumulation, pos->psqtaccumulation, 2 * PSQT_BUCKETS * sizeof(**psqtaccumulation));

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
		for (int j = 0; j < PSQT_BUCKETS; j++) {
			if (psqtaccumulation[i][j] != pos->psqtaccumulation[i][j]) {
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

int32_t evaluate_accumulator(const struct position *pos) {
	assert(nnue_init_done);
	struct data buf;
	transform(pos, pos->accumulation, buf.ft_out);
	affine_propagate_hidden1(buf.ft_out, buf.hidden1_out, hidden1_biases, hidden1_weights);
	affine_propagate_hidden2(buf.hidden1_out, buf.hidden2_out, hidden2_biases, hidden2_weights);
	int32_t psqt = (pos->psqtaccumulation[pos->turn][0] - pos->psqtaccumulation[other_color(pos->turn)][0]) / 2;
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

static void permute_quad(weight_t *weights, int in_dims, int out_dims) {
	weight_t *tmp = malloc(in_dims * out_dims * sizeof(*tmp));

	for (int g = 0; g < in_dims / 4; g++)
		for (int h = 0; h < out_dims / 8; h++)
			for (int j = 0; j < 8; j++)
				for (int k = 0; k < 4; k++)
					tmp[4 * out_dims * g + 32 * h + 4 * j + k] = weights[out_dims * (4 * g + k) + 8 * h + j];

	memcpy(weights, tmp, in_dims * out_dims * sizeof(*tmp));
	free(tmp);
}
#endif

void permute_weights(void) {
#if defined(AVX2)
	for (int col = 0; col < FT_OUT_DIMS; col++)
		if (8 <= col % 32 && col % 32 < 16)
			swap_cols(hidden1_weights, HIDDEN1_OUT_DIMS, col, col + 8);

	permute_quad(hidden1_weights, FT_OUT_DIMS, HIDDEN1_OUT_DIMS);
	permute_quad(hidden2_weights, HIDDEN1_OUT_DIMS, HIDDEN2_OUT_DIMS);
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
	reset_ftcache();

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
	printf("info string evaluation nnue ");
	if (builtin)
		printf("built in");
	else
		printf("file <%s>", pathnnue);
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

	reset_ftcache();

#ifndef NDEBUG
	nnue_init_done = 1;
#endif
}

const char *simd =
#if defined(VNNI)
"vnni"
#elif defined(AVX2)
"avx2"
#else
"none"
#endif
;
