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

#define RANKED_WIN(rank) (VALUE_WIN + 0x100 * (rank))

uint64_t endgame_keys[2 * 6 * 11];
struct endgame endgame_table[ENDGAMESIZE] = { 0 };
struct endgame endgame_KXK[2] = { 0 };

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

void endgame_store(const char *str, int32_t (*evaluate)(const struct position *pos, int strongside)) {
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
		if (!strcmp(strong, weak) && color)
			break;
		char *ptr = color ? weak : strong;
		for (size_t i = 0; i < strlen(ptr); i++)
			ptr[i] = tolower(ptr[i]);

		char pieces[128];
		sprintf(pieces, "%s%ld/8/8/8/8/8/8/%s%ld", strong, 8 - strlen(strong), weak, 8 - strlen(weak));

		char *fen[] = { pieces, "w", "-", "-", "0", "1", };
		pos_from_fen(&pos, 6, fen);
		refresh_endgame_key(&pos);
		
		struct endgame *e = endgame_get(&pos);
		if (e->evaluate) {
			fprintf(stderr, "Endgame entry collision\n");
			exit(1);
		}

		e->evaluate = evaluate;
		e->endgame_key = pos.endgame_key;
		e->strongside = color;
	}
}

/* This function makes sure that the hashing of endgames is injective.
 * It takes a while.
 */
void endgame_test(void) {
	struct position pos;
	size_t max_pieces[] = { 0, 9, 11, 11, 11, 10 };
	size_t max_index = 1;
	size_t KXK = 0;
	for (size_t i = pawn; i < king; i++)
		max_index *= max_pieces[i] * max_pieces[i];
	for (size_t index = 0; index < max_index; index++) {
		int pieces[2][7];
		size_t denominator = 1;
		for (int color = 0; color < 2; color++) {
			for (int piece = pawn; piece < king; piece++) {
				pieces[color][piece] = (index / denominator) % max_pieces[piece];
				denominator *= max_pieces[piece];
			}
		}

		for (int color = 0; color < 2; color++)
			for (int piece = pawn; piece < king; piece++)
				pos.piece[color][piece] = (1 << pieces[color][piece]) - 1;

		refresh_endgame_key(&pos);

		struct endgame *e = endgame_probe(&pos);
		if (e && !is_KXK(&pos, black) && !is_KXK(&pos, white)) {
			printf("\n");
			for (int color = 0; color < 2; color++) {
				for (int piece = pawn; piece < king; piece++) {
					printf("%2d ", pieces[color][piece]);
				}
				printf("\n");
			}
		}
		else if (e) {
			KXK++;
		}

		if (index % (max_index / 100) == 0)
			printf("%ld%%\r", index / (max_index / 100));
	}
	printf("Total KXK: %ld\n", KXK);
}

static inline int32_t push_toward_edge(int square) {
	int x = square % 8;
	int y = square / 8;
	x = MIN(x, 7 - x);
	y = MIN(y, 7 - y);
	return 3 - MIN(x, y);
}

static inline int32_t push_toward_corner(int square) {
	int x = square % 8;
	int y = square / 8;
	x = MIN(x, 7 - x);
	y = MIN(y, 7 - y);
	return 6 - (x + y);
}

static inline int32_t push_toward(int square1, int square2) {
	return 7 - distance(square1, square2);
}

static inline int32_t push_away(int square1, int square2) {
	return distance(square1, square2);
}

int32_t evaluate_draw(const struct position *pos, int strongside) {
	int weakside = 1 - strongside;
	UNUSED(pos);
	UNUSED(weakside);
	return 0;
}

int32_t evaluate_KBNK(const struct position *pos, int strongside) {
	int weakside = 1 - strongside;
	UNUSED(pos);
	int weak_king = ctz(pos->piece[weakside][king]);
	int strong_king = ctz(pos->piece[strongside][king]);
	uint64_t darkbishop = pos->piece[strongside][bishop] & same_colored_squares(a1);
	int corner1 = darkbishop ? a1 : a8;
	int corner2 = darkbishop ? h8 : h1;
	int closest_corner = distance(weak_king, corner1) < distance(weak_king, corner2) ? corner1 : corner2;
	return RANKED_WIN(0) + push_toward(strong_king, weak_king) + 0x8 * push_toward(closest_corner, weak_king) - pos->halfmove;
}

