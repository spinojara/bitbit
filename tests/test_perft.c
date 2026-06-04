#include "perft.h"
#include "transposition.h"

uint64_t perft_helper(const char *fen, int depth) {
	struct position pos;
	pos_from_fen2(&pos, fen);
	refresh_zobrist_key(&pos);
	return perft(&pos, depth, 0);
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
