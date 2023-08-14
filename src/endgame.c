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
	for (int piece = pawn; piece <= queen; piece++)
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
		for (int piece = pawn; piece < king; piece++) {
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
			fprintf(stderr, "Endgame entry collision.\n");
			exit(1);
		}
		e = endgame_get(&pos);

		e->evaluate = evaluate;
		e->endgame_key = pos.endgame_key;
		e->strong_side = color;
	}
}

/* This function makes sure that the hashing of endgames is injective.
 * It takes a while.
 */
void endgame_test(void) {
	struct position pos = { 0 };
	size_t max_pieces[] = { 0, 9, 11, 11, 11, 10 };
	size_t max_index = 1;
	for (size_t i = pawn; i < king; i++)
		max_index *= max_pieces[i] * max_pieces[i];
	for (size_t index = 0; index < max_index; index++) {
		int pieces[2][7];
		pieces[white][king] = pieces[black][king] = 1;
		size_t denominator = 1;
		for (int color = 0; color < 2; color++) {
			int total = 1;
			for (int piece = pawn; piece < king; piece++) {
				pieces[color][piece] = (index / denominator) % max_pieces[piece];
				denominator *= max_pieces[piece];
				total += pieces[color][piece];
				if (total > 16)
					goto outer;
			}
		}

#if 0
		printf("HERE\n");
		for (int color = 0; color < 2; color++) {
			for (int piece = pawn; piece <= king; piece++) {
				printf("%d ", pieces[color][piece]);
			}
			printf("\n");
		}
#endif

#ifndef NDEBUG
		pos.piece[white][all] = pos.piece[black][all] = 0;
		for (int color = 0; color < 2; color++) {
			int sign = 2 * color - 1;
			int total = 0;
			for (int piece = pawn; piece <= king; piece++) {
				pos.piece[color][piece] = (1 << pieces[color][piece]) - 1;
				total += pieces[color][piece];
				pos.piece[color][piece] <<= 32 - sign * total - pieces[black][piece] * (color == black);
				pos.piece[color][all] |= pos.piece[color][piece];
			}
		}
		for (int color = 0; color < 2; color++) {
			int total = 0;
			for (int piece = pawn; piece <= king; piece++) {
				int pop = popcount(pos.piece[color][piece]);
				total += pop;
				assert(pop == pieces[color][piece]);
			}
			assert(total == (int)popcount(pos.piece[color][all]));
		}
		assert(!(pos.piece[white][all] & pos.piece[black][all]));
#endif

		refresh_endgame_key(&pos);
		struct endgame *e = endgame_probe(&pos);
		/* Asserts verify_material. */
		if (e) {
			for (int color = 0; color < 2; color++) {
				for (int piece = pawn; piece <= king; piece++) {
					printf("%ld ", popcount(pos.piece[color][piece]));
				}
				printf("\n");
			}
			endgame_evaluate(e, &pos);
			printf("OK\n");
		}

		if (index % (max_index / 100) == 0)
			printf("%ld%%\r", index / (max_index / 100));
outer:;
	}
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
	f = MIN(f, 7 - f);
	r = MIN(r, 7 - r);
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
	assert(verify_material(pos, strong_side, mv[pawn]));
	assert(verify_material(pos, weak_side, 0));
	int pawn_square = ctz(pos->piece[strong_side][pawn]);
	int r = rank_of(orient_horizontal(strong_side, pawn_square));
	int32_t eval = r;
	if (bitbase_KPK_probe(pos, strong_side) == BITBASE_WIN)
		eval += VALUE_WIN + material_value[pawn] - pos->halfmove;
	return eval;
}

int32_t evaluate_KPKP(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	UNUSED(weak_side);
	assert(verify_material(pos, strong_side, mv[pawn]));
	assert(verify_material(pos, weak_side, mv[pawn]));
	int pawn_white = ctz(pos->piece[white][pawn]);
	int y_white = rank_of(pawn_white);
	int pawn_black = orient_horizontal(black, ctz(pos->piece[black][pawn]));
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
	assert(verify_material(pos, strong_side, mv[bishop] + mv[pawn]));
	assert(verify_material(pos, weak_side, 0));
	int pawn_square = orient_horizontal(strong_side, ctz(pos->piece[strong_side][pawn]));
	int bishop_square = orient_horizontal(strong_side, ctz(pos->piece[strong_side][bishop]));
	int f = file_of(pawn_square);
	int r = rank_of(pawn_square);
	int promotion_square = f + 8 * 7;
	int32_t eval = r;
	/* We can probe bitbase_KPK directly because the bishop is simply ignored. */
	if ((0 < f && f < 7) || (same_colored_squares(bishop_square) & same_colored_squares(promotion_square)) ||
			bitbase_KPK_probe(pos, strong_side) == BITBASE_WIN)
		eval += VALUE_WIN + material_value[bishop] + material_value[pawn] - pos->halfmove;
	return eval;
}

