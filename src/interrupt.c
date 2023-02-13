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

#include "interrupt.h"

#include <signal.h>

#include "init.h"
#include "util.h"

volatile int interrupt = 0;

void sigint_handler(int num) {
	UNUSED(num);
	interrupt = 2;
	signal(SIGINT, sigint_handler);
}

void interrupt_init(void) {
	signal(SIGINT, sigint_handler);
	init_status("setting signal handler");
}
