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

#define _GNU_SOURCE
#include <sys/mman.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/param.h>
#include <signal.h>

#ifdef SYZYGY
#include <tbprobe.h>
#endif

#include "search.h"
#include "move.h"
#include "transposition.h"
#include "option.h"
#include "nnue.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "bitboard.h"
#include "moveorder.h"
#include "position.h"
#include "transposition.h"
#include "endgame.h"
#include "history.h"
#include "util.h"
#include "movegen.h"
#include "movepicker.h"
#include "timeman.h"
#include "io.h"
#include "polyglot.h"

const struct searchinfo gsi = { 0 };

static const char *prefix;

static long max_file_size = 1024 * 1024;
static int random_moves_min = 4;
static int random_moves_max = 8;

#if 0
static int32_t eval_limit = 1500;
#endif

static atomic_int s;

static pthread_mutex_t filemutex;

static const char *syzygy = NULL;
static const char *openings = NULL;
static int nosyzygy = 0;

static atomic_uint_fast64_t bytes;
static atomic_uint_fast64_t positions;

static inline void stop(void) {
	if (!atomic_load_explicit(&s, memory_order_relaxed))
		fprintf(stderr, "broadcasting stop signal...\n");
	atomic_store_explicit(&s, 1, memory_order_relaxed);
}

static inline int stopped(void) {
	return atomic_load_explicit(&s, memory_order_relaxed);
}

FILE *newfile(void) {
	if (stopped())
		return NULL;

	static long n = 1;
	static char date[64] = { 0 };

	pthread_mutex_lock(&filemutex);

	char name[BUFSIZ];
	char tmp[64];
	time_t t = time(NULL);
	struct tm tm;
	strftime(tmp, 64, "%Y%m%d", localtime_r(&t, &tm));
	if (strcmp(tmp, date)) {
		memcpy(date, tmp, 64);
		sprintf(name, "%s/selfplay-%s", prefix, date);

		errno = 0;
		if (mkdir(name, 0777) && errno != EEXIST) {
			fprintf(stderr, "error: failed to create directory %s\n", name);
			stop();
			pthread_mutex_unlock(&filemutex);
			return NULL;
		}

		n = 1;
	}

	int fd;
	for (fd = -1; n < LONG_MAX && fd == -1; n++) {
		sprintf(name, "%s/selfplay-%s/%ld.bit", prefix, date, n);
		errno = 0;
		fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd == -1 && errno != EEXIST) {
			fprintf(stderr, "error: failed to create file %s\n", name);
			stop();
			pthread_mutex_unlock(&filemutex);
			return NULL;
		}
	}

	pthread_mutex_unlock(&filemutex);

	return fd != -1 ? fdopen(fd, "wb") : NULL;
}

void custom_search(struct position *pos, uint64_t nodes, move_t moves[MOVES_MAX], int64_t evals[MOVES_MAX], struct transpositiontable *tt, struct history *history, uint64_t seed) {
	struct searchinfo si = { 0 };
	si.tt = tt;
	si.ti = NULL;
	si.history = history;
	si.max_nodes = nodes;
	si.hard_max_nodes = 5 * si.max_nodes;
	si.seed = seed;

	struct searchstack realss[PLY_MAX + 4] = { 0 };
	struct searchstack *ss = &realss[4];

	refresh_accumulator(pos, 0);
	refresh_accumulator(pos, 1);
	refresh_endgame_key(pos);
	refresh_zobrist_key(pos);

	for (int i = 0; i < MOVES_MAX; i++)
		evals[i] = -VALUE_INFINITE;
	movegen_legal(pos, moves, MOVETYPE_ALL);

	struct movepicker mp = { 0 };
	mp.move = moves;
	mp.eval = evals;

	int ply = 0;
	history_store(pos, si.history, ply);

	for (int depth = 1; depth <= PLY_MAX / 2 && !si.interrupt && si.nodes < si.max_nodes; depth++) {
		for (int i = 0; moves[i] && !si.interrupt; i++) {
			move_t *move = &moves[i];

			do_zobrist_key(pos, move);
			do_endgame_key(pos, move);
			do_move(pos, move);
			do_accumulator(pos, move);
			ss[0].move = *move;
			ss[0].continuation_history_entry = &(si.continuation_history[pos->mailbox[move_to(move)]][move_to(move)]);
			si.nodes++;

			int32_t eval = -negamax(pos, depth - 1, ply + 1, -VALUE_MATE, VALUE_MATE, 0, &si, ss + 1);
			if (!si.interrupt)
				evals[i] = eval;

			if (si.nodes >= si.max_nodes)
				si.interrupt = 1;

			undo_zobrist_key(pos, move);
			undo_endgame_key(pos, move);
			undo_move(pos, move);
			undo_accumulator(pos, move);

			if (si.interrupt && i > 0)
				moves[i] = 0;
		}

		si.done_depth = depth;

		sort_moves(&mp);
	}
}