int32_t evaluate_KBBK(const struct position *pos, int strongside) {
	int weakside = 1 - strongside;
	UNUSED(pos);
	int weak_king = ctz(pos->piece[weakside][king]);
	int strong_king = ctz(pos->piece[strongside][king]);
	return RANKED_WIN(1) + push_toward(strong_king, weak_king) + 0x8 * push_toward_corner(weak_king) - pos->halfmove;
}

int32_t evaluate_KXK(const struct position *pos, int strongside) {
	int weakside = 1 - strongside;
	UNUSED(pos);
	int weak_king = ctz(pos->piece[weakside][king]);
	int strong_king = ctz(pos->piece[strongside][king]);
	int32_t material = 0;
	for (int color = 0; color < 2; color++) {
		int32_t sign = 2 * (color == strongside) - 1;
		for (int piece = pawn; piece < king; piece++)
			material += sign * 100 * material_value[piece] * popcount(pos->piece[color][piece]);
	}
	return RANKED_WIN(2) + material + push_toward(strong_king, weak_king) + 0x8 * push_toward_corner(weak_king) - pos->halfmove;
}

int is_KXK(const struct position *pos, int color) {
	return !(pos->piece[1 - color][all] ^ pos->piece[1 - color][king]) &&
			(pos->piece[color][rook] ||
			 pos->piece[color][queen] ||
			 (popcount(pos->piece[color][knight] | pos->piece[color][bishop]) >= 3));
}

/* Should be called before do_move. */
void do_endgame_key(struct position *pos, const move *m) {
	assert(*m);
	assert(pos->mailbox[move_from(m)]);
	int turn = pos->turn;
	int victim = pos->mailbox[move_to(m)] % 6;
	if (victim) {
		int before = popcount(pos->piece[1 - turn][victim]);
		int after = before - 1;
		assert(before);
		pos->endgame_key ^= endgame_key(1 - turn, victim, before);
		pos->endgame_key ^= endgame_key(1 - turn, victim, after);
	}
	if (move_flag(m) == MOVE_EN_PASSANT) {
		int before = popcount(pos->piece[1 - turn][pawn]);
		int after = before - 1;
		assert(before);
		pos->endgame_key ^= endgame_key(1 - turn, pawn, before);
		pos->endgame_key ^= endgame_key(1 - turn, pawn, after);
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
	int turn = 1 - pos->turn;
	if (victim) {
		int before = popcount(pos->piece[1 - turn][victim]);
		int after = before + 1;
		pos->endgame_key ^= endgame_key(1 - turn, victim, before);
		pos->endgame_key ^= endgame_key(1 - turn, victim, after);
	}
	if (move_flag(m) == MOVE_EN_PASSANT) {
		int before = popcount(pos->piece[1 - turn][pawn]);
		int after = before + 1;
		pos->endgame_key ^= endgame_key(1 - turn, pawn, before);
		pos->endgame_key ^= endgame_key(1 - turn, pawn, after);
	}
	if (move_flag(m) == MOVE_PROMOTION) {
		int before_pawn = popcount(pos->piece[turn][pawn]);
		int after_pawn = before_pawn + 1;
		assert(before_pawn);
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
	uint64_t seed = 1274012836ull;
	for (size_t i = 0; i < SIZE(endgame_keys); i++)
		endgame_keys[i] = xorshift64(&seed);
	
	endgame_store("KNNK", &evaluate_draw);
	endgame_store("KBNK", &evaluate_KBNK);
	endgame_store("KBBK", &evaluate_KBBK);
	for (int color = 0; color < 2; color++) {
		endgame_KXK[color].evaluate = evaluate_KXK;
		endgame_KXK[color].strongside = color;
	}
}
