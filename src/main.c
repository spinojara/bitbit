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
#include "evaluate.h"
#include "transposition_table.h"
#include "interface.h"
#include "interrupt.h"

int main(int argc, char **argv) {
	/* --version */
	if (init(argc, argv))
		goto term;
	interrupt_init();
	util_init();
	/* no magic found */
	if (magic_bitboard_init())
		goto term;
	attack_gen_init();
	bitboard_init();
	evaluate_init();
	/* transposition table size == 0 */
	if (transposition_table_init())
		goto term;
	interface_init();
	interface(argc, argv);
term:;
	interface_term();
	transposition_table_term();
	term();
}
