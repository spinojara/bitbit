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

#include "io.h"

#include <string.h>

#include "bitboard.h"

int write_uintx(FILE *f, uint64_t p, size_t x) {
	uint8_t buf[8];
	switch (x) {
	case 8:
		buf[7] = p >> 56;
		buf[6] = p >> 48;
		buf[5] = p >> 40;
		buf[4] = p >> 32;
		/* fallthrough */
	case 4:
		buf[3] = p >> 24;
		buf[2] = p >> 16;
		/* fallthrough */
	case 2:
		buf[1] = p >> 8;
		/* fallthrough */
	case 1:
		buf[0] = p;
		break;
	default:
		return 1;
	}

	return fwrite(buf, 1, x, f) != x;
}

int read_uintx(FILE *f, void *p, size_t x) {
	uint8_t buf[8];
	if (fread(buf, 1, x, f) != x)
		return 1;
	if (!p)
		return 0;
	switch (x) {
	case 1:
		*(uint8_t *)p = buf[0];
		break;
	case 2:
		*(uint16_t *)p = (uint16_t)buf[0] | (uint16_t)buf[1] << 8;
		break;
	case 4:
		*(uint32_t *)p = (uint32_t)buf[0] | (uint32_t)buf[1] << 8 | (uint32_t)buf[2] << 16 | (uint32_t)buf[3] << 24;
		break;
	case 8:
		*(uint64_t *)p = (uint64_t)buf[0] | (uint64_t)buf[1] << 8 | (uint64_t)buf[2] << 16 | (uint64_t)buf[3] << 24 |
			         (uint64_t)buf[4] << 32 | (uint64_t)buf[5] << 40 | (uint64_t)buf[6] << 48 | (uint64_t)buf[7] << 56;
		break;
	default:
		return 1;
	}
	return 0;
}

int write_position(FILE *f, const struct position *pos) {
	uint8_t buf[62] = { ctz(pos->piece[WHITE][KING]), ctz(pos->piece[BLACK][KING]) };
	int index = 2, sq;
	uint64_t b;

	for (int turn = 0; turn < 2; turn++) {
		for (int piece = PAWN; piece < KING; piece++) {
			if (index >= 62)
				break;
			b = pos->piece[turn][piece];
			int cpiece = colored_piece(piece, turn);
			while (b) {
				sq = ctz(b);
				buf[index++] = cpiece;
				buf[index++] = sq;
				b = clear_ls1b(b);
			}
		}
	}
	if (fwrite(buf, 1, 62, f) != 62)
		return 1;
	if (write_uintx(f, pos->turn ? 1 : 0, 1))
		return 1;
	if (write_uintx(f, 0 <= pos->en_passant ? pos->en_passant : 64, 1))
		return 1;
	if (write_uintx(f, pos->castle, 1))
		return 1;
	if (write_uintx(f, pos->halfmove, 1))
		return 1;
	if (write_uintx(f, pos->fullmove, 2))
		return 1;
	return 0;
}

int read_position(FILE *f, struct position *pos) {
	uint8_t buf[62];
	if (fread(buf, 1, 62, f) != 62)
		return 1;
	memset(pos->piece, 0, sizeof(pos->piece));
	memset(pos->mailbox, 0, sizeof(pos->mailbox));
	
	pos->piece[WHITE][KING] = bitboard(buf[0]);
	pos->piece[BLACK][KING] = bitboard(buf[1]);

	for (int index = 2; index <= 62; ) {
		int cpiece = buf[index++];
		if (!cpiece)
			break;
		int piece = uncolored_piece(cpiece);
		int color = color_of_piece(cpiece);
		int sq = buf[index++];

		pos->piece[color][piece] |= bitboard(sq);
		pos->piece[color][ALL] |= pos->piece[color][piece];
		pos->mailbox[sq] = cpiece;
	}
	uint8_t turn;
	if (read_uintx(f, &turn, 1))
		return 1;
	uint8_t en_passant;
	if (read_uintx(f, &en_passant, 1))
		return 1;
	uint8_t castle;
	if (read_uintx(f, &castle, 1))
		return 1;
	uint8_t halfmove;
	if (read_uintx(f, &halfmove, 1))
		return 1;
	uint16_t fullmove;
	if (read_uintx(f, &fullmove, 2))
		return 1;

	pos->turn = turn;
	pos->en_passant = en_passant < 64 ? en_passant : -1;
	pos->castle = castle;
	pos->halfmove = halfmove;
	pos->fullmove = fullmove;

	return 0;
}

int write_move(FILE *f, move_t move) {
	return write_uintx(f, move, 2);
}

int read_move(FILE *f, move_t *move) {
	uint16_t temp;
	if (read_uintx(f, &temp, 2))
		return 1;
	*move = temp;
	return 0;
}

int write_eval(FILE *f, int32_t eval) {
	return write_uintx(f, eval, 4);
}

int read_eval(FILE *f, int32_t *eval) {
	union {
		int32_t eval;
		uint32_t ueval;
	} t;
	if (read_uintx(f, &t.ueval, 4))
		return 1;

	*eval = t.eval;
	return 0;
}
