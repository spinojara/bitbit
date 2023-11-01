#!/usr/bin/env python3

import torch
import random

K_HALF_DIMENSIONS = 256
FT_IN_DIMS = 64 * 64 * 10
VIRTUAL = 64 * 10
FT_OUT_DIMS = 2 * K_HALF_DIMENSIONS
FV_SCALE = 16

SHIFT = 6
FT_SHIFT = 0

weight_limit = 127 / 2 ** SHIFT

PS_W_PAWN   =  0 * 64
PS_B_PAWN   =  1 * 64
PS_W_KNIGHT =  2 * 64
PS_B_KNIGHT =  3 * 64
PS_W_BISHOP =  4 * 64
PS_B_BISHOP =  5 * 64
PS_W_ROOK   =  6 * 64
PS_B_ROOK   =  7 * 64
PS_W_QUEEN  =  8 * 64
PS_B_QUEEN  =  9 * 64
PS_END      = 10 * 64

piece_to_index = [
	[ 0, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, 0,
	     PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, 0, ],
	[ 0, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, 0,
	     PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, 0, ],
]

piece_value = [ 0, 97, 491, 514, 609, 1374, ]

LRELU_SLOPE = 0.01

def orient(turn, square):
    return square ^ (0x0 if turn else 0x38)

def make_index_virtual(turn, square, piece):
    return orient(turn, square) + piece_to_index[turn][piece] + PS_END * 64

class nnue(torch.nn.Module):
    def __init__(self):
        super(nnue, self).__init__()
        self.ft = torch.nn.Linear(FT_IN_DIMS + VIRTUAL, K_HALF_DIMENSIONS + 1)
        self.hidden1 = torch.nn.Linear(FT_OUT_DIMS, 32)
        self.hidden2 = torch.nn.Linear(32, 32)
        self.output = torch.nn.Linear(32, 1)
        self.relu = torch.nn.ReLU(inplace = True)
        self.sigmoid = torch.nn.Sigmoid()

        # Initialize virtual features to 0
        torch.nn.init.zeros_(self.ft.weight[:, -VIRTUAL:])
        # Psqt Values
        for color in range(0, 2):
            for piece in range(1, 6):
                for square in range(64):
                    self.ft.weight.data[K_HALF_DIMENSIONS, make_index_virtual(color, square, piece)] = (2 * color - 1) * piece_value[piece] / (127 * 2 ** FT_SHIFT)
        # Initialize output bias to 0
        torch.nn.init.zeros_(self.output.bias)

    def forward(self, features1, features2):
        f1, psqt1 = torch.split(self.ft(features1), [256, 1], dim = 1)
        f2, psqt2 = torch.split(self.ft(features2), [256, 1], dim = 1)
        accumulation = torch.cat([f1, f2], dim = 1)
        psqtaccumulation = 0.5 * (psqt1 - psqt2)

        ft_out = self.clamp(accumulation)
        hidden1_out = self.clamp(self.hidden1(ft_out))
        hidden2_out = self.clamp(self.hidden2(hidden1_out))
        return self.output(hidden2_out) + FV_SCALE * 2 ** FT_SHIFT * psqtaccumulation / 2 ** SHIFT

    def clamp(self, x):
        return x.clamp_(0.0, 1.0)

    def clamp_weights(self):
        self.hidden1.weight.data.clamp_(-weight_limit, weight_limit)
        self.hidden2.weight.data.clamp_(-weight_limit, weight_limit)
        self.output.weight.data.clamp_(-weight_limit, weight_limit)