int32_t evaluate_KRKP(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[rook]));
	assert(verify_material(pos, weak_side, mv[pawn]));
	int strong_king = ctz(pos->piece[strong_side][king]);
	int weak_king = ctz(pos->piece[weak_side][king]);
	int pawn_square = orient_horizontal(weak_side, ctz(pos->piece[weak_side][pawn]));
	int r = rank_of(pawn_square);
	int32_t eval = -r;
	unsigned p = bitbase_KRKP_probe(pos, strong_side);
	if (p == BITBASE_WIN)
		eval += VALUE_WIN + material_value[rook] - material_value[pawn] + push_toward(strong_king, weak_king) - pos->halfmove;
	else if (p == BITBASE_LOSE)
		eval -= VALUE_WIN + material_value[pawn] - material_value[rook] - pos->halfmove;
	return eval;
}

int32_t evaluate_KRKN(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[rook]));
	assert(verify_material(pos, weak_side, mv[knight]));
	int strong_king = ctz(pos->piece[strong_side][king]);
	int weak_king = ctz(pos->piece[weak_side][king]);
	int weak_knight = ctz(pos->piece[weak_side][knight]);
	return 2 * push_toward_edge(weak_king) + push_away(weak_king, weak_knight) + 3 * push_toward(weak_king, strong_king);
}

int32_t evaluate_KRKB(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[rook]));
	assert(verify_material(pos, weak_side, mv[bishop]));
	int weak_king = ctz(pos->piece[weak_side][king]);
	return push_toward_edge(weak_king);
}

/* Drawish because of 50 move rule. */
int32_t evaluate_KBBKN(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, 2 * mv[bishop]));
	assert(verify_material(pos, weak_side, mv[knight]));
	int strong_king = ctz(pos->piece[strong_side][king]);
	int weak_king = ctz(pos->piece[weak_side][king]);
	return push_toward(strong_king, weak_king) + 4 * push_toward_edge(weak_king);
}

int32_t evaluate_KNNK(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	UNUSED(weak_side);
	UNUSED(pos);
	assert(verify_material(pos, strong_side, 2 * mv[knight]));
	assert(verify_material(pos, weak_side, 0));
	return 0;
}

int32_t evaluate_KNNKP(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, 2 * mv[knight]));
	assert(verify_material(pos, weak_side, mv[pawn]));
	int square = ctz(pos->piece[weak_side][pawn]);
	int weak_king = ctz(pos->piece[weak_side][king]);
	int r = rank_of(orient_horizontal(weak_side, square));
	return 3 * push_toward_edge(weak_king) - 4 * r;
}

int32_t evaluate_KBNK(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[bishop] + mv[knight]));
	assert(verify_material(pos, weak_side, 0));
	int weak_king = ctz(pos->piece[weak_side][king]);
	int strong_king = ctz(pos->piece[strong_side][king]);
	uint64_t darkbishop = pos->piece[strong_side][bishop] & same_colored_squares(a1);
	int corner1 = darkbishop ? a1 : a8;
	int corner2 = darkbishop ? h8 : h1;
	int closest_corner = distance(weak_king, corner1) < distance(weak_king, corner2) ? corner1 : corner2;
	return VALUE_WIN + material_value[bishop] + material_value[knight] + push_toward(strong_king, weak_king) + 0x8 * push_toward(closest_corner, weak_king) - pos->halfmove;
}

int32_t evaluate_KQKP(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, strong_side, mv[queen]));
	assert(verify_material(pos, weak_side, mv[pawn]));
	int weak_king = ctz(pos->piece[weak_side][king]);
	int strong_king = ctz(pos->piece[strong_side][king]);
	int32_t eval = push_toward(strong_king, weak_king);

	int square = ctz(pos->piece[weak_side][pawn]);
	int r = rank_of(orient_horizontal(weak_side, square));
	/* r != 7th rank */
	if (r != 6 || distance(weak_king, square) != 1 ||
			((FILE_B | FILE_D | FILE_E | FILE_G) & pos->piece[weak_side][pawn]))
		eval += VALUE_WIN + material_value[queen] - material_value[rook] - pos->halfmove;
	return eval;
}

int32_t evaluate_KQKX(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	int piece = pos->piece[weak_side][knight] ? knight : pos->piece[weak_side][bishop] ? bishop : rook;
	int weak_king = ctz(pos->piece[weak_side][king]);
	int strong_king = ctz(pos->piece[strong_side][king]);
	assert(verify_material(pos, strong_side, mv[queen]));
	assert(verify_material(pos, weak_side, mv[piece]));
	return VALUE_WIN + material_value[queen] - material_value[piece] + push_toward(strong_king, weak_king) + 0x8 * push_toward_edge(weak_king) - pos->halfmove;
}

int32_t evaluate_KXK(const struct position *pos, int strong_side) {
	int weak_side = other_color(strong_side);
	assert(verify_material(pos, weak_side, 0));
	int weak_king = ctz(pos->piece[weak_side][king]);
	int strong_king = ctz(pos->piece[strong_side][king]);
	int32_t material = 0;
	for (int piece = pawn; piece < king; piece++)
		material += material_value[piece] * popcount(pos->piece[strong_side][piece]);
	return VALUE_WIN + material + push_toward(strong_king, weak_king) + 0x8 * push_toward_edge(weak_king) - pos->halfmove;
}

