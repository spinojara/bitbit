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

#ifndef TUNE_H
#define TUNE_H

void print_tune(void);

void settune(int argc, char **argv);

void tune_variable(const char *name, double start, void (*set)(double), double (*get)(void), int is_int_type);

#ifdef TUNE
#define TUNEVAR(type, var, start, min, max) \
type var = (start); \
void tune_set_ ## var (double x) { \
	if (_Generic((min), void *: 0, default: 1) && (type)_Generic((min), void *: 0, default: (min)) > x) { \
		x = (type)_Generic(min, void *: 0, default: (min)); \
	} \
	if (_Generic((max), void *: 0, default: 1) && (type)_Generic((max), void *: 0, default: (max)) < x) { \
		x = (type)_Generic(max, void *: 0, default: (max)); \
	} \
	var = (type)x; \
}\
double tune_get_ ## var (void) { \
	return (double)(var); \
}\
__attribute__((constructor)) void tune_ ## var (void) { \
	tune_variable(#var, (start), tune_set_ ## var, tune_get_ ## var, (type)1 / 2 == (type)0); \
}
#else
#define TUNEVAR(type, var, start, min, max) const type var = (start);
#endif

#endif
