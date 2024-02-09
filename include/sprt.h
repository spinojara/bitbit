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

#ifndef SPRT_H
#define SPRT_H

#include <stdint.h>

enum {
	HNONE,
	H0,
	H1,
	HERROR,
	HCANCEL,
};

int sprt(int testtype, unsigned long games, uint64_t trinomial[3], uint64_t pentanomial[5], double alpha, double beta, double eloerror, double maintime, double increment, double elo0, double elo1, double *llh, int threads, void *ssl);

double sprt_elo(const unsigned long N[5], double *plusminus);

#endif
