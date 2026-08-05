#include "movegen.h"
#include "position.h"
#include "movepicker.h"
#include "search.h"

static struct searchinfo sizero;
static struct searchstack sszero[7];
static uint64_t seed = 12374;

static move_t *lastmove(move_t *moves) {
	move_t *move;
	for (move = moves; *move; move++);
	return move == moves ? NULL : move - 1;
}

static int find_and_drop(move_t *moves, move_t move) {
	for (move_t *m = moves; *m; m++) {
		if (move_compare(*m, move)) {
			/* lm can never be NULL here. */
			move_t *lm = lastmove(moves);
			*m = *lm;
			*lm = 0;
			return 1;
		}
	}
	return 0;
}

static void perft_movepicker(struct position *pos, int depth) {
	if (depth <= 0)
		return;

	move_t allmoves[MOVES_MAX];
	move_t legalmoves[MOVES_MAX] = { 0 };
	struct pstate pstate;
	pstate_init(pos, &pstate);
	movegen(pos, &pstate, allmoves, MOVETYPE_ALL);
	size_t nmoves = 0;
	for (move_t *move = allmoves; *move; move++)
		if (legal(pos, &pstate, move))
			legalmoves[nmoves++] = *move;

	move_t ttmove = 0;
	move_t killer1 = 0;
	move_t killer2 = 0;
	move_t countermove = 0;

	if (nmoves) {
		ttmove = legalmoves[uniformint(&seed, 0, nmoves)];
		killer1 = legalmoves[uniformint(&seed, 0, nmoves)];
		killer2 = legalmoves[uniformint(&seed, 0, nmoves)];
		countermove = legalmoves[uniformint(&seed, 0, nmoves)];

		/* Killers can never be the same in the history. */
		if (killer1 == killer2)
			killer2 = 0;
	}

	struct movepicker mp = { 0 };
	movepicker_init(&mp, 0, pos, &pstate, ttmove, killer1, killer2, countermove, &sizero, &sszero[6]);

	move_t move;
	while ((move = next_move(&mp))) {
		if (!legal(pos, &pstate, &move))
			continue;

		CU_ASSERT_TRUE(find_and_drop(legalmoves, move));

		do_move(pos, &move);
		perft_movepicker(pos, depth - 1);
		undo_move(pos, &move);
	}

	/* legalmoves should now be empty. */
	CU_ASSERT_FALSE(*legalmoves);
}

static void movepicker_helper(const char *fen, int depth) {
	struct position pos;
	pos_from_fen2(&pos, fen);
	perft_movepicker(&pos, depth);
}

static void test_movepicker_1(void) {
	movepicker_helper("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4);
}

static void test_movepicker_2(void) {
	movepicker_helper("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 3);
}

static void test_movepicker_3(void) {
	movepicker_helper("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5);
}

static void test_movepicker_4(void) {
	movepicker_helper("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4);
	movepicker_helper("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4);
}

static void test_movepicker_5(void) {
	movepicker_helper("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3);
}

static void test_movepicker_6(void) {
	movepicker_helper("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3);
}
