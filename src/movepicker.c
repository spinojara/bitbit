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

#include "movepicker.h"

#include <assert.h>

#include "moveorder.h"
#include "bitboard.h"
#include "movegen.h"
#include "search.h"
#include "util.h"

#ifdef TUNE
#define CONST
#else
#define CONST const
#endif

CONST int from_attack = 10000;
CONST int into_attack = 15000;
CONST int check_threat = 28577;

CONST double mvv_lva_factor = 4.83;
CONST double continuation_history_factor = 8.08;

CONST int goodquiet_threshold = 5715;

static inline int good_capture(struct position *pos, move_t *move, int threshold) {
	return (is_capture(pos, move) || move_flag(move) == MOVE_EN_PASSANT) && see_geq(pos, move, threshold);
}

static inline int promotion(struct position *pos, move_t *move, int threshold) {
	UNUSED(pos);
	return move_flag(move) == MOVE_PROMOTION && (int)move_promote(move) + 2 >= threshold;
}

int find_next(struct movepicker *mp, int (*filter)(struct position *, move_t *, int), int threshold) {
	for (int i = 0; mp->move[i]; i++) {
		if (filter(mp->pos, &mp->move[i], threshold)) {
			for (int j = i - 1; j >= 0; j--) {
				move_t move = mp->move[j + 1];
				mp->move[j + 1] = mp->move[j];
				mp->move[j] = move;
			}
			return 1;
		}
	}
	return 0;
}

void sort_moves(struct movepicker *mp) {
	if (!mp->move[0])
		return;
	for (int i = 1; mp->move[i]; i++) {
		move_t move = mp->move[i];
		int64_t eval = mp->eval[i];
		int j;
		for (j = i - 1; j >= 0 && mp->eval[j] < eval; j--) {
			mp->move[j + 1] = mp->move[j];
			mp->eval[j + 1] = mp->eval[j];
		}
		mp->move[j + 1] = move;
		mp->eval[j + 1] = eval;
	}
}

void evaluate_nonquiet(struct movepicker *mp) {
	for (int i = 0; mp->move[i]; i++) {
		move_t *move = &mp->move[i];
		int square_from = move_from(move);
		int square_to = move_to(move);
		int attacker = mp->pos->mailbox[square_from];
		int victim = move_flag(move) == MOVE_EN_PASSANT ? PAWN : uncolored_piece(mp->pos->mailbox[square_to]);
		mp->eval[i] = mp->si->capture_history[attacker][victim][square_to] / 512;
#ifdef TUNE
		mp->eval[i] += mvv_lva_factor * (victim ? mvv_lva(uncolored_piece(attacker), victim) : 0);
#else
		mp->eval[i] += 5 * (victim ? mvv_lva(uncolored_piece(attacker), victim) : 0);
#endif
	}
}

void evaluate_quiet(struct movepicker *mp) {
	uint64_t attacked[7] = { 0 };
	attacked[KNIGHT] = attacked[BISHOP] = mp->pstate->attacked[PAWN];
	attacked[ROOK] = attacked[BISHOP] | mp->pstate->attacked[KNIGHT] | mp->pstate->attacked[BISHOP];
	attacked[QUEEN] = attacked[ROOK] | mp->pstate->attacked[ROOK];

	for (int i = 0; mp->move[i]; i++) {
		move_t *move = &mp->move[i];
		int from_square = move_from(move);
		uint64_t from = bitboard(from_square);
		int to_square = move_to(move);
		uint64_t to = bitboard(to_square);
		int attacker = mp->pos->mailbox[from_square];
		mp->eval[i] = mp->si->quiet_history[attacker][from_square][to_square];
		if ((mp->ss - 1)->move)
#ifdef TUNE
			mp->eval[i] += continuation_history_factor * (*(mp->ss - 1)->continuation_history_entry)[attacker][to_square];
#else
			mp->eval[i] += 8 * (*(mp->ss - 1)->continuation_history_entry)[attacker][to_square];
#endif
		if ((mp->ss - 2)->move)
#ifdef TUNE
			mp->eval[i] += continuation_history_factor * (*(mp->ss - 2)->continuation_history_entry)[attacker][to_square];
#else
			mp->eval[i] += 8 * (*(mp->ss - 2)->continuation_history_entry)[attacker][to_square];
#endif
		if ((mp->ss - 4)->move)
#ifdef TUNE
			mp->eval[i] += continuation_history_factor * (*(mp->ss - 4)->continuation_history_entry)[attacker][to_square];
#else
			mp->eval[i] += 8 * (*(mp->ss - 4)->continuation_history_entry)[attacker][to_square];
#endif
		attacker = uncolored_piece(attacker);
		if (from & attacked[attacker])
			mp->eval[i] += 100000000;

		if (!see_geq(mp->pos, &mp->move[i], 0))
			mp->eval[i] -= 1000000000;

		if (to & mp->pstate->check_threats[attacker])
			mp->eval[i] += check_threat;
	}
}

