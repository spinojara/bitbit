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

#include "endgame.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "position.h"
#include "util.h"
#include "evaluate.h"
#include "bitboard.h"
#include "move.h"
#include "bitbase.h"

uint64_t endgame_keys[2 * 6 * 11];
struct endgame endgame_table[ENDGAMESIZE] = { 0 };
struct endgame endgame_KXK[2] = { 0 };

const uint32_t mv[6] = { 0, 0x1, 0x10, 0x100, 0x1000, 0x10000 };

int verify_material(const struct position *pos, int color, int material_verify) {
	int material = 0;
	for (int piece = PAWN; piece <= QUEEN; piece++)
		material += mv[piece] * popcount(pos->piece[color][piece]);
	if (material_verify != -1 && material_verify != material)
		return 0;
	return 1;
}

static inline uint64_t endgame_key(int color, int piece, int count) {
	assert(color == 0 || color == 1);
	assert(piece <= 5);
	return endgame_keys[color + 2 * piece + 2 * 5 * count];
}

void refresh_endgame_key(struct position *pos) {
	pos->endgame_key = 0;
	for (int color = 0; color < 2; color++) {
		for (int piece = PAWN; piece < KING; piece++) {
			int count = popcount(pos->piece[color][piece]);
			pos->endgame_key ^= endgame_key(color, piece, count);
		}
	}
}

void endgame_store(const char *str, int32_t (*evaluate)(const struct position *pos, int strong_side)) {
	struct position pos;
	for (int color = 0; color < 2; color++) {
		char strong[64];
		char weak[64];
		for (size_t i = 0, j = 0, k = 0, K = 0; i < strlen(str); i++) {
			if (str[i] == 'K')
				K++;
			if (K == 1) {
				char c = str[i];
				strong[j++] = c;
			}
			else if (K == 2) {
				char c = str[i];
				weak[k++] = c;
			}
			strong[j] = '\0';
			weak[k] = '\0';
		}
		/* For KPKP for example, strong_side will be equal to white. */
		if (!strcmp(strong, weak) && !color)
			continue;
		char *ptr = color ? weak : strong;
		for (size_t i = 0; i < strlen(ptr); i++)
			ptr[i] = tolower(ptr[i]);

		char pieces[512];
		sprintf(pieces, "%s%ld/8/8/8/8/8/8/%s%ld", strong, 8 - strlen(strong), weak, 8 - strlen(weak));

		char *fen[] = { pieces, "w", "-", "-", "0", "1", };
		pos_from_fen(&pos, 6, fen);
		refresh_endgame_key(&pos);
		
		struct endgame *e = endgame_probe(&pos);
		if (e) {
			fprintf(stderr, "Endgame %s already exists.\n", str);
			exit(1);
		}
		e = endgame_get(&pos);
		if (e->evaluate) {
			fprintf(stderr, "Endgame %s entry collision.\n", str);
			exit(1);
		}

		e->evaluate = evaluate;
		e->endgame_key = pos.endgame_key;
		e->strong_side = color;
	}
}

/* This function makes sure that the hashing of endgames is injective. */
void endgame_test(void) {
#ifdef NDEBUG
	printf("Assertions need to be enabled for endgame_test to work properly.\n");
	exit(1);
#endif
	struct position pos = { 0 };
	size_t max_pieces[] = { 0, 9, 11, 11, 11, 10 };
	size_t max_index = 1;
	for (size_t i = PAWN; i < KING; i++)
		max_index *= max_pieces[i] * max_pieces[i];
	for (size_t index = 0; index < max_index; index++) {
		int pieces[2][7];
		pieces[WHITE][KING] = pieces[BLACK][KING] = 1;
		size_t denominator = 1;
		for (int color = 0; color < 2; color++) {
			int total = 1;
			for (int piece = PAWN; piece < KING; piece++) {
				pieces[color][piece] = (index / denominator) % max_pieces[piece];
				denominator *= max_pieces[piece];
				total += pieces[color][piece];
				if (total > 16)
					goto outer;
			}
		}

		pos.piece[WHITE][ALL] = pos.piece[BLACK][ALL] = 0;
		for (int color = 0; color < 2; color++) {
			int sign = 2 * color - 1;
			int total = 0;
			for (int piece = PAWN; piece <= KING; piece++) {
				pos.piece[color][piece] = (1 << pieces[color][piece]) - 1;
				total += pieces[color][piece];
				pos.piece[color][piece] <<= 32 - sign * total - pieces[BLACK][piece] * (color == BLACK);
				pos.piece[color][ALL] |= pos.piece[color][piece];
			}
		}

		refresh_endgame_key(&pos);
		struct endgame *e = endgame_probe(&pos);
		/* Asserts verify_material. */
		if (e)
			endgame_evaluate(e, &pos);

outer:;
		if (index % (max_index / 100) == 0)
			printf("%ld%%\r", 100 * index / max_index);
	}
	printf("Endgame table works correctly.\n");
}