int32_t evaluate_material(const struct position *pos) {
	int32_t eval = 0;
	for (int piece = PAWN; piece < KING; piece++)
		eval += popcount(pos->piece[WHITE][piece]) * material_value[piece];
	for (int piece = PAWN; piece < KING; piece++)
		eval -= popcount(pos->piece[BLACK][piece]) * material_value[piece];
	return pos->turn ? eval : -eval;
}

int32_t search_material(struct position *pos, int alpha, int beta) {
	uint64_t checkers = generate_checkers(pos, pos->turn);
	int32_t eval = evaluate_material(pos), best_eval = -VALUE_INFINITE;

	if (!checkers) {
		if (eval >= beta)
			return beta;
		if (eval > alpha)
			alpha = eval;
		best_eval = eval;
	}

	struct pstate pstate;
	pstate_init(pos, &pstate);
	struct movepicker mp;
	struct searchstack ss[10] = { 0 };
	movepicker_init(&mp, 1, pos, &pstate, 0, 0, 0, 0, &gsi, ss + 5);
	move_t move;
	while ((move = next_move(&mp))) {
		if (!legal(pos, &pstate, &move))
			continue;

		do_move(pos, &move);
		eval = -search_material(pos, -beta, -alpha);
		undo_move(pos, &move);

		if (eval > best_eval) {
			best_eval = eval;
			if (eval > alpha) {
				alpha = eval;
				if (eval >= beta)
					break;
			}
		}
	}

	return best_eval;
}

int filter_move(struct position *pos, move_t *move, int32_t eval) {
	UNUSED(pos);
	UNUSED(move);
	UNUSED(eval);
	int ret = 0;
#if 0
	int piece = uncolored_piece(pos->mailbox[move_from(move)]);
	if (piece == KING || piece == ROOK || piece == QUEEN)
		return 1;

	do_move(pos, move);
	if (search_material(pos, -VALUE_INFINITE, VALUE_INFINITE) != -eval) {
		ret = 1;
	}
	undo_move(pos, move);
	if (ret) {
		char movestr[16];
		char fen[128];
		printf("filtered move %s for position %s\n", move_str_algebraic(movestr, move), pos_to_fen(fen, pos));
	}
#endif
	return ret;
}

move_t random_move(struct position *pos, uint64_t *seed) {
	move_t moves[MOVES_MAX], filtered[MOVES_MAX] = { 0 };
	movegen_legal(pos, moves, MOVETYPE_ALL);

	int32_t eval = search_material(pos, -VALUE_INFINITE, VALUE_INFINITE);

	int nmoves = 0;
	for (int i = 0; moves[i]; i++) {
		move_t *move = &moves[i];
		if (!filter_move(pos, move, eval))
			filtered[nmoves++] = moves[i];
	}

	if (nmoves <= 1)
		return filtered[0];

	return filtered[uniformint(seed, 0, nmoves)];
}

