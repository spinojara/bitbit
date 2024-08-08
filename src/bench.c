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

#include "bench.h"

#include "timeman.h"
#include "util.h"
#include "search.h"
#include "move.h"
#include "history.h"

static const char *fens[] = {
	"2kr3r/pp4p1/2bbq2p/2p1p2R/P1Pp2n1/1P1P1NP1/1B2BP2/R4K2 b - - 2 25",
	"r3kb1r/2p1ppp1/p2qb2p/1p6/3PPn2/P2P3P/2PBBPP1/R4RK1 w kq - 1 18",
	"r1bqk2r/2p1bppp/ppn2n2/3p3P/3P4/P5P1/1P2PPB1/RNBQK1NR w KQkq - 0 9",
	"2kr4/ppp3pp/2bbp3/4r3/8/PP2B2P/2P2KP1/R3R3 w - - 4 20",
	"R7/5kp1/7p/6r1/8/8/7K/5r2 w - - 4 37",
	"rq2kb1r/p2n1ppp/1p2p3/3n4/3P4/5N1P/PP1Q1PP1/RNB3KR b kq - 1 13",
	"k3r3/2pq1p2/prp3p1/2Rp3p/PP1P4/4PP2/3BQ3/1b1K3R w - - 2 34",
	"k3r3/2p2p2/prpq2p1/2Rp3p/PP1P4/4PP2/4Q3/1bBK3R w - - 4 35",
	"4r3/1k3p2/3q2p1/3p3p/1pbP4/4PP2/1Q5R/2BK4 w - - 0 45",
	"8/3k4/R2n2pp/P6b/1P2p3/2R1K3/6P1/6r1 w - - 5 48",
	"r1bqkb1r/1pp2p1p/5n2/4p1p1/3n4/P1Pp1P1P/NP1P2P1/R1BK2NR b kq - 0 12",
	"8/3Q3p/1p1bp1r1/3pk3/1P1N4/2P1P1P1/1P5P/6K1 w - - 1 26",
	"r5k1/pp4p1/2pp3r/4n2p/4p2P/PP4P1/3Q1P2/2R3K1 b - - 0 26",
	"r2qr1k1/1ppb1pp1/p6p/2Rn4/3P4/4P3/PP1BBPPP/3Q1RK1 b - - 0 17",
	"1Q6/p6r/3p3q/kpp1p2N/4P3/P2P4/1PP3P1/5RK1 w - - 7 34",
	"5k2/1p3p2/2p2p1R/8/8/6K1/r6P/8 b - - 2 45",
	"8/8/pp2r2p/3NPpk1/8/P7/1P3PPP/2R3K1 w - - 5 35",
	"2krr3/pb5p/nppb1q2/3p1p2/3P4/4BN1P/PP4P1/R2QRNK1 b - - 3 21",
	"8/8/8/5pp1/2r4p/4kP1P/1R2b1PK/8 w - - 16 60",
	"r3kb1r/pQ1n2p1/5n2/3p1p1p/8/4PN2/PP1P1PPP/R1B1K2R b KQkq - 0 12",
	"4R3/1pk5/2p5/p7/1q2p3/8/2P1K3/8 b - - 1 54",
	"1r5r/1p3k1p/p4n2/5Npb/8/7P/4qbB1/1R3R1K w - - 2 32",
	"4r3/1b2k3/1b2p1p1/1pp1n1p1/7p/RPP2P1P/P3N1P1/3R1K2 w - - 5 37",
	"8/R1b5/3kp1p1/1pp5/7p/1PP2b2/P4r2/2N1R1K1 b - - 9 46",
	"2q2b1r/6p1/2B2k2/rP3p1p/pp6/7P/5PP1/1R1R2K1 w - - 2 29",
	"r1b2rk1/ppp1bpp1/8/7P/N7/2BR4/PP2NP1P/4K2R b K - 0 17",
	"8/5R2/5P2/3K4/8/8/2k5/5r2 b - - 0 84",
	"r1b1k2r/pppp2pp/2nb4/4qP2/8/2PP4/PP2BPPP/R1BQK2R w KQkq - 1 11",
	"1k5r/3n1ppp/4b3/1B2Pn2/8/PP3N2/3NKPPP/R1R5 w - - 6 22",
	"r3kr2/p6p/5bp1/pR6/n3p2P/5bPB/2P2P2/2K1R1N1 b q - 3 37",
	"r6k/1R4p1/p1N2bQp/P7/1PP2P2/6P1/8/6K1 w - - 1 37",
	"8/8/5kp1/8/5K1P/1R6/8/5b2 w - - 19 71",
	"4k3/8/8/6K1/4b2P/8/3R4/8 b - - 2 95",
	"8/8/4R3/3r2N1/1k3P1p/1P3K2/5P2/8 w - - 10 48",
	"1r4k1/5p1p/2nbp1p1/4r3/P1B5/6PP/1PP2P2/1R1Q1K1R b - - 12 31",
	"8/1pp1k1p1/2b2p1p/2P1p3/1P2P2P/2B2P2/r4NP1/3BK2R b K h3 0 22",
	"8/8/R7/4k3/1n5P/8/2r5/7K w - - 0 51",
	"5rk1/6p1/1pn2qb1/2pp1p2/2r2P1R/4B2Q/6PP/4R1K1 b - - 1 31",
	"r1q4r/p1p3pk/1p6/8/8/1P2PP1P/Pn2K1P1/R4R2 w - - 0 21",
	"r3kb1r/ppp2p1p/3p4/4p3/2B1P1b1/6P1/PP1NNq1P/n1BK3R w kq - 4 15",
	"3q1b1r/p1r2pp1/1p2kn2/1B2N2p/1B6/P3P3/3P1PPP/1Q2K2R w K - 2 19",
	"r1b1k2r/ppp1np1p/2nqp3/3p4/4P3/NP1P1N2/P1P1QPPP/R3KB1R w KQkq - 1 9",
	"8/p1p2p2/1p1p1k2/3P4/2P2P1r/5q1p/1P5Q/K5R1 w - - 1 42",
	"2b1k2r/p3n3/p3p1p1/1r3pB1/Q1P5/5N2/Pb3PPP/3R1RK1 b k - 1 19",
	"3r3k/1p1n1R1p/4p3/p1Pp4/P2Pp3/1P2N1q1/4Q1P1/2R3K1 w - - 2 31",
	"8/4R3/8/p7/1p2Pkp1/1Pr5/P3K3/8 w - - 1 58",
	"1K6/1P6/6R1/2k5/8/4P1P1/7r/8 w - - 3 86",
	"r3kb1r/pp2pppp/2n5/5b2/6nN/1PN5/P2P1PPP/R1BQK2R b Kkq - 5 12",
	"5rk1/2R3p1/3p3p/8/4ppqP/6P1/6K1/4R3 w - - 0 42",
	"8/1p6/5b1k/2N4p/1p3p1n/8/4KP2/6R1 w - - 0 44",
	"2R5/6p1/p5k1/1b4rp/1p3p1Q/1P1PqP2/6P1/7K w - - 9 53",
	"3rk1r1/4bp2/p4n1p/1p3Np1/1PpB4/P6P/2PK1PP1/R6R b - - 2 21",
	"1r6/2p5/4p1kp/p7/P2PPpP1/B1n5/6K1/R7 w - - 1 37",
	"r3k2r/1pq2p2/p2bpn1p/3p1np1/3P4/P1P1BN2/1P3P1P/2R1Q1RK b kq - 9 19",
	"rnb1kbnr/pp6/2qpP1p1/1R6/4Pp2/4N1NP/P3BP2/2BQK2R w Kkq - 1 18",
	"3rbk2/QB2n3/3qpp2/2R3p1/2RP4/P5P1/2N4P/6K1 w - - 1 38",
	"1nbk1r2/1ppp1p1B/r4Np1/2P3N1/p2P4/8/PP2KPPP/R2R4 w - - 2 19",
	"7r/4R3/pkp4P/1p1p4/1P1P3K/P6B/8/8 w - - 5 61",
	"7r/1p2kpR1/2n1p3/1p1p4/3P4/P1PN1P2/1P3K2/8 b - - 2 30",
	"8/kp4p1/r7/2n5/2P2Rqp/6P1/5P2/2R3K1 b - - 7 47",
	"2r5/p6p/1p4k1/4P1N1/3B2pP/P7/P1P2PP1/1R2KB1R b K - 0 24",
	"3Q4/2K3k1/4p1p1/7p/1B3P2/2P3P1/8/4q3 w - - 4 51",
	"6r1/p7/kp6/3Q3p/8/8/P1P2PPP/4R1K1 b - - 0 28",
	"r2qkb2/1ppbn3/p1n1p1p1/3pPp1r/3P1Q2/N1P3PP/PP3PB1/R3K2R b KQq - 5 14",
};

static const int depth = 10;
static struct position pos;

void bench(struct transpositiontable *tt) {
	struct history h = { 0 };
	struct timeinfo ti = { 0 };
	timepoint_t total = 0;
	move_t move[2];
	for (unsigned i = 0; i < SIZE(fens); i++) {
		const char *fen = fens[i];
		pos_from_fen2(&pos, fen);
		printf("position fen %s\n", fen);
		printf("go depth %d\n", depth);
		timepoint_t before = time_now();
		search(&pos, depth, 1, &ti, move, tt, &h, 1);
		total += time_now() - before;
		print_bestmove(&pos, move[0], move[1]);
	}

	printf("time: %ld ms\n", total / TPPERMS);
}
