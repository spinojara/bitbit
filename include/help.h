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

char help_exit[] =
"Exits bitbit.\n\n"
"exit\n"
;

char help_help[] =
"Shows a help page.\n\n"
"help [COMMAND]\n"
;

char help_version[] =
"Shows bitbit's version.\n\n"
"version [OPTION]\n\n"
"\t-d\tshows extra debug information\n"
;

char help_clear[] =
"Clears the screen.\n\n"
"clear\n"
;

char help_setpos[] =
"Sets a position.\n\n"
"setpos [OPTION] [FEN]\n\n"
"\t-i\tuses interactive mode\n"
"\t-r\tsets a random position\n"
"\t-s\tsets the starting position\n"
;

char help_domove[] =
"Does a move.\n\n"
"domove [OPTION] [MOVE]\n\n"
"\t-f\tflips the move\n"
"\t-r\treverses the last move\n"
;

char help_perft[] =
"Does a performance test.\n\n"
"perft [OPTION] [DEPTH]\n\n"
"\t-t\ttimes the performance test\n"
"\t-v\tshows verbose output\n"
;

char help_eval[] =
"Evaluates the position.\n\n"
"eval [OPTION] [TIME]\n\n"
"\t-d\targument is depth instead of time\n"
"\t-m\tdoes the best move when the evaluation is done\n"
"\t-t\ttimes the evaluation\n"
"\t-v\tshows verbose output\n"
;

char help_board[] =
"Shows the position.\n\n"
"board [OPTION]\n\n"
"\t-f\tflips the output\n"
"\t-h\tshows the history\n"
"\t-v\tshows verbose output\n"
;

char help_tt[] =
"Sets the transposition table.\n\n"
"tt [OPTION] [SIZE]\n\n"
"\t-e\t empties the transposition table\n"
"\t-s\t displays or sets the transposition table size\n"
;
