/* bitbit, a bitboard based chess engine written in c.
 * Copyright (C) 2022 Isak Ellmer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "init.h"
#include "util.h"
#include "bitboard.h"
#include "magic_bitboard.h"
#include "attack_gen.h"
#include "search.h"
#include "evaluate.h"
#include "transposition_table.h"
#include "move_order.h"
#include "interface.h"
#include "interrupt.h"
#include "pawn.h"
#include "nnue.h"

int main(int argc, char **argv) {
	int ret = 0;
	/* --version */
	if (init(argc, argv))
		goto term;
	interrupt_init();
	util_init();
	/* no magic found */
	if ((ret = magic_bitboard_init()))
		goto term;
	attack_gen_init();
	bitboard_init();
	evaluate_init();
	search_init();
	move_order_init();
	/* transposition table size == 0 */
	if ((ret = transposition_table_init()))
		goto term;
	position_init();
	if ((ret = pawn_init()))
		goto term;
	interface_init();
	nnue_init(argc, argv);
	ret = interface(argc, argv);
term:;
	interface_term();
	pawn_term();
	transposition_table_term();
	term();
	return ret;
}
