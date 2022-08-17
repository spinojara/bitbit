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
#include <stdlib.h>

int interrupt = 0;

struct sigaction sa;

void sigint_handler(int num) {
	interrupt = 1;
}

void interrupt_init() {
	sa.sa_handler = &sigint_handler;
	sigaction(SIGINT, &sa, NULL);
}
