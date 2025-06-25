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

#include <stdio.h>
#include <stdlib.h>

#include "move.h"
#include "position.h"
#include "evaluate.h"
#include "util.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "option.h"
#include "endgame.h"
#include "kpk.h"
#include "io.h"
#include "nnue.h"

void store_information(struct position *pos, uint64_t piece_square[7][64]) {
	for (int color = 0; color < 2; color++) {
		for (int piece = PAWN; piece <= KING; piece++) {
			uint64_t b = pos->piece[color][piece];
			while (b) {
				int square = ctz(b);
				square = orient_horizontal(color, square);
				piece_square[piece][square]++;
				b = clear_ls1b(b);
			}
		}
	}
}

void print_information(uint64_t square[64], uint64_t total) {
	for (int r = 7; r >= 0; r--) {
		printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n|");
		for (int f = 0; f < 8; f++) {
			int sq = make_square(f, r);
			printf(" %5.2f |", 100.f * square[sq] / (2 * total));
		}
		printf("\n");
	}
	printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");
	printf("\n");
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("provide a filename\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		printf("could not open %s\n", argv[1]);
		return 2;
	}

	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	position_init();
	struct position pos = { 0 };
	startpos(&pos);

	uint64_t piece_square[7][64] = { 0 };

	move_t move;
	char result = 0;
	int32_t eval = 0;
	size_t total = 0;
	size_t count = 0;
	size_t games = 0;
	size_t draws = 0;
	char startfen[128] = { 0 };
	char fen[128];
	char movestr[16];
	int print_flag = 0;
	unsigned char flag;
	while (1) {
		count++;
		if (count % 20000 == 0)
			printf("collecting data: %lu\r", count);
		move = 0;
		if (read_move(f, &move))
			break;
		if (move) {
			if (print_flag)
				printf("%s\n", move_str_pgn(movestr, &pos, &move));
			print_flag = 0;
			struct pstate ps;
			pstate_init(&pos, &ps);
			if (!pseudo_legal(&pos, &ps, &move) || !legal(&pos, &ps, &move)) {
				fprintf(stderr, "error: illegal move %s for position %s\n", move_str_algebraic(movestr, &move), pos_to_fen(fen, &pos));
				exit(1);
			}
			do_move(&pos, &move);
		}
		else {
			if (read_position(f, &pos))
				break;
			if (read_result(f, &result))
				break;
			if (!feof(f))
				games++;
			pos_to_fen(startfen, &pos);
		}

		if (read_eval(f, &eval) || read_flag(f, &flag))
			break;
		if (feof(f))
			break;

		if (eval == VALUE_NONE || flag & FLAG_SKIP)
			continue;
		if (popcount(all_pieces(&pos)) < 6 && ((result == RESULT_LOSS && (2 * pos.turn - 1) * eval >= VALUE_WIN) || (result == RESULT_WIN && (2 * pos.turn - 1) * eval <= -VALUE_WIN))) {
			print_position(&pos);
			print_fen(&pos);
			printf("eval: %d\n", eval);
			printf("result: %s\n", result == RESULT_DRAW ? "draw" : result == RESULT_LOSS ? "black wins" : result == RESULT_WIN ? "white wins" : "unknown");
		}
#if 0
		const int material_values[] = { 0, 1, 3, 3, 5, 9, 0 };
		int material[2] = { 0 };
		for (int color = 0; color < 2; color++) {
			for (int piece = PAWN; piece < KING; piece++) {
				uint64_t b = pos.piece[color][piece];
				material[color] += material_values[piece] * popcount(b);
			}
		}

#if 0
		if ((material[WHITE] == 4 && material[BLACK] == 0) ||
				(material[WHITE] == 0 && material[BLACK] == 4)) {
			int bitbase = bitbase_KPK_probe(&pos, pos.turn);
			print_position(&pos);
			print_fen(&pos);
			printf("start: %s\n", startfen);
			printf("bitbase: %d\n", bitbase);
			printf("eval: %d\n", eval);
		}
#endif
#if 1
		int material_delta = abs(material[WHITE] - material[BLACK]);

		if (material_delta >= 3 && eval == 0 && pos.halfmove <= 0) {
			print_position(&pos);
			print_fen(&pos);
			printf("start: %s\n", startfen);
			printf("%d\n", (2 * pos.turn - 1) * evaluate_classical(&pos));
			printf("%d\n", pos.turn ? eval : -eval);
			print_flag = 1;
			if (eval == 0)
				c++;
			a++;
		}
#endif
#endif
		store_information(&pos, piece_square);
		total++;

		if (result == RESULT_DRAW)
			draws++;
	}
	printf("\033[2K");
	printf("total positions: %lu\n", total);
	printf("total games: %lu\n", games);
	printf("draw percent: %lg\n", (double)draws / total);
	for (int piece = PAWN; piece <= KING; piece++)
		print_information(piece_square[piece], total);

	fclose(f);
}