int is_KXK(const struct position *pos, int color) {
	return !(pos->piece[other_color(color)][all] ^ pos->piece[other_color(color)][king]) &&
			(pos->piece[color][rook] ||
			 pos->piece[color][queen] ||
			 (popcount(pos->piece[color][knight] | pos->piece[color][bishop]) >= 3));
}

/* Should be called before do_move. */
void do_endgame_key(struct position *pos, const move *m) {
	assert(*m);
	assert(pos->mailbox[move_from(m)]);
	int turn = pos->turn;
	int victim = uncolored_piece(pos->mailbox[move_to(m)]);
	if (victim) {
		int before = popcount(pos->piece[other_color(turn)][victim]);
		int after = before - 1;
		assert(before);
		pos->endgame_key ^= endgame_key(other_color(turn), victim, before);
		pos->endgame_key ^= endgame_key(other_color(turn), victim, after);
	}
	if (move_flag(m) == MOVE_EN_PASSANT) {
		int before = popcount(pos->piece[other_color(turn)][pawn]);
		int after = before - 1;
		assert(before);
		pos->endgame_key ^= endgame_key(other_color(turn), pawn, before);
		pos->endgame_key ^= endgame_key(other_color(turn), pawn, after);
	}
	if (move_flag(m) == MOVE_PROMOTION) {
		int before_pawn = popcount(pos->piece[turn][pawn]);
		int after_pawn = before_pawn - 1;
		assert(before_pawn);
		pos->endgame_key ^= endgame_key(turn, pawn, before_pawn);
		pos->endgame_key ^= endgame_key(turn, pawn, after_pawn);

		int piece = move_promote(m) + 2;
		int before = popcount(pos->piece[turn][piece]);
		int after = before + 1;
		pos->endgame_key ^= endgame_key(turn, piece, before);
		pos->endgame_key ^= endgame_key(turn, piece, after);
	}
}

/* Should be called before undo_move. */
void undo_endgame_key(struct position *pos, const move *m) {
	assert(*m);
	assert(!pos->mailbox[move_from(m)]);
	assert(pos->mailbox[move_to(m)]);
	int victim = move_capture(m);
	int turn = other_color(pos->turn);
	if (victim) {
		int before = popcount(pos->piece[other_color(turn)][victim]);
		int after = before + 1;
		pos->endgame_key ^= endgame_key(other_color(turn), victim, before);
		pos->endgame_key ^= endgame_key(other_color(turn), victim, after);
	}
	if (move_flag(m) == MOVE_EN_PASSANT) {
		int before = popcount(pos->piece[other_color(turn)][pawn]);
		int after = before + 1;
		pos->endgame_key ^= endgame_key(other_color(turn), pawn, before);
		pos->endgame_key ^= endgame_key(other_color(turn), pawn, after);
	}
	if (move_flag(m) == MOVE_PROMOTION) {
		int before_pawn = popcount(pos->piece[turn][pawn]);
		int after_pawn = before_pawn + 1;
		pos->endgame_key ^= endgame_key(turn, pawn, before_pawn);
		pos->endgame_key ^= endgame_key(turn, pawn, after_pawn);

		int piece = move_promote(m) + 2;
		int before = popcount(pos->piece[turn][piece]);
		int after = before - 1;
		pos->endgame_key ^= endgame_key(turn, piece, before);
		pos->endgame_key ^= endgame_key(turn, piece, after);
	}
}

void endgame_init(void) {
	uint64_t seed = start_seed;
	for (size_t i = 0; i < SIZE(endgame_keys); i++)
		endgame_keys[i] = xorshift64(&seed);
	
	endgame_store("KPK",   &evaluate_KPK);   /* done */
	endgame_store("KPKP",  &evaluate_KPKP);  /* done */
	endgame_store("KBPK",  &evaluate_KBPK);  /* done */
	endgame_store("KRKP",  &evaluate_KRKP);  /* done */
	endgame_store("KRKN",  &evaluate_KRKN);  /* done */
	endgame_store("KRKB",  &evaluate_KRKB);  /* done */
	endgame_store("KBBKN", &evaluate_KBBKN); /* done */
	endgame_store("KNNK",  &evaluate_KNNK);  /* done */
	endgame_store("KNNKP", &evaluate_KNNKP);
	endgame_store("KBNK",  &evaluate_KBNK);  /* done */
	endgame_store("KQKP",  &evaluate_KQKP);  /* done */
	endgame_store("KQKN",  &evaluate_KQKX);  /* done */
	endgame_store("KQKB",  &evaluate_KQKX);  /* done */
	endgame_store("KQKR",  &evaluate_KQKX);  /* done */
	for (int color = 0; color < 2; color++) {
		endgame_KXK[color].evaluate = evaluate_KXK;
		endgame_KXK[color].strong_side = color;
	}
}
