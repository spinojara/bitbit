#include <string.h>

#include "history.h"
#include "position.h"

int repetition_helper(const char *str, int upcoming_rep, int repetition_once, int repetition_twice) {
	struct position pos;
	struct history h;

	char *strcopy = malloc(strlen(str) + 1);
	strcpy(strcopy, str);

	char *moves = strstr(strcopy, "moves");
	moves[-1]   = '\0';

	if (!strcmp(strcopy, "startpos"))
		startpos(&pos);
	else
		pos_from_fen2(&pos, strcopy);

	history_reset(&pos, &h);

	strtok(moves, " ");
	const char *movestr;
	while ((movestr = strtok(NULL, " "))) {
		move_t move = string_to_move(&pos, movestr);
		history_next(&pos, &h, move);
	}

	free(strcopy);

	history_store(&pos, &h, 0);

	CU_ASSERT_EQUAL(upcoming_repetition(&pos, &h, 0), upcoming_rep);
	CU_ASSERT_EQUAL(repetition(&pos, &h, 0, 1), repetition_once);

	CU_ASSERT_EQUAL(repetition(&pos, &h, 0, 2), repetition_twice);
	return 0;
}

void test_repetition_1(void) {
	repetition_helper("startpos moves e2e4 c7c5 g1f3 b8c6 f1b5 g7g6 e1g1 f8g7 b1c3 e7e5 b5c4 g8e7", 0, 0, 0);
}

void test_repetition_2(void) { repetition_helper("startpos moves e2e4 e7e5 e1e2 e8e7 e2e1", 0, 0, 0); }

void test_repetition_3(void) {
	repetition_helper("startpos moves e2e4 e7e5 e1e2 e8e7 e2e1 e7e8", 0, 0, 0);
	repetition_helper("startpos moves e2e4 e7e5 e1e2 e8e7 e2e1 e7e8 e1e2", 1, 0, 0);
	repetition_helper("startpos moves e2e4 e7e5 e1e2 e8e7 e2e1 e7e8 e1e2", 1, 0, 0);
	repetition_helper("startpos moves e2e4 e7e5 e1e2 e8e7 e2e1 e7e8 e1e2 e8e7", 1, 1, 0);
	repetition_helper("startpos moves e2e4 e7e5 e1e2 e8e7 e2e1 e7e8 e1e2 e8e7 e2e1 e7e8 e1e2", 1, 1, 0);
	repetition_helper("startpos moves e2e4 e7e5 e1e2 e8e7 e2e1 e7e8 e1e2 e8e7 e2e1 e7e8 e1e2 e8e7", 1, 1, 1);
}

void test_repetition_4(void) {
	repetition_helper("startpos moves g1f3 b8c6", 0, 0, 0);
	repetition_helper("startpos moves g1f3 b8c6 f3g1", 1, 0, 0);
}

void test_repetition_5(void) {
	repetition_helper("rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 2 moves c8h3 d1d3", 0, 0, 0);
	repetition_helper("rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 2 moves c8h3 d1d3 h3c8", 1, 0, 0);
}

void test_repetition_6(void) {
	repetition_helper("rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 2 moves c8h3 d1d3 h3d7", 0, 0, 0);
	repetition_helper("rnbqkbnr/ppp1pppp/8/3p4/3P4/8/PPP1PPPP/RNBQKBNR b KQkq - 0 2 moves c8h3 d1d3 h3d7 d3d1", 1,
	                  0, 0);
}

void test_repetition_7(void) {
	repetition_helper("rnbqkbnr/1ppppppp/8/p7/P7/8/1PPPPPPP/RNBQKBNR w Kk - 0 2 moves a1a3 a8a6 a3h3", 0, 0, 0);
	repetition_helper("rnbqkbnr/1ppppppp/8/p7/P7/8/1PPPPPPP/RNBQKBNR w Kk - 0 2 moves a1a3 a8a6 a3h3 a6a8", 1, 0,
	                  0);
}
