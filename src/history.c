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

#include "history.h"

#include <string.h>

#include "transposition.h"
#include "attackgen.h"
#include "bitboard.h"
#include "endgame.h"

#ifndef NDEBUG
int history_init_done = 0;
#endif

static uint64_t cuckoo[8192];
static unsigned char A[8192], B[8192];

void history_next(struct position *pos, struct history *h, move_t move) {
	h->zobrist_key[h->ply] = pos->zobrist_key;
	h->move[h->ply] = move;
	do_zobrist_key(pos, &h->move[h->ply]);
	do_endgame_key(pos, &h->move[h->ply]);
	do_move(pos, &h->move[h->ply]);
	h->ply++;
}

void history_previous(struct position *pos, struct history *h) {
	h->ply--;
	undo_zobrist_key(pos, &h->move[h->ply]);
	undo_endgame_key(pos, &h->move[h->ply]);
	undo_move(pos, &h->move[h->ply]);
}

void history_reset(const struct position *pos, struct history *h) {
	memset(h, 0, sizeof(*h));
	h->start = *pos;
}

void history_store(const struct position *pos, struct history *h, int ply) {
	if (!option_history)
		return;

	h->zobrist_key[h->ply + ply] = pos->zobrist_key;
}

int repetition(const struct position *pos, const struct history *h, int ply, int n) {
	if (!option_history || pos->halfmove < 4)
		return 0;

	int offset = h->ply + ply;
	int end = min(pos->halfmove, offset);

	assert(h->zobrist_key[offset] == pos->zobrist_key);

	for (int d = 4, count = 0; d <= end; d += 2)
		if (h->zobrist_key[offset - d] == pos->zobrist_key && ++count == n)
			return 1;
	return 0;
}

/* <https://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf>
 * (11+-8 Elo).
 */
#define H1(key) ((key) & 0x1FFF)
#define H2(key) (((key) >> 16) & 0x1FFF)

#define SWAP(x, y) {       \
	uint64_t z = (x);  \
	(x) = (y);         \
	(y) = z;           \
}

int upcoming_repetition(const struct position *pos, const struct history *h, int ply) {
	if (!option_history || pos->halfmove < 3)
		return 0;
	assert(history_init_done);

	int offset = h->ply + ply;
	int end = min(pos->halfmove, offset);

	uint64_t other = h->zobrist_key[offset] ^ h->zobrist_key[offset - 1] ^ zobrist_turn_key();

	for (int d = 3; d <= end; d += 2) {
		other ^= h->zobrist_key[offset - d] ^ h->zobrist_key[offset - (d - 1)] ^ zobrist_turn_key();
		if (other)
			continue;
		uint64_t diff = h->zobrist_key[offset] ^ h->zobrist_key[offset - d];
		uint64_t i;
		if (cuckoo[i = H1(diff)] == diff || cuckoo[i = H2(diff)] == diff) {
			/* We don't have to check that we are not capturing a piece.
			 * If we were, then that piece would have already been captured
			 * on the position in the h that we are comparing to.
			 */
			if (!(between(A[i], B[i]) & all_pieces(pos))) {
				if (ply > d || 1)
					return 1;

				if (color_of_piece(pos->mailbox[A[i]] ? pos->mailbox[A[i]] : pos->mailbox[B[i]]) != pos->turn)
					continue;

				for (int e = d + 4; e <= end; e += 2)
					if (h->zobrist_key[offset - e] == pos->zobrist_key)
						return 1;
			}
		}
	}

	return 0;
}

void history_init(void) {
	const int max_tries = 1000000;
	for (int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
	for (int a = a1; a <= h8; a++) {
	for (int b = a + 1; b <= h8; b++) {
		int upiece = uncolored_piece(piece);
		if (upiece == PAWN || !((attacks(upiece, a, 0, 0) & bitboard(b))))
			continue;
		uint64_t mv = zobrist_piece_key(piece, a) ^ zobrist_piece_key(piece, b) ^ zobrist_turn_key();
		uint64_t i = H1(mv);
		int aa = a, bb = b;
		int k;
		for (k = 0; k < max_tries; k++) {
			SWAP(cuckoo[i], mv);
			SWAP(A[i], aa);
			SWAP(B[i], bb);

			if (!mv)
				break;
			i = (i == H1(mv)) ? H2(mv) : H1(mv);
		}
		if (k == max_tries) {
			fprintf(stderr, "error: failed to initialize cuckoo tables\n");
			exit(1);
		}
	}
	}
	}

#ifndef NDEBUG
	history_init_done = 1;
#endif
}