/* +---+---+---+---+---+---+---+---+
 * | 6 | 5 | 4 | 3 | 3 | 4 | 5 | 6 |
 * +---+---+---+---+---+---+---+---+
 * | 5 | 4 | 3 | 2 | 2 | 3 | 4 | 5 |
 * +---+---+---+---+---+---+---+---+
 * | 4 | 3 | 2 | 1 | 1 | 2 | 3 | 4 |
 * +---+---+---+---+---+---+---+---+
 * | 3 | 2 | 1 | 0 | 0 | 1 | 2 | 3 |
 * +---+---+---+---+---+---+---+---+
 * | 3 | 2 | 1 | 0 | 0 | 1 | 2 | 3 |
 * +---+---+---+---+---+---+---+---+
 * | 4 | 3 | 2 | 1 | 1 | 2 | 3 | 4 |
 * +---+---+---+---+---+---+---+---+
 * | 5 | 4 | 3 | 2 | 2 | 3 | 4 | 5 |
 * +---+---+---+---+---+---+---+---+
 * | 6 | 5 | 4 | 3 | 3 | 4 | 5 | 6 |
 * +---+---+---+---+---+---+---+---+
 */
static inline int32_t push_toward_edge(int square) {
	int f = file_of(square);
	int r = rank_of(square);
	f = min(f, 7 - f);
	r = min(r, 7 - r);
	return 6 - (f + r);
}

static inline int32_t push_toward(int square1, int square2) {
	return 7 - distance(square1, square2);
}

static inline int32_t push_away(int square1, int square2) {
	return distance(square1, square2);
}

int32_t evaluate_KPK(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	UNUSED(weak_side);
	assert(verify_material(pos, strong_side, mv[PAWN]));
	assert(verify_material(pos, weak_side, 0));
	int pawn_square = ctz(pos->piece[strong_side][PAWN]);
	int r = rank_of(orient_horizontal(strong_side, pawn_square));
	int32_t eval = r;
	if (bitbase_KPK_probe(pos, strong_side) == BITBASE_WIN)
		eval += VALUE_WIN + material_value[PAWN] - pos->halfmove;
	return eval;
}

int32_t evaluate_KPKP(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	UNUSED(weak_side);
	assert(verify_material(pos, strong_side, mv[PAWN]));
	assert(verify_material(pos, weak_side, mv[PAWN]));
	int pawn_white = ctz(pos->piece[WHITE][PAWN]);
	int y_white = rank_of(pawn_white);
	int pawn_black = orient_horizontal(BLACK, ctz(pos->piece[BLACK][PAWN]));
	int y_black = rank_of(pawn_black);
	int32_t eval = y_white - y_black;
	unsigned p = bitbase_KPKP_probe(pos, strong_side);
	if (p == BITBASE_WIN)
		eval += VALUE_WIN - pos->halfmove;
	else if (p == BITBASE_LOSE)
		eval -= VALUE_WIN - pos->halfmove;
	return eval;
}

int32_t evaluate_KBPK(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	UNUSED(weak_side);
	assert(verify_material(pos, strong_side, mv[BISHOP] + mv[PAWN]));
	assert(verify_material(pos, weak_side, 0));
	int pawn_square = orient_horizontal(strong_side, ctz(pos->piece[strong_side][PAWN]));
	int bishop_square = orient_horizontal(strong_side, ctz(pos->piece[strong_side][BISHOP]));
	int f = file_of(pawn_square);
	int r = rank_of(pawn_square);
	int promotion_square = f + 8 * 7;
	int32_t eval = r;
	/* We can probe bitbase_KPK directly because the bishop is simply ignored. */
	if ((0 < f && f < 7) || (same_colored_squares(bishop_square) & same_colored_squares(promotion_square)) ||
			bitbase_KPK_probe(pos, strong_side) == BITBASE_WIN)
		eval += VALUE_WIN + material_value[BISHOP] + material_value[PAWN] - pos->halfmove;
	return eval;
}

int32_t evaluate_KRKP(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[ROOK]));
	assert(verify_material(pos, weak_side, mv[PAWN]));
	int strong_king = ctz(pos->piece[strong_side][KING]);
	int weak_king = ctz(pos->piece[weak_side][KING]);
	int pawn_square = orient_horizontal(weak_side, ctz(pos->piece[weak_side][PAWN]));
	int r = rank_of(pawn_square);
	int32_t eval = -r;
	unsigned p = bitbase_KRKP_probe(pos, strong_side);
	if (p == BITBASE_WIN)
		eval += VALUE_WIN + material_value[ROOK] - material_value[PAWN] + push_toward(strong_king, weak_king) - pos->halfmove;
	else if (p == BITBASE_LOSE)
		eval -= VALUE_WIN + material_value[PAWN] - material_value[ROOK] - pos->halfmove;
	return eval;
}

