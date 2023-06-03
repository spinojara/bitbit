#!/usr/bin/env python3

import torch

K_HALF_DIMENSIONS = 256
FT_IN_DIMS = 64 * 64 * 10
VIRTUAL = 64 * 10
FT_OUT_DIMS = 2 * K_HALF_DIMENSIONS

SHIFT = 6
FT_SHIFT = 3

weight_limit = 127 / 2 ** SHIFT

LRELU_SLOPE = 0.01

class nnue(torch.nn.Module):
    def __init__(self):
        super(nnue, self).__init__()
        self.ft = torch.nn.Linear(FT_IN_DIMS + VIRTUAL, K_HALF_DIMENSIONS)
        self.hidden1 = torch.nn.Linear(FT_OUT_DIMS, 32)
        self.hidden2 = torch.nn.Linear(32, 32)
        self.output = torch.nn.Linear(32, 1)
        self.relu = torch.nn.ReLU(inplace = True)
        self.sigmoid = torch.nn.Sigmoid()

        torch.nn.init.zeros_(self.ft.weight)
        torch.nn.init.zeros_(self.ft.bias)
        torch.nn.init.zeros_(self.output.bias)

    def forward(self, features1, features2):
        f1 = self.ft(features1);
        f2 = self.ft(features2);
        accumulation = torch.cat([f1, f2], dim = 1)

        ft_out = self.activation(accumulation)
        hidden1_out = self.activation(self.hidden1(ft_out))
        hidden2_out = self.activation(self.hidden2(hidden1_out))
        return self.output(hidden2_out)

    def activation(self, x):
        return self.lclamp(x)

    def clamp(self, x):
        return x.clamp(0.0, 1.0)

    def lclamp(self, x):
        return LRELU_SLOPE * x.clamp_max(0.0) + x.clamp(0.0, 1.0) + LRELU_SLOPE * (x.clamp_min(1.0) - 1)

    def clamp_weights(self):
        self.hidden1.weight.data = self.hidden1.weight.data.clamp(-weight_limit, weight_limit)
        self.hidden2.weight.data = self.hidden2.weight.data.clamp(-weight_limit, weight_limit)
        self.output.weight.data = self.output.weight.data.clamp(-weight_limit, weight_limit)