void play_game(FILE *openingsfile, struct transpositiontable *tt, uint64_t nodes, uint64_t *seed, FILE *out) {
	move_t moves[MOVES_MAX];
	int64_t evals[MOVES_MAX];

	struct history h = { 0 };
	struct position pos;

	char result = RESULT_UNKNOWN;
	move_t move = 0;

	int move_value_diff_threshold = 50;

	struct endgame *e;

	int draw_counter = 0;

	startpos(&pos);
	if (openingsfile && !polyglot_explore(openingsfile, &pos, 6, seed)) {
		fprintf(stderr, "error: start position not found in opening book\n");
		stop();
		return;
	}

	int random_moves = uniformint(seed, random_moves_min, random_moves_max + 1);
	for (int i = 0; i < random_moves; i++) {
		move = random_move(&pos, seed);
		if (move)
			do_move(&pos, &move);
		else
			return;
	}

	movegen_legal(&pos, moves, MOVETYPE_ALL);
	if (!moves[0])
		return;

	history_reset(&pos, &h);
	refresh_endgame_key(&pos);
	refresh_zobrist_key(&pos);

	int32_t eval[POSITIONS_MAX] = { 0 };
	unsigned char flag[POSITIONS_MAX] = { 0 };

	int tb_draw = 0;

	while (1) {
		int is_check = generate_checkers(&pos, pos.turn) != 0;
		int tb_move = 0;

		movegen_legal(&pos, moves, MOVETYPE_ALL);
		if (!moves[0]) {
			result = is_check ? (pos.turn ? RESULT_LOSS : RESULT_WIN) : RESULT_DRAW;
			break;
		}

		h.zobrist_key[h.ply] = pos.zobrist_key;

		move_t *bestmove = NULL;
		eval[h.ply] = VALUE_NONE;

		if (popcount(all_pieces(&pos)) <= 2) {
			result = RESULT_DRAW;
			break;
		}
#ifdef SYZYGY
		else if (syzygy && popcount(all_pieces(&pos)) <= TB_LARGEST && !pos.castle) {
			uint64_t white = pos.piece[WHITE][ALL];
			uint64_t black = pos.piece[BLACK][ALL];
			uint64_t kings = pos.piece[WHITE][KING] | pos.piece[BLACK][KING];
			uint64_t queens = pos.piece[WHITE][QUEEN] | pos.piece[BLACK][QUEEN];
			uint64_t rooks = pos.piece[WHITE][ROOK] | pos.piece[BLACK][ROOK];
			uint64_t bishops = pos.piece[WHITE][BISHOP] | pos.piece[BLACK][BISHOP];
			uint64_t knights = pos.piece[WHITE][KNIGHT] | pos.piece[BLACK][KNIGHT];
			uint64_t pawns = pos.piece[WHITE][PAWN] | pos.piece[BLACK][PAWN];
			unsigned rule50 = 0;
			unsigned castling = 0;
			unsigned ep = 0;
			int turn = pos.turn;
			unsigned ret;
			ret = tb_probe_root(white, black, kings, queens, rooks, bishops, knights, pawns, rule50, castling, ep, turn, NULL);
			if (ret != TB_RESULT_FAILED) {
				unsigned wdl = TB_GET_WDL(ret);
				unsigned from = TB_GET_FROM(ret);
				unsigned to = TB_GET_TO(ret);
				unsigned promote = TB_GET_PROMOTES(ret);
				if (wdl == TB_DRAW)
					eval[h.ply] = 0;
				else if (wdl == TB_WIN || wdl == TB_CURSED_WIN)
					eval[h.ply] = VALUE_WIN;
				else if (wdl == TB_LOSS || wdl == TB_BLESSED_LOSS)
					eval[h.ply] = -VALUE_WIN;

				switch (promote) {
				case TB_PROMOTES_QUEEN:
					promote = 3;
					break;
				case TB_PROMOTES_ROOK:
					promote = 2;
					break;
				case TB_PROMOTES_BISHOP:
					promote = 1;
					break;
				case TB_PROMOTES_KNIGHT:
				default:
					promote = 0;
					break;
				}

				for (int i = 0; moves[i]; i++) {
					if (move_from(&moves[i]) == from && move_to(&moves[i]) == to && (move_flag(&moves[i]) != MOVE_PROMOTION || move_promote(&moves[i]) == promote)) {
						tb_move = 1;
						bestmove = &moves[i];
						break;
					}
				}
			}
			else {
				ret = tb_probe_wdl(white, black, kings, queens, rooks, bishops, knights, pawns, rule50, castling, ep, turn);
				if (ret != TB_RESULT_FAILED) {
					if (ret == TB_DRAW)
						eval[h.ply] = 0;
					else if (ret == TB_WIN || ret == TB_CURSED_WIN)
						eval[h.ply] = VALUE_WIN;
					else if (ret == TB_LOSS || ret == TB_BLESSED_LOSS)
						eval[h.ply] = -VALUE_WIN;
				}
			}
		}

		if (eval[h.ply] == 0)
			tb_draw++;
		else
			tb_draw = 0;
#endif
		if (eval[h.ply] == VALUE_NONE || !bestmove) {
			custom_search(&pos, nodes, moves, evals, tt, &h, *seed);
			bestmove = moves;
			if (eval[h.ply] == VALUE_NONE)
				eval[h.ply] = evals[0];
		}

		int32_t eval_now = eval[h.ply];

		if (eval_now != VALUE_NONE && abs(eval_now) <= 15 + 35 * (tb_draw > 0))
			draw_counter++;
		else if (eval_now != VALUE_NONE)
			draw_counter = 0;

		if (!*bestmove) {
			result = is_check ? (pos.turn ? RESULT_LOSS : RESULT_WIN) : RESULT_DRAW;
			break;
		}
		else if ((((h.ply >= 80 && draw_counter >= 10) || h.ply >= 240) && (tb_draw || !tb_move)) ||
				repetition(&pos, &h, 0, 2) || pos.halfmove >= 100) {
			result = RESULT_DRAW;
			if (eval_now != VALUE_NONE && !tb_draw) {
				int32_t v = eval_now * (2 * pos.turn - 1);
				if (v <= -400)
					result = RESULT_LOSS;
				else if (v >= 400)
					result = RESULT_WIN;
			}
			break;
		}

		int skip = is_capture(&pos, bestmove) || is_check || move_flag(bestmove);

		if (!skip && (e = endgame_probe(&pos))) {
			int32_t v = endgame_evaluate(e, &pos);
			result = RESULT_DRAW;
			if (v != VALUE_NONE && !tb_draw) {
				v *= 2 * pos.turn - 1;
				if (v > VALUE_WIN / 2)
					result = RESULT_WIN;
				else if (v < -VALUE_WIN / 2)
					result = RESULT_LOSS;
			}
			break;
		}

		int nmoves;
		for (nmoves = 1; moves[nmoves]; nmoves++)
			if (evals[nmoves] < eval_now - move_value_diff_threshold)
				break;

		move_value_diff_threshold = max(10, move_value_diff_threshold - 2);
#if 0
		skip = skip || !bernoulli(exp(-(double)pos.halfmove / 8.0), seed));
#endif

		if (skip)
			flag[h.ply] |= FLAG_SKIP;

		if (eval_now != VALUE_NONE && abs(eval_now) < VALUE_WIN && !tb_move) {
#if 0
			/* Density on (0,1) is f(x)=1.5-x so we are slightly
			 * more likely to pick better moves. */
			double r = 1.5 - sqrt(2.25 - 2.0 * uniform(seed));
#else
			double r = uniform(seed);
#endif
			r = MIN(MAX(r, 0.0), 0.999);
			move = moves[(int)(nmoves * r)];
		}
		else {
			move = *bestmove;
		}

		history_next(&pos, &h, move);
	}

	flag[h.ply] |= FLAG_SKIP;

	if (h.ply) {
		if (write_move(out, 0)) {
			fprintf(stderr, "error: failed to write move\n");
			stop();
		}
		if (write_position(out, &h.start)) {
			fprintf(stderr, "error: failed to write position\n");
			stop();
		}
		if (write_result(out, result)) {
			fprintf(stderr, "error: failed to write result\n");
			stop();
		}
		int count = 0;
		for (int i = 0; i <= h.ply; i++) {
			if (write_eval(out, eval[i])) {
				fprintf(stderr, "error: failed to write eval\n");
				stop();
			}
			if (write_flag(out, flag[i])) {
				fprintf(stderr, "error: failed to write flag\n");
				stop();
			}
			if (!(flag[i] & FLAG_SKIP) && eval[i] != VALUE_NONE)
				count++;
			if (i < h.ply) {
				if (write_move(out, h.move[i])) {
					fprintf(stderr, "error: failed to write move\n");
					stop();
				}
			}
		}
		atomic_fetch_add(&bytes, 69 + 5 * h.ply);
		atomic_fetch_add(&positions, count);
	}
}