int32_t evaluate_KRKN(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[ROOK]));
	assert(verify_material(pos, weak_side, mv[KNIGHT]));
	int strong_king = ctz(pos->piece[strong_side][KING]);
	int weak_king = ctz(pos->piece[weak_side][KING]);
	int weak_knight = ctz(pos->piece[weak_side][KNIGHT]);
	return 2 * push_toward_edge(weak_king) + push_away(weak_king, weak_knight) + 3 * push_toward(weak_king, strong_king);
}

int32_t evaluate_KRKB(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[ROOK]));
	assert(verify_material(pos, weak_side, mv[BISHOP]));
	int weak_king = ctz(pos->piece[weak_side][KING]);
	return push_toward_edge(weak_king);
}

/* Drawish because of 50 move rule. */
int32_t evaluate_KBBKN(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, 2 * mv[BISHOP]));
	assert(verify_material(pos, weak_side, mv[KNIGHT]));
	int strong_king = ctz(pos->piece[strong_side][KING]);
	int weak_king = ctz(pos->piece[weak_side][KING]);
	return push_toward(strong_king, weak_king) + 4 * push_toward_edge(weak_king);
}

int32_t evaluate_KNNK(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	UNUSED(weak_side);
	UNUSED(pos);
	assert(verify_material(pos, strong_side, 2 * mv[KNIGHT]));
	assert(verify_material(pos, weak_side, 0));
	return 0;
}

int32_t evaluate_KNNKP(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, 2 * mv[KNIGHT]));
	assert(verify_material(pos, weak_side, mv[PAWN]));
	int square = ctz(pos->piece[weak_side][PAWN]);
	int weak_king = ctz(pos->piece[weak_side][KING]);
	int r = rank_of(orient_horizontal(weak_side, square));
	return 3 * push_toward_edge(weak_king) - 4 * r;
}

int32_t evaluate_KBNK(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[BISHOP] + mv[KNIGHT]));
	assert(verify_material(pos, weak_side, 0));
	int weak_king = ctz(pos->piece[weak_side][KING]);
	int strong_king = ctz(pos->piece[strong_side][KING]);
	uint64_t darkbishop = pos->piece[strong_side][BISHOP] & same_colored_squares(a1);
	int corner1 = darkbishop ? a1 : a8;
	int corner2 = darkbishop ? h8 : h1;
	int closest_corner = distance(weak_king, corner1) < distance(weak_king, corner2) ? corner1 : corner2;
	return VALUE_WIN + material_value[BISHOP] + material_value[KNIGHT] + push_toward(strong_king, weak_king) + 0x8 * push_toward(closest_corner, weak_king) - pos->halfmove;
}

int32_t evaluate_KQKP(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[QUEEN]));
	assert(verify_material(pos, weak_side, mv[PAWN]));
	int weak_king = ctz(pos->piece[weak_side][KING]);
	int strong_king = ctz(pos->piece[strong_side][KING]);
	int32_t eval = push_toward(strong_king, weak_king);

	int square = ctz(pos->piece[weak_side][PAWN]);
	int r = rank_of(orient_horizontal(weak_side, square));
	/* r != 7th rank */
	if (r != 6 || distance(weak_king, square) != 1 ||
			((FILE_B | FILE_D | FILE_E | FILE_G) & pos->piece[weak_side][PAWN]))
		eval += VALUE_WIN + material_value[QUEEN] - material_value[ROOK] - pos->halfmove;
	return eval;
}

int32_t evaluate_KQKX(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	int piece = pos->piece[weak_side][KNIGHT] ? KNIGHT : pos->piece[weak_side][BISHOP] ? BISHOP : ROOK;
	int weak_king = ctz(pos->piece[weak_side][KING]);
	int strong_king = ctz(pos->piece[strong_side][KING]);
	assert(verify_material(pos, strong_side, mv[QUEEN]));
	assert(verify_material(pos, weak_side, mv[piece]));
	return VALUE_WIN + material_value[QUEEN] - material_value[piece] + push_toward(strong_king, weak_king) + 0x8 * push_toward_edge(weak_king) - pos->halfmove;
}

int32_t evaluate_KXK(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, weak_side, 0));
	int weak_king = ctz(pos->piece[weak_side][KING]);
	int strong_king = ctz(pos->piece[strong_side][KING]);
	int32_t material = 0;
	for (int piece = PAWN; piece < KING; piece++)
		material += material_value[piece] * popcount(pos->piece[strong_side][piece]);
	return VALUE_WIN + material + push_toward(strong_king, weak_king) + 0x8 * push_toward_edge(weak_king) - pos->halfmove;
}

