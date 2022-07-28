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
#include "hash_table.h"
#include "interface.h"

int main(int argc, char **argv) {
	/* --version */
	if (!init(argc, argv))
		return 0;
	util_init();
	magic_bitboard_init();
	attack_gen_init();
	bitboard_init();
	evaluate_init();
	/* hash table size == 0 */
	if (!hash_table_init())
		goto exit_early;
	hash_table_init();
	interface_init();
	interface(argc, argv);
	interface_term();
	hash_table_term();
exit_early:
	term();
}
