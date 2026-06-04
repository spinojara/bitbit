#include "position.h"
#include "move.h"
#include "moveorder.h"

int see_helper(const char *fen, const char *movestr, int geq) {
	struct position pos;
	CU_ASSERT_FATAL(fen_is_ok2(fen));
	pos_from_fen2(&pos, fen);
	move_t move = string_to_move(&pos, movestr);
	CU_ASSERT_FATAL(move);

	return see_geq(&pos, &move, geq);
}

void test_sse_1(void) {
	CU_ASSERT_TRUE(see_helper("k7/8/8/4n3/4R3/8/8/K7 w - - 0 1", "e4e5", 300));
	CU_ASSERT_FALSE(see_helper("k7/8/8/4n3/4R3/8/8/K7 w - - 0 1", "e4e5", 301));

	CU_ASSERT_TRUE(see_helper("k7/8/5p2/4n3/4R3/8/8/K7 w - - 0 1", "e4e5", -200));
	CU_ASSERT_FALSE(see_helper("k7/8/5p2/4n3/4R3/8/8/K7 w - - 0 1", "e4e5", -199));

	CU_ASSERT_TRUE(see_helper("k7/8/5q2/4n3/4R3/2B5/8/K7 w - - 0 1", "e4e5", 300));
	CU_ASSERT_FALSE(see_helper("k7/8/5q2/4n3/4R3/2B5/8/K7 w - - 0 1", "e4e5", 301));

	CU_ASSERT_TRUE(see_helper("k7/8/5q2/4n3/4R3/2B5/8/K7 w - - 0 1", "c3e5", 300));
	CU_ASSERT_FALSE(see_helper("k7/8/5q2/4n3/4R3/2B5/8/K7 w - - 0 1", "c3e5", 301));

	CU_ASSERT_TRUE(see_helper("k7/8/3p1q2/4n3/4R3/2B5/8/K7 w - - 0 1", "c3e5", 0));
	CU_ASSERT_FALSE(see_helper("k7/8/3p1q2/4n3/4R3/2B5/8/K7 w - - 0 1", "c3e5", 1));

	CU_ASSERT_TRUE(see_helper("k7/8/3p1q2/4n3/4R3/2B5/8/K7 w - - 0 1", "e4e5", -200));
	CU_ASSERT_FALSE(see_helper("k7/8/3p1q2/4n3/4R3/2B5/8/K7 w - - 0 1", "e4e5", -199));

	CU_ASSERT_TRUE(see_helper("k7/8/5q2/4n3/4R3/8/8/K7 w - - 0 1", "e4e5", -200));
	CU_ASSERT_FALSE(see_helper("k7/8/5q2/4n3/4R3/8/8/K7 w - - 0 1", "e4e5", -199));

	CU_ASSERT_TRUE(see_helper("k7/8/5q2/4n3/4R3/8/8/K3R3 w - - 0 1", "e4e5", 300));
	CU_ASSERT_FALSE(see_helper("k7/8/5q2/4n3/4R3/8/8/K3R3 w - - 0 1", "e4e5", 301));

	CU_ASSERT_TRUE(see_helper("k3r3/8/5q2/4n3/4R3/4R3/8/K7 w - - 0 1", "e4e5", -200));
	CU_ASSERT_FALSE(see_helper("k3r3/8/5q2/4n3/4R3/4R3/8/K7 w - - 0 1", "e4e5", -199));
}

void test_sse_2(void) {
	CU_ASSERT_TRUE(see_helper("k3r3/8/5q2/4n3/4R3/4R3/4R3/K7 w - - 0 1", "e4e5", 300));
	CU_ASSERT_FALSE(see_helper("k3r3/8/5q2/4n3/4R3/4R3/4R3/K7 w - - 0 1", "e4e5", 301));

	CU_ASSERT_TRUE(see_helper("k3r3/8/3b1q2/4n3/4R3/4R3/8/K7 w - - 0 1", "e4e5", -200));
	CU_ASSERT_FALSE(see_helper("k3r3/8/3b1q2/4n3/4R3/4R3/8/K7 w - - 0 1", "e4e5", -199));

	CU_ASSERT_TRUE(see_helper("k7/2B5/3b4/4n3/4R3/8/8/K7 w - - 0 1", "e4e5", 100));
	CU_ASSERT_FALSE(see_helper("k7/2B5/3b4/4n3/4R3/8/8/K7 w - - 0 1", "e4e5", 101));

	CU_ASSERT_TRUE(see_helper("k2q4/3r4/3p2r1/8/5B2/6B1/3Q4/K7 w - - 0 1", "f4d6", 0));
	CU_ASSERT_FALSE(see_helper("k2q4/3r4/3p2r1/8/5B2/6B1/3Q4/K7 w - - 0 1", "f4d6", 1));
}

void test_sse_3(void) {
	CU_ASSERT_TRUE(see_helper("k6b/3q4/3P4/4B3/8/8/8/K7 b - - 0 1", "d7d6", 100));
	CU_ASSERT_FALSE(see_helper("k6b/3q4/3P4/4B3/8/8/8/K7 b - - 0 1", "d7d6", 101));

	CU_ASSERT_TRUE(see_helper("k6b/3q4/3P4/4B3/8/8/8/K7 b - - 0 1", "d7d6", 100));
	CU_ASSERT_FALSE(see_helper("k6b/3q4/3P4/4B3/8/8/8/K7 b - - 0 1", "d7d6", 101));

	CU_ASSERT_TRUE(see_helper("k7/3q4/3P1q2/2Q1B3/8/8/8/K7 b - - 0 1", "d7d6", -800));
	CU_ASSERT_FALSE(see_helper("k7/3q4/3P1q2/2Q1B3/8/8/8/K7 b - - 0 1", "d7d6", -799));
}

void test_sse_4(void) {
	CU_ASSERT_TRUE(see_helper("k4b2/8/3p4/8/4N3/8/8/K6B w - - 0 1", "e4d6", 100));
	CU_ASSERT_FALSE(see_helper("k4b2/8/3p4/8/4N3/8/8/K6B w - - 0 1", "e4d6", 101));

	CU_ASSERT_TRUE(see_helper("5b2/8/2kp4/8/4N3/8/8/K6B w - - 0 1", "e4d6", -200));
	CU_ASSERT_FALSE(see_helper("5b2/8/2kp4/8/4N3/8/8/K6B w - - 0 1", "e4d6", -199));

	CU_ASSERT_TRUE(see_helper("k2r1b2/8/2Rp4/8/4N3/8/8/K7 w - - 0 1", "e4d6", -200));
	CU_ASSERT_FALSE(see_helper("k2r1b2/8/2Rp4/8/4N3/8/8/K7 w - - 0 1", "e4d6", -199));

	CU_ASSERT_TRUE(see_helper("k2r1b2/8/2Rp4/8/4N3/8/8/K6B w - - 0 1", "e4d6", 100));
	CU_ASSERT_FALSE(see_helper("k2r1b2/8/2Rp4/8/4N3/8/8/K6B w - - 0 1", "e4d6", 101));

	CU_ASSERT_TRUE(see_helper("5b2/8/2kp4/8/4N3/6B1/8/K6B w - - 0 1", "e4d6", 100));
	CU_ASSERT_FALSE(see_helper("5b2/8/2kp4/8/4N3/6B1/8/K6B w - - 0 1", "e4d6", 101));
}