int is_KXK(const struct position *pos, int color) {
	return !(pos->piece[other_color(color)][ALL] ^ pos->piece[other_color(color)][KING]) &&
			(pos->piece[color][ROOK] ||
			 pos->piece[color][QUEEN] ||
			 (popcount(pos->piece[color][KNIGHT] | pos->piece[color][BISHOP]) >= 3) ||
			 (popcount(pos->piece[color][BISHOP]) >= 2));
}

/* Should be called before do_move. */
void do_endgame_key(struct position *pos, const move_t *move) {
	assert(*move);
	assert(pos->mailbox[move_from(move)]);
	int turn = pos->turn;
	int victim = uncolored_piece(pos->mailbox[move_to(move)]);
	if (victim) {
		int before = popcount(pos->piece[other_color(turn)][victim]);
		int after = before - 1;
		assert(before);
		pos->endgame_key ^= endgame_key(other_color(turn), victim, before);
		pos->endgame_key ^= endgame_key(other_color(turn), victim, after);
	}
	if (move_flag(move) == MOVE_EN_PASSANT) {
		int before = popcount(pos->piece[other_color(turn)][PAWN]);
		int after = before - 1;
		assert(before);
		pos->endgame_key ^= endgame_key(other_color(turn), PAWN, before);
		pos->endgame_key ^= endgame_key(other_color(turn), PAWN, after);
	}
	if (move_flag(move) == MOVE_PROMOTION) {
		int before_pawn = popcount(pos->piece[turn][PAWN]);
		int after_pawn = before_pawn - 1;
		assert(before_pawn);
		pos->endgame_key ^= endgame_key(turn, PAWN, before_pawn);
		pos->endgame_key ^= endgame_key(turn, PAWN, after_pawn);

		int piece = move_promote(move) + 2;
		int before = popcount(pos->piece[turn][piece]);
		int after = before + 1;
		pos->endgame_key ^= endgame_key(turn, piece, before);
		pos->endgame_key ^= endgame_key(turn, piece, after);
	}
}

/* Should be called before undo_move. */
void undo_endgame_key(struct position *pos, const move_t *move) {
	assert(*move);
	assert(!pos->mailbox[move_from(move)]);
	assert(pos->mailbox[move_to(move)]);
	int victim = move_capture(move);
	int turn = other_color(pos->turn);
	if (victim && move_flag(move) != MOVE_EN_PASSANT) {
		int before = popcount(pos->piece[other_color(turn)][victim]);
		int after = before + 1;
		pos->endgame_key ^= endgame_key(other_color(turn), victim, before);
		pos->endgame_key ^= endgame_key(other_color(turn), victim, after);
	}
	if (move_flag(move) == MOVE_EN_PASSANT) {
		int before = popcount(pos->piece[other_color(turn)][PAWN]);
		int after = before + 1;
		pos->endgame_key ^= endgame_key(other_color(turn), PAWN, before);
		pos->endgame_key ^= endgame_key(other_color(turn), PAWN, after);
	}
	if (move_flag(move) == MOVE_PROMOTION) {
		int before_pawn = popcount(pos->piece[turn][PAWN]);
		int after_pawn = before_pawn + 1;
		pos->endgame_key ^= endgame_key(turn, PAWN, before_pawn);
		pos->endgame_key ^= endgame_key(turn, PAWN, after_pawn);

		int piece = move_promote(move) + 2;
		int before = popcount(pos->piece[turn][piece]);
		int after = before - 1;
		pos->endgame_key ^= endgame_key(turn, piece, before);
		pos->endgame_key ^= endgame_key(turn, piece, after);
	}
}

void endgame_init(void) {
	uint64_t seed = SEED;
	for (size_t i = 0; i < SIZE(endgame_keys); i++)
		endgame_keys[i] = xorshift64(&seed);
	
	endgame_store("KPK",   &evaluate_KPK);
	endgame_store("KPKP",  &evaluate_KPKP);
	endgame_store("KBPK",  &evaluate_KBPK);
	endgame_store("KRKP",  &evaluate_KRKP);
	endgame_store("KRKN",  &evaluate_KRKN);
	endgame_store("KRKB",  &evaluate_KRKB);
	endgame_store("KBBKN", &evaluate_KBBKN);
	endgame_store("KNNK",  &evaluate_KNNK);
	endgame_store("KNNKP", &evaluate_KNNKP);
	endgame_store("KBNK",  &evaluate_KBNK);
	endgame_store("KQKP",  &evaluate_KQKP);
	endgame_store("KQKN",  &evaluate_KQKX);
	endgame_store("KQKB",  &evaluate_KQKX);
	endgame_store("KQKR",  &evaluate_KQKX);
	for (int color = 0; color < 2; color++) {
		endgame_KXK[color].evaluate = evaluate_KXK;
		endgame_KXK[color].strong_side = color;
	}
}
