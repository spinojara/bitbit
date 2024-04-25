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

#include "init.h"
#include "bitboard.h"
#include "magicbitboard.h"
#include "attackgen.h"
#include "search.h"
#include "evaluate.h"
#include "transposition.h"
#include "moveorder.h"
#include "interface.h"
#include "interrupt.h"
#include "tables.h"
#include "endgame.h"
#include "nnue.h"

int main(int argc, char **argv) {
	int ret;
	/* --version */
	if (init(argc, argv))
		return 0;
	interrupt_init();
	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	tables_init();
	search_init();
	moveorder_init();
	position_init();
	transposition_init();
	endgame_init();
	nnue_init();
	ret = interface(argc, argv);
	return ret;
}
