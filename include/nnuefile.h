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

#ifndef NNUEFILE_H
#define NNUEFILE_H

#include "nnue.h"

int nnuefile(FILE *f, ft_weight_t *ft_weights, ft_bias_t *ft_biases, ft_weight_t *psqt_weights, weight_t *hidden1_weights, bias_t *hidden1_biases, weight_t *hidden2_weights, bias_t *hidden2_biases, weight_t *output_weights, bias_t *output_biases);

#endif
