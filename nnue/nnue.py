#!/usr/bin/env python3

PS_W_PAWN   =  1
PS_B_PAWN   =  1 * 64 + 1
PS_W_KNIGHT =  2 * 64 + 1
PS_B_KNIGHT =  3 * 64 + 1
PS_W_BISHOP =  4 * 64 + 1
PS_B_BISHOP =  5 * 64 + 1
PS_W_ROOK   =  6 * 64 + 1
PS_B_ROOK   =  7 * 64 + 1
PS_W_QUEEN  =  8 * 64 + 1
PS_B_QUEEN  =  9 * 64 + 1
PS_END      = 10 * 64 + 1

piece_to_index = [
	[ 0, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, 0,
	     PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, 0 ],
	[ 0, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, 0,
	     PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, 0 ],
]

# should use horizontal symmetry
def orient(turn, square):
    return square ^ (0x0 if turn == 1 else 0x3F)

def make_index(turn, square, piece, king_square):
    return piece_to_index[turn][piece] + PS_END * king_square
