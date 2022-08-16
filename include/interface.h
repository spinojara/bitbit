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

#ifndef INTERFACE_H
#define INTERFACE_H

struct arg {
	int v;
	int t;
	int r;
	int f;
	int h;
	int m;
	int s;
	int e;
	int i;
	int argc;
	char **argv;
};

void interface(int argc, char **argv);

void interface_init();

void interface_term();

int interface_version(struct arg *arg);

#endif
