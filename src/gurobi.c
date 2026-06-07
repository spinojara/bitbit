#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <gurobi_c.h>

#include "attackgen.h"
#include "position.h"
#include "util.h"
#include "attackgen.h"
#include "magicbitboard.h"
#include "bitboard.h"

GRBenv *env = NULL;
GRBmodel *model = NULL;

void print_mailbox(int *mailbox) {
	const int flip = 0;
	int i, j, t;
	char pieces[] = " PNBRQKpnbrqk";
	char letters[] = "abcdefgh";

	printf("\n   ");
	for (i = 0; i < 8; i++)
		printf("   %c", letters[flip ? 7 - i : i]);
	printf("\n");
	for (i = 0; i < 8; i++) {
		printf("    +---+---+---+---+---+---+---+---+\n  %i |", flip ? 1 + i : 8 - i);
		for (j = 0; j < 8; j++) {
			t = flip ? 8 * i + (7 - j) : 8 * (7 - i) + j;
			printf(" %c |", pieces[mailbox[t]]);
		}
		printf(" %i\n", flip ? 1 + i : 8 - i);
	}
	printf("    +---+---+---+---+---+---+---+---+\n   ");
	for (i = 0; i < 8; i++)
		printf("   %c", letters[flip ? 7 - i : i]);
	printf("\n\n");
}

const char *algebraic_simple(int sq) {
	static char str[3];
	return algebraic(str, sq);
}

const char *piece_to_str(int piece) {
	static char buf[4096];

	sprintf(buf, "%s_%s", color_of_piece(piece) == WHITE ? "white" : "black",
			uncolored_piece(piece) == PAWN ? "pawn" :
			uncolored_piece(piece) == KNIGHT ? "knight" :
			uncolored_piece(piece) == BISHOP ? "bishop" :
			uncolored_piece(piece) == ROOK ? "rook" :
			uncolored_piece(piece) == QUEEN ? "queen" :
			uncolored_piece(piece) == KING ? "king" : "");
	return buf;
}

const char *piece_to_str_(char *s, int piece) {
	sprintf(s, "%s_%s", color_of_piece(piece) == WHITE ? "white" : "black",
			uncolored_piece(piece) == PAWN ? "pawn" :
			uncolored_piece(piece) == KNIGHT ? "knight" :
			uncolored_piece(piece) == BISHOP ? "bishop" :
			uncolored_piece(piece) == ROOK ? "rook" :
			uncolored_piece(piece) == QUEEN ? "queen" :
			uncolored_piece(piece) == KING ? "king" : "");
	return s;
}

void exit_error(void) {
	fprintf(stderr, "error: %s\n", GRBgeterrormsg(env));
	exit(1);
}

struct vcol {
	int num;
	int *ind;
	double *val;
};

void vcol_add(struct vcol *vcol, int ind, double val) {
	if (ind == -1) {
		fprintf(stderr, "vcol: trying to add negative index, this is not good.\n");
		exit(4);
	}

	for (int i = 0; i < vcol->num; i++) {
		if (vcol->ind[i] == ind) {
			fprintf(stderr, "vcol: trying to add index that's already added, this is not good.\n");
			exit(5);
		}
	}

	vcol->num++;
	vcol->ind = realloc(vcol->ind, vcol->num * sizeof(*vcol->ind));
	vcol->val = realloc(vcol->val, vcol->num * sizeof(*vcol->val));
	vcol->ind[vcol->num - 1] = ind;
	vcol->val[vcol->num - 1] = val;
}

