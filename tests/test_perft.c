#include "transposition.h"
#include "movegen.h"
#include "nnue.h"
#include "endgame.h"

void compare_keys(const struct position *pos, uint64_t zobrist_key, uint64_t endgame_key, int16_t accumulation[2][K_HALF_DIMENSIONS], int32_t psqtaccumulation[2][PSQT_BUCKETS]) {
	CU_ASSERT_EQUAL(pos->zobrist_key, zobrist_key);
	CU_ASSERT_EQUAL(pos->endgame_key, endgame_key);
	for (int color = 0; color < 2; color++) {
		for (int i = 0; i < K_HALF_DIMENSIONS; i++)
			CU_ASSERT_EQUAL(pos->accumulation[color][i], accumulation[color][i]);

		for (int i = 0; i < PSQT_BUCKETS; i++)
			CU_ASSERT_EQUAL(pos->psqtaccumulation[color][i], psqtaccumulation[color][i]);
	}
}

uint64_t perft_extra_checks(struct position *pos, int depth) {
	if (depth <= 0)
		return 0;

	move_t moves[MOVES_MAX];
	struct pstate pstate;
	pstate_init(pos, &pstate);
	movegen(pos, &pstate, moves, MOVETYPE_ALL);

	uint64_t nodes = 0, count;
	for (move_t *move = moves; *move; move++) {
		if (!legal(pos, &pstate, move))
			continue;
		if (depth == 1) {
			count = 1;
			nodes++;
		}
		else {
			uint64_t zobrist_key_before = pos->zobrist_key;
			uint64_t endgame_key_before = pos->endgame_key;
			int16_t accumulation_before[2][K_HALF_DIMENSIONS];
			int32_t psqtaccumulation_before[2][PSQT_BUCKETS];
			memcpy(accumulation_before, pos->accumulation, sizeof(accumulation_before));
			memcpy(psqtaccumulation_before, pos->psqtaccumulation, sizeof(psqtaccumulation_before));

			do_zobrist_key(pos, move);
			do_endgame_key(pos, move);
			do_move(pos, move);
			do_accumulator(pos, move);

			uint64_t zobrist_key_after = pos->zobrist_key;
			uint64_t endgame_key_after = pos->endgame_key;
			int16_t accumulation_after[2][K_HALF_DIMENSIONS];
			int32_t psqtaccumulation_after[2][PSQT_BUCKETS];
			memcpy(accumulation_after, pos->accumulation, sizeof(accumulation_after));
			memcpy(psqtaccumulation_after, pos->psqtaccumulation, sizeof(psqtaccumulation_after));

			refresh_zobrist_key(pos);
			refresh_endgame_key(pos);
			refresh_accumulator(pos, WHITE);
			refresh_accumulator(pos, BLACK);

			compare_keys(pos, zobrist_key_after, endgame_key_after, accumulation_after, psqtaccumulation_after);

			count = perft_extra_checks(pos, depth - 1);

			undo_zobrist_key(pos, move);
			undo_endgame_key(pos, move);
			undo_move(pos, move);
			undo_accumulator(pos, move);
			nodes += count;

			compare_keys(pos, zobrist_key_before, endgame_key_before, accumulation_before, psqtaccumulation_before);
		}
	}
	return nodes;
}

uint64_t perft_helper(const char *fen, int depth) {
	struct position pos;
	pos_from_fen2(&pos, fen);
	refresh_zobrist_key(&pos);
	refresh_endgame_key(&pos);
	refresh_accumulator(&pos, WHITE);
	refresh_accumulator(&pos, BLACK);
	return perft_extra_checks(&pos, depth);
}

void test_perft_1(void) {
	CU_ASSERT_EQUAL(perft_helper("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6), 119060324);
}

void test_perft_2(void) {
	CU_ASSERT_EQUAL(perft_helper("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 5), 193690690);
}

void test_perft_3(void) {
	CU_ASSERT_EQUAL(perft_helper("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7), 178633661);
}

void test_perft_4(void) {
	CU_ASSERT_EQUAL(perft_helper("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6), 706045033);
	CU_ASSERT_EQUAL(perft_helper("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 6), 706045033);
}

void test_perft_5(void) {
	CU_ASSERT_EQUAL(perft_helper("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5), 89941194);
}

void test_perft_6(void) {
	CU_ASSERT_EQUAL(perft_helper("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 5), 164075551);
}