struct threadinfo {
	int jobn;
	uint64_t seed;
	uint64_t nodes;
	size_t tt_size;
};

void *playthread(void *arg) {
	struct threadinfo *ti = (struct threadinfo *)arg;
	uint64_t seed = ti->seed;
	uint64_t nodes = ti->nodes;
	struct transpositiontable tt = { 0 };
	if (transposition_alloc(&tt, ti->tt_size)) {
		fprintf(stderr, "error: failed to allocate transposition table\n");
		stop();
	}

	FILE *openingsfile = openings ? fopen(openings, "rb") : NULL;
	if (!openingsfile && openings) {
		fprintf(stderr, "error: failed to open file %s\n", openings);
		stop();
	}

	while (!stopped()) {
		FILE *f = newfile();
		if (!f) {
			fprintf(stderr, "error: failed to create new file\n");
			stop();
			break;
		}

		while (ftell(f) < max_file_size && !stopped()) {
			play_game(openingsfile, &tt, nodes, &seed, f);
		}

		fclose(f);

	}

	transposition_free(&tt);
	if (openingsfile)
		fclose(openingsfile);

	printf("exiting thread %d\n", ti->jobn);

	return NULL;
}

static void sigint(int num) {
	stop();
	signal(num, &sigint);
}

int main(int argc, char **argv) {
	signal(SIGINT, &sigint);
	signal(SIGTERM, &sigint);

	atomic_init(&bytes, 0);
	atomic_init(&positions, 0);
	atomic_init(&s, 0);

	int jobs = 1;
	int tt_MiB = 6 * 1024;
	int nodes = 10000;
	static struct option opts[] = {
		{ "jobs",           required_argument, NULL, 'j' },
		{ "tt",             required_argument, NULL, 't' },
		{ "nodes",          required_argument, NULL, 'n' },
		{ "without-syzygy", no_argument,       NULL, 'w' },
		{ "syzygy",         required_argument, NULL, 'z' },
		{ "openings",       required_argument, NULL, 'o' },
	};

	char *endptr;
	int c, option_index = 0;
	int error = 0;
	while ((c = getopt_long(argc, argv, "j:t:n:wz:o:", opts, &option_index)) != -1) {
		switch (c) {
		case 't':
			errno = 0;
			tt_MiB = strtol(optarg, &endptr, 10);
			if (errno || *endptr != '\0' || tt_MiB <= 1024) {
				error = 1;
				fprintf(stderr, "error: bad argument: tt\n");
			}
			break;
		case 'j':
			errno = 0;
			jobs = strtol(optarg, &endptr, 10);
			if (errno || *endptr != '\0' || jobs < 1 || jobs > 1024) {
				error = 1;
				fprintf(stderr, "error: bad argument: jobs\n");
			}
			break;
		case 'n':
			errno = 0;
			nodes = strtoll(optarg, &endptr, 10);
			if (errno || *endptr != '\0' || nodes < 100) {
				error = 1;
				fprintf(stderr, "error: bad argument: nodes\n");
			}
			break;
		case 'z':
			syzygy = optarg;
			break;
		case 'w':
			nosyzygy = 1;
			break;
		case 'o':
			openings = optarg;
			break;
		default:
			error = 1;
			break;
		}
	}
	if (error)
		return 1;

	if (optind >= argc) {
		fprintf(stderr, "usage: %s datadir\n", argv[0]);
		return 2;
	}
	prefix = argv[optind];
	if (strlen(prefix) >= BUFSIZ - 1024) {
		fprintf(stderr, "error: strlen of datadir is too large (%ld)\n", strlen(prefix));
		return 3;
	}
	if (syzygy) {
#if SYZYGY
		if (!tb_init(syzygy)) {
			fprintf(stderr, "error: init for syzygy tablebases failed for path '%s'.\n", syzygy);
			return 4;
		}
		if (TB_LARGEST == 0) {
			fprintf(stderr, "error: no syzygy tablebases found for path '%s'.\n", syzygy);
			return 5;
		}
		printf("Running with syzygy tablebases for up to %d pieces.\n", TB_LARGEST);
#else
		fprintf(stderr, "error: syzygy tablebases not supported for this configuration.\n");
		return 6;
#endif
	}
	if (!syzygy && !nosyzygy) {
		fprintf(stderr, "error: either --syzygy or --without-syzygy must be set.\n");
		return 7;
	}

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

	option_history = 1;
	option_nnue = 1;
	option_pure_nnue = 1;
	option_transposition = 1;
	option_deterministic = 0;

	pthread_mutex_init(&filemutex, NULL);

	timepoint_t t = time_now();

	pthread_t *thread = malloc(jobs * sizeof(*thread));
	struct threadinfo *ti = malloc(jobs * sizeof(*ti));

	for (int i = 0; i < jobs; i++) {
		ti[i].jobn = i;
		ti[i].seed = t + i;
		ti[i].tt_size = 1024ll * 1024ll * tt_MiB / jobs;
		ti[i].nodes = nodes;

		pthread_create(&thread[i], NULL, &playthread, &ti[i]);
		printf("started thread %d\n", i);
	}

	/* Sleep here so that the threads have time to start up, this will give
	 * a more accurate speed estimation.
	 */
	sleep(1);
	int first = 1;
	timepoint_t last = 0;
	while (!stopped()) {
		sleep(1);
		timepoint_t now = time_now();
		timepoint_t elapsed = now - last;
		if (elapsed <= (first ? 3 : 20) * TPPERSEC)
			continue;

		uint64_t bytesnow = atomic_exchange(&bytes, 0);
		uint64_t positionsnow = atomic_exchange(&positions, 0);
		if (last) {
			first = 0;
			printf("\33[2K%ld fens/s (%ld bytes/s)\r", positionsnow * TPPERSEC / elapsed, bytesnow * TPPERSEC / elapsed);
			fflush(stdout);
		}
		last = now;
	}
	stop();

	for (int i = 0; i < jobs; i++)
		pthread_join(thread[i], NULL);

	pthread_mutex_destroy(&filemutex);

	free(ti);
	free(thread);

#if SYZYGY
	if (syzygy)
		tb_free();
#endif
}