void add_constr(struct vcol *vcol, char sense, double rhs, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
void add_constr(struct vcol *vcol, char sense, double rhs, const char *fmt, ...) {
	if (!vcol->num) {
		fprintf(stderr, "vcol: trying to add empty constraint.\n");
		exit(6);
	}

	char buf[4096] = { 0 };
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	if (GRBaddconstr(model, vcol->num, vcol->ind, vcol->val, sense, rhs, buf))
		exit_error();

	free(vcol->ind);
	free(vcol->val);
}

enum {
	VAR_PIECE,
	VAR_MOVE,
};

/* <piece> at <square> */
static int piece_vars[64 * (BLACK_KING + 1)];
/* <piece> can move from <square1> to <square2> */
static int move_vars[64 * 64 * (KING + 1)];
/* Amount of promotions of <piece> */
static int promotion_vars[BLACK_KING];

int move_index(int piece, int from, int to) {
	if (piece < PAWN || piece > KING) {
		fprintf(stderr, "Trying to get move index for piece %d\n", piece);
		exit(3);
	}
	return from + 64 * to + 64 * 64 * piece;
}

int piece_index(int piece, int sq) {
	if (piece < WHITE_PAWN || piece > BLACK_KING) {
		fprintf(stderr, "Trying to get piece index for piece %d\n", piece);
		exit(3);
	}
	return sq + 64 * piece;
}

int move_var(int piece, int from, int to) {
	int ret = move_vars[move_index(piece, from, to)];
	if (ret == -1) {
		fprintf(stderr, "Move var %d %d %d has not been stored!\n", piece, from, to);
		exit(7);
	}
	return ret;
}

int has_piece_var(int piece, int sq) {
	return piece_vars[piece_index(piece, sq)] != -1;
}

int has_move_var(int piece, int from, int to) {
	return move_vars[move_index(piece, from, to)] != -1;
}

int piece_var(int piece, int sq) {
	int ret = piece_vars[piece_index(piece, sq)];
	if (ret == -1) {
		fprintf(stderr, "Piece var %d %d has not been stored!\n", piece, sq);
		exit(7);
	}
	return ret;
}

int piece_var_r(int ind) {
	for (size_t i = 0; i < SIZE(piece_vars); i++)
		if (ind == piece_vars[i])
			return i;
	return -1;
}

int move_var_r(int ind) {
	for (size_t i = 0; i < SIZE(move_vars); i++)
		if (ind == move_vars[i])
			return i;
	return -1;
}

int add_var(double obj, double lb, double ub, const char *s) {
	static int vars = 0;

	if (GRBaddvar(model, 0, NULL, NULL, obj, lb, ub, GRB_INTEGER, s))
		exit_error();

	return vars++;
}

void add_piece_var(int piece, int sq, double lb, double ub) {
	if (piece < WHITE_PAWN || piece > BLACK_KING) {
		fprintf(stderr, "Trying to add_piece_var for piece %d\n", piece);
		exit(3);
	}
	char sqstr[3];
	char buf[4096];
	sprintf(buf, "%s_%s_on_%s", color_of_piece(piece) == WHITE ? "white" : "black",
			uncolored_piece(piece) == PAWN ? "pawn" :
			uncolored_piece(piece) == KNIGHT ? "knight" :
			uncolored_piece(piece) == BISHOP ? "bishop" :
			uncolored_piece(piece) == ROOK ? "rook" :
			uncolored_piece(piece) == QUEEN ? "queen" :
			uncolored_piece(piece) == KING ? "king" : "", algebraic(sqstr, sq));
	int index = piece_index(piece, sq);
	if (piece_vars[index] != -1) {
		fprintf(stderr, "Piece var already stored for %d %d\n", piece, sq);
		exit(8);
	}
	piece_vars[index] = add_var(0.0, lb, ub, buf);
}

int add_move_var(int piece, int from, int to, double lb, double ub) {
	if (piece < PAWN || piece > KING) {
		fprintf(stderr, "Trying to add_move_var for piece %d\n", piece);
		exit(2);
	}
	double obj = 1.0;

	if (piece == PAWN && rank_of(to) == 7)
		obj = 4.0;

	char sq1[3], sq2[3];
	char buf[4096];
	sprintf(buf, "white_%s_moving_from_%s_to_%s",
			piece == PAWN ? "pawn" :
			piece == KNIGHT ? "knight" :
			piece == BISHOP ? "bishop" :
			piece == ROOK ? "rook" :
			piece == QUEEN ? "queen" :
			piece == KING ? "king" : "",
			algebraic(sq1, from),
			algebraic(sq2, to));

	int index = move_index(piece, from, to);
	if (move_vars[index] != -1) {
		fprintf(stderr, "Move var already stored for %d %d %d\n", piece, from, to);
		exit(9);
	}
	move_vars[index] = add_var(obj, lb, ub, buf);
	return move_vars[index];
}

int add_promotion_var(int piece) {
	if (uncolored_piece(piece) < KNIGHT || uncolored_piece(piece) > QUEEN) {
		fprintf(stderr, "Trying to add_promotion_var for piece %d\n", piece);
		exit(2);
	}

	if (promotion_vars[piece] != -1) {
		fprintf(stderr, "Promotion var already stored for %d\n", piece);
		exit(9);
	}

	char buf[4096];
	sprintf(buf, "%s_%s_promotions",
			color_of_piece(piece) == WHITE ? "white" : "black",
			uncolored_piece(piece) == KNIGHT ? "knight" :
			uncolored_piece(piece) == BISHOP ? "bishop" :
			uncolored_piece(piece) == ROOK ? "rook" :
			uncolored_piece(piece) == QUEEN ? "queen" : "");
	promotion_vars[piece] = add_var(0.0, 0.0, 8.0, buf);
	return promotion_vars[piece];
}

int has_piece(int piece) {
	int exclude_pieces[] = { BLACK_QUEEN };

	for (size_t i = 0; i < SIZE(exclude_pieces); i++)
		if (exclude_pieces[i] == piece)
			return 0;
	return 1;
}

int main(void) {
	magicbitboard_init();
	attackgen_init();
	bitboard_init();
	position_init();

	for (size_t i = 0; i < SIZE(move_vars); i++)
		move_vars[i] = -1;
	for (size_t i = 0; i < SIZE(piece_vars); i++)
		piece_vars[i] = -1;
	for (size_t i = 0; i < SIZE(promotion_vars); i++)
		promotion_vars[i] = -1;

	if (GRBemptyenv(&env))
		exit_error();

	if (GRBstartenv(env))
		exit_error();

	if (GRBnewmodel(env, &model, "chess", 0, NULL, NULL, NULL, NULL, NULL))
		exit_error();

	if (GRBsetintparam(env, "MIPFocus", 2))
		exit_error();

	if (GRBsetintparam(env, "Presolve", 2))
		exit_error();

	if (GRBsetdblparam(env, "PoolGap", 0.0))
		exit_error();

	if (GRBsetintparam(env, "PrePasses", 100))
		exit_error();

	if (GRBsetintparam(env, "Symmetry", 2))
		exit_error();

	for (int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
		if (!has_piece(piece))
			continue;

		for (int sq = 0; sq < 64; sq++) {
			/* It is fine to remove this half. We remove this half
			 * because we can only castle on the other side.
			 */
			if (piece == WHITE_KING && file_of(sq) < 4)
				continue;
			if (uncolored_piece(piece) == PAWN && (rank_of(sq) == 0 || rank_of(sq) == 7))
				continue;
			add_piece_var(piece, sq, 0.0, 1.0);

			if (piece == WHITE_PAWN) {
				uint64_t pawn = bitboard(sq);
				uint64_t b = shift(pawn, N);
				b |= shift(pawn, N | E);
				b |= shift(pawn, N | W);
				if (rank_of(sq) == 1)
					b |= shift_twice(pawn, N);
				while (b) {
					int to = ctz(b);

					add_move_var(PAWN, sq, to, 0.0, 1.0);

					b = clear_ls1b(b);
				}

			}
			else if (piece <= WHITE_KING) {
				uint64_t b = attacks(uncolored_piece(piece), sq, 0, 0);
				while (b) {
					int to = ctz(b);

					add_move_var(uncolored_piece(piece), sq, to, 0.0, 1.0);

					b = clear_ls1b(b);
				}
			}
		}
	}

	/* Add castling moves */
	int castling_target[] = { a1, h1 };
	for (int i = 0; i < 2; i++) {
		if (!has_piece_var(WHITE_ROOK, castling_target[i]))
			continue;
		int ind = add_move_var(KING, e1, castling_target[i], 0.0, 1.0);
		/* Not very restrictive, but that's ok */
		struct vcol vcol = { 0 };
		vcol_add(&vcol, ind, 1.0);
		vcol_add(&vcol, piece_var(WHITE_ROOK, castling_target[i]), -1.0);
		add_constr(&vcol, GRB_LESS_EQUAL, 0.0, "rook_must_exist_to_castle_%s", i == 0 ? "long" : "short");

		struct vcol vcol2 = { 0 };
		vcol_add(&vcol2, ind, 1.0);
		vcol_add(&vcol2, piece_var(WHITE_KING, e1), -1.0);
		add_constr(&vcol2, GRB_LESS_EQUAL, 0.0, "king_must_exist_to_castle_%s", i == 0 ? "long" : "short");
	}

	/* Exactly one king */
	for (int color = 0; color < 2; color++) {
		struct vcol vcol = { 0 };
		for (int sq = 0; sq < 64; sq++)
			if (has_piece_var(colored_piece(KING, color), sq))
				vcol_add(&vcol, piece_var(colored_piece(KING, color), sq), 1.0);
		add_constr(&vcol, GRB_EQUAL, 1.0, "only_one_%s_king", color == WHITE ? "white" : "black");
	}

	/* Max 1 piece on each square */
	for (int sq = 0; sq < 64; sq++) {
		struct vcol vcol = { 0 };
		for (int piece = WHITE_PAWN; piece <= BLACK_KING; piece++)
			if (has_piece_var(piece, sq))
				vcol_add(&vcol, piece_var(piece, sq), 1.0);
		add_constr(&vcol, GRB_LESS_EQUAL, 1.0, "no_more_than_1_piece_on_%s", algebraic_simple(sq));
	}

	/* Move only allowed if correct piece is on square */
	for (int sq = 0; sq < 64; sq++) {
		for (int piece = WHITE_PAWN; piece <= WHITE_KING; piece++) {
			if (!has_piece_var(piece, sq))
				continue;
			for (int to = 0; to < 64; to++) {
				if (!has_move_var(piece, sq, to))
					continue;
				struct vcol vcol = { 0 };
				vcol_add(&vcol, move_var(piece, sq, to), 1.0);
				vcol_add(&vcol, piece_var(piece, sq), -1.0);
				char sqstr[3];
				add_constr(&vcol, GRB_LESS_EQUAL, 0.0, "move_to_%s_only_allowed_if_%s_on_%s", algebraic(sqstr, to), piece_to_str(piece), algebraic_simple(sq));
			}
		}
	}
#if 1
	/* Constraints for moves blocked by other pieces */
	for (int piece = PAWN; piece <= KING; piece++) {
		for (int from = 0; from < 64; from++) {
			for (int to = 0; to < 64; to++) {
				if (!has_move_var(piece, from, to))
					continue;
				struct vcol vcol = { 0 };
				int is_pawn_capture = piece == PAWN && to != from + 8 && to != from + 16;
#if 1
				if (is_pawn_capture) {
					struct vcol attacked = { 0 };
					for (int enemy = BLACK_PAWN; enemy <= BLACK_QUEEN; enemy++)
						if (has_piece_var(enemy, to))
							vcol_add(&attacked, piece_var(enemy, to), -1.0);
					/* En passant */
					if (rank_of(from) == 4)
						if (has_piece_var(BLACK_PAWN, to - 8))
							vcol_add(&attacked, piece_var(BLACK_PAWN, to - 8), -1.0);
					vcol_add(&attacked, move_var(PAWN, from, to), 1.0);
					char str[3];
					add_constr(&attacked, GRB_LESS_EQUAL, 0.0, "pawn_move_from_%s_to_%s_needs_to_capture", algebraic(str, from), algebraic_simple(to));
				}
#if 1
				else if (piece == KNIGHT || piece == KING) {
					/* Do nothing. Constraints are added below. */
				}
				else {
					uint64_t b = between(from, to);
					while (b) {
						int sq = ctz(b);

						for (int blocker = WHITE_PAWN; blocker <= BLACK_KING; blocker++)
							if (has_piece_var(blocker, sq)) {
								char str1[3], str2[3], str3[64];
								struct vcol blocked = { 0 };
								vcol_add(&blocked, piece_var(blocker, sq), 1.0);
								vcol_add(&blocked, move_var(piece, from, to), 1.0);
								add_constr(&blocked, GRB_LESS_EQUAL, 1.0, "blocked_move_for_%s_from_%s_to_%s_by_%s_on_%s", piece_to_str(piece), algebraic_simple(from), algebraic(str1, to), piece_to_str_(str3, blocker), algebraic(str2, sq));
								vcol_add(&vcol, piece_var(blocker, sq), 1.0);
							}

						b = clear_ls1b(b);
					}
				}
#endif
#endif
				/* Castling are supposed to be blocked by a rook on the target square */
				if (piece == KING && from == e1 && (to == a1 || to == h1))
					continue;
				/* We could exclude the king here if piece == KING */
				for (int blocker = WHITE_PAWN; blocker <= (piece == PAWN && !is_pawn_capture ? BLACK_KING : WHITE_KING); blocker++)
					if (has_piece_var(blocker, to)) {
						char str1[3], str2[3], str3[64];
						struct vcol blocked = { 0 };
						vcol_add(&blocked, piece_var(blocker, to), 1.0);
						vcol_add(&blocked, move_var(piece, from, to), 1.0);
						add_constr(&blocked, GRB_LESS_EQUAL, 1.0, "blocked_move_for_%s_from_%s_to_%s_by_%s_on_%s", piece_to_str(piece), algebraic_simple(from), algebraic(str1, to), piece_to_str_(str3, blocker), algebraic(str2, to));
					}
			}
		}
	}
#endif
#if 1
	/* Piece counts */
	for (int color = 0; color < 2; color++) {
		struct vcol all = { 0 };
		for (int piece = PAWN; piece < KING; piece++) {
			int real_piece = colored_piece(piece, color);
			if (!has_piece(real_piece))
				continue;
			if (piece == PAWN) {
				struct vcol single = { 0 };
				for (int sq = 0; sq < 64; sq++) {
					if (has_piece_var(real_piece, sq)) {
						vcol_add(&single, piece_var(real_piece, sq), 1.0);
						vcol_add(&all, piece_var(real_piece, sq), 1.0);
					}
				}
				add_constr(&single, GRB_LESS_EQUAL, 8.0, "not_more_than_8_%s_%ss", color == WHITE ? "white" : "black", piece_to_str(real_piece));
			}
			else {
				int promotion_var = add_promotion_var(real_piece);

				struct vcol single = { 0 };
				for (int sq = 0; sq < 64; sq++) {
					if (has_piece_var(real_piece, sq)) {
						vcol_add(&single, piece_var(real_piece, sq), 1.0);
						vcol_add(&all, piece_var(real_piece, sq), 1.0);
					}
				}

				int pieces = piece == QUEEN ? 1 : 2;
				vcol_add(&single, promotion_var, -1.0);
				add_constr(&single, GRB_LESS_EQUAL, (double)pieces, "not_more_than_%d_%s_%ss", 8 + pieces, color == WHITE ? "white" : "black", piece_to_str(real_piece));
			}
		}
		/* 15 because it excludes the king. */
		if (all.num)
			add_constr(&all, GRB_LESS_EQUAL, 15.0, "not_more_than_16_%s_pieces", color == WHITE ? "white" : "black");
	}
#endif
#if 1
	for (int color = 0; color < 2; color++) {
		struct vcol vcol = { 0 };
		for (int piece = KNIGHT; piece <= QUEEN; piece++) {
			int real_piece = colored_piece(piece, color);
			if (!has_piece(real_piece))
				continue;
			vcol_add(&vcol, promotion_vars[real_piece], 1.0);
		}
		for (int sq = 0; sq < 64; sq++)
			if (has_piece_var(colored_piece(PAWN, color), sq))
				vcol_add(&vcol, piece_var(colored_piece(PAWN, color), sq), 1.0);
		if (vcol.num)
			add_constr(&vcol, GRB_LESS_EQUAL, 8.0, "not_more_than_8_%s_promotions", color == WHITE ? "white" : "black");
	}
#endif

#if 1
	/* King can not be in check */
	for (int color = 0; color < 2; color++) {
		for (int attacker = PAWN; attacker <= KING; attacker++) {
			for (int to = 0; to < 64; to++) {
				if (!has_piece_var(colored_piece(KING, color), to))
					continue;
				for (int from = 0; from < 64; from++) {
					if (!has_piece_var(colored_piece(attacker, other_color(color)), from))
						continue;
					if (attacker == PAWN) {
						/* Reversed since other_color(color) is the attacker. */
						int up = other_color(color) == WHITE ? N : S;
						uint64_t pawn = bitboard(from);
						uint64_t b = shift(pawn, up | E) | shift(pawn, up | W);
						if (!(b & bitboard(to)))
							continue;
					}
					else if (!(attacks(attacker, from, 0, 0) & bitboard(to)))
						continue;
					struct vcol vcol = { 0 };

					uint64_t b = between(from, to);
					while (b) {
						int sq = ctz(b);

						for (int blocker = WHITE_PAWN; blocker <= BLACK_KING; blocker++) {
							/* King can not block itself. */
							if (uncolored_piece(blocker) == KING && color_of_piece(blocker) == color)
								continue;
							if (has_piece_var(blocker, sq))
								vcol_add(&vcol, piece_var(blocker, sq), -1.0);
						}

						b = clear_ls1b(b);
					}
					vcol_add(&vcol, piece_var(colored_piece(KING, color), to), 1.0);
					vcol_add(&vcol, piece_var(colored_piece(attacker, other_color(color)), from), 1.0);
					char str[3];
					char str1[64];
					add_constr(&vcol, GRB_LESS_EQUAL, 1.0, "%s_not_in_check_on_%s_by_%s_on_%s", piece_to_str_(str1, colored_piece(KING, color)), algebraic(str, to), piece_to_str(colored_piece(attacker, other_color(color))), algebraic_simple(from));
				}
			}
		}
	}
#endif

	const char *dirnames[] = { NULL, "north", "south", NULL, "east", "north_east", "south_east", NULL, "west", "north_west", "south_west" };
	/* Speed up by adding redundant constraints */
	for (int to = 0; to < 64; to++) {
		for (int dir = 1; dir < 16; dir++) {
			if (popcount(dir) > 2 || (dir & N && dir & S) || (dir & E && dir & W))
				/* Not a real direction */
				continue;
			struct vcol vcol = { 0 };
			uint64_t shifted = shift(bitboard(to), dir);
			if (!shifted)
				continue;

			uint64_t r = ray(to, ctz(shifted));

			for (int piece = BISHOP; piece <= QUEEN; piece++) {
				 for (int from = 0; from < 64; from++) {
					 if (bitboard(from) & r) {
						 if (has_move_var(piece, from, to))
							 vcol_add(&vcol, move_var(piece, from, to), 1.0);
					 }
				 }
			}
			if (vcol.num)
				add_constr(&vcol, GRB_LESS_EQUAL, 1.0, "not_more_than_1_attacker_to_%s_from_%s", algebraic_simple(to), dirnames[dir]);
		}
	}

	if (GRBsetintattr(model, GRB_INT_ATTR_MODELSENSE, GRB_MAXIMIZE))
		exit_error();

	if (GRBwrite(model, "gurobi.lp"))
		exit_error();

	if (GRBoptimize(model))
		exit_error();

	int solcount;
	if (GRBgetintattr(model, GRB_INT_ATTR_SOLCOUNT, &solcount))
		exit_error();

	int numvars;
	if (GRBgetintattr(model, GRB_INT_ATTR_NUMVARS, &numvars))
		exit_error();

	if (solcount > 0) {
		double *x = malloc(numvars * sizeof(*x));

		if (GRBgetdblattrarray(model, GRB_DBL_ATTR_X, 0, numvars, x))
			exit_error();

		double objval;
		if (GRBgetdblattr(model, GRB_DBL_ATTR_OBJVAL, &objval))
			exit_error();

		int mailbox[64] = { 0 };
		for (int j = 0; j < numvars; j++) {
			char *vname;
			if (GRBgetstrattrelement(model, GRB_STR_ATTR_VARNAME, j, &vname))
				exit_error();
			double obj;
			if (GRBgetdblattrelement(model, GRB_DBL_ATTR_OBJ, j, &obj))
				exit_error();

			int pv = piece_var_r(j), mv = move_var_r(j);
			if (x[j] != 0.0 && pv != -1) {
				int sq = pv % 64;
				int piece = pv / 64;
				mailbox[sq] = piece;
			}
			else if (x[j] != 0.0 && mv != -1) {
				int from = mv % 64;
				int to = (mv / 64) % 64;
				char str[3];
				printf("%s%s: %d\n", algebraic_simple(from), algebraic(str, to), (int)round(obj * x[j]));
			}
		}
		print_mailbox(mailbox);
		char pieces[] = " PNBRQKpnbrqk";
		for (int row = 7; row >= 0; row--) {
			int empty = 0;
			for (int col = 0; col < 8; col++) {
				int index = col + 8 * row;
				if (mailbox[index]) {
					if (empty) {
						printf("%d", empty);
						empty = 0;
					}
					printf("%c", pieces[mailbox[index]]);
				}
				else
					empty++;
			}
			if (empty)
				printf("%d", empty);
			if (row)
				printf("/");
		}
		printf(" w - -\n");
		printf("Moves: %d\n", (int)round(objval));
	}

	return 0;
}
