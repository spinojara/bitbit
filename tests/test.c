#include <CUnit/Basic.h>

#include "attackgen.h"
#include "bitboard.h"
#include "endgame.h"
#include "history.h"
#include "magicbitboard.h"
#include "moveorder.h"
#include "nnue.h"
#include "position.h"
#include "search.h"
#include "transposition.h"

#include "test_perft.c"
#include "test_repetition.c"
#include "test_sse.c"

int main(void) {
	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	search_init();
	moveorder_init();
	position_init();
	transposition_init();
	endgame_init();
	history_init();
	nnue_init();

	CU_pSuite pSuite = NULL;

	CU_initialize_registry();

	pSuite = CU_add_suite("Repetition", NULL, NULL);
	CU_add_test(pSuite, "No repetition", test_repetition_1);
	CU_add_test(pSuite, "No repetition because of castling", test_repetition_2);
	CU_add_test(pSuite, "Repetition with kings", test_repetition_3);
	CU_add_test(pSuite, "Repetition with knights", test_repetition_4);
	CU_add_test(pSuite, "Repetition with queens", test_repetition_5);
	CU_add_test(pSuite, "Repetition with bishops", test_repetition_6);
	CU_add_test(pSuite, "Repetition with rooks", test_repetition_7);

	pSuite = CU_add_suite("Perft", NULL, NULL);
	CU_add_test(pSuite, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", test_perft_1);
	CU_add_test(pSuite, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", test_perft_2);
	CU_add_test(pSuite, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", test_perft_3);
	CU_add_test(pSuite, "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", test_perft_4);
	CU_add_test(pSuite, "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", test_perft_5);
	CU_add_test(pSuite, "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", test_perft_6);

	pSuite = CU_add_suite("Static exchange evaluation", NULL, NULL);
	CU_add_test(pSuite, "SSE standard", test_sse_1);
	CU_add_test(pSuite, "SSE added pieces", test_sse_2);
	CU_add_test(pSuite, "SSE pins", test_sse_3);
	CU_add_test(pSuite, "SSE discovery", test_sse_4);

	CU_basic_set_mode(CU_BRM_NORMAL);
	CU_basic_run_tests();
	CU_basic_show_failures(CU_get_failure_list());
	printf("\n");

	CU_cleanup_registry();
	return CU_get_error();
}