void filter_moves(struct movepicker *mp) {
	for (int i = 0; mp->move[i]; i++) {
		if (move_compare(mp->move[i], mp->ttmove) ||
				move_compare(mp->move[i], mp->killer1) ||
				move_compare(mp->move[i], mp->killer2) ||
				move_compare(mp->move[i], mp->counter_move)) {
			mp->move[i] = mp->end[-1];
			*--mp->end = 0;
		}
	}
}

/* Need to check for duplicates with tt and killer moves somehow. */
move_t next_move(struct movepicker *mp) {
	int i;
	switch (mp->stage) {
	case STAGE_TT:
		mp->stage++;
		if (mp->ttmove)
			return mp->ttmove;
		/* fallthrough */
	case STAGE_GENNONQUIET:
		mp->stage++;
		mp->end = movegen(mp->pos, mp->pstate, mp->move, MOVETYPE_NONQUIET);
		filter_moves(mp);
		/* fallthrough */
	case STAGE_SORTNONQUIET:
		evaluate_nonquiet(mp);
		sort_moves(mp);
		mp->stage++;
		/* fallthrough */
	case STAGE_GOODCAPTURE:
		if (find_next(mp, &good_capture, 100))
			return *mp->move++;
		mp->stage++;
		/* fallthrough */
	case STAGE_PROMOTION:
		if (find_next(mp, &promotion, QUEEN))
			return *mp->move++;
		mp->stage++;
		/* fallthrough */
	case STAGE_OKCAPTURE:
		if (find_next(mp, &good_capture, 0))
			return *mp->move++;
		mp->stage++;
		/* fallthrough */
	case STAGE_KILLER1:
		mp->stage++;
		if (!move_compare(mp->killer1, mp->ttmove) &&
				pseudo_legal(mp->pos, mp->pstate, &mp->killer1))
			return mp->killer1;
		/* fallthrough */
	case STAGE_KILLER2:
		mp->stage++;
		if (!move_compare(mp->killer2, mp->ttmove) &&
				pseudo_legal(mp->pos, mp->pstate, &mp->killer2))
			return mp->killer2;
		/* fallthrough */
	case STAGE_COUNTER_MOVE:
		mp->stage++;
		if (!move_compare(mp->counter_move, mp->ttmove) && !move_compare(mp->counter_move, mp->killer1) &&
				!move_compare(mp->counter_move, mp->killer2) &&
				pseudo_legal(mp->pos, mp->pstate, &mp->counter_move)) {
			return mp->counter_move;
		}
		/* fallthrough */
	case STAGE_GENQUIET:
		if (mp->quiescence)
			return 0;
		/* First put all the bad leftovers at the end. */
		for (i = 0; mp->move[i]; i++)
			mp->badnonquiet[-i] = mp->move[i];
		mp->badnonquiet[-i] = 0;

		mp->stage++;
		mp->end = movegen(mp->pos, mp->pstate, mp->move, MOVETYPE_QUIET);
		filter_moves(mp);
		/* fallthrough */
	case STAGE_SORTQUIET:
		evaluate_quiet(mp);
		sort_moves(mp);
		mp->stage++;
		/* fallthrough */
	case STAGE_GOODQUIET:
		if (*mp->move && !mp->prune && *mp->eval > goodquiet_threshold)
			return mp->eval++, *mp->move++;
		mp->stage++;
		/* fallthrough */
	case STAGE_BADNONQUIET:
		if (*mp->badnonquiet)
			return *mp->badnonquiet--;
		mp->stage++;
		/* fallthrough */
	case STAGE_BADQUIET:
		if (*mp->move && !mp->prune)
			return *mp->move++;
		mp->stage++;
		/* fallthrough */
	case STAGE_DONE:
		return 0;
	default:
		assert(0);
		return 0;
	}
}

void movepicker_init(struct movepicker *mp, int quiescence, struct position *pos, const struct pstate *pstate, move_t ttmove, move_t killer1, move_t killer2, move_t counter_move, const struct searchinfo *si, const struct searchstack *ss) {
	mp->quiescence = quiescence && !pstate->checkers;
	mp->prune = 0;

	mp->move = mp->moves;
	mp->move[0] = 0;
	mp->eval = mp->evals;
	mp->badnonquiet = &mp->moves[MOVES_MAX - 1];

	mp->pos = pos;
	mp->pstate = pstate;
	mp->si = si;
	mp->ss = ss;
	mp->ttmove = ttmove;

	mp->killer1 = killer1;
	mp->killer2 = killer2;
	mp->counter_move = counter_move;

	mp->stage = STAGE_TT;
}
