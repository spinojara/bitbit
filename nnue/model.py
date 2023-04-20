#!/usr/bin/env python3

import torch

K_HALF_DIMENSIONS = 256
FT_IN_DIMS = 64 * 64 * 10
VIRTUAL = 64 * 10
FT_OUT_DIMS = 2 * K_HALF_DIMENSIONS

weight_limit = 127 / 64

class nnue(torch.nn.Module):
    def __init__(self):
        super(nnue, self).__init__()
        self.ft = torch.nn.Linear(FT_IN_DIMS + VIRTUAL, K_HALF_DIMENSIONS)
        self.hidden1 = torch.nn.Linear(FT_OUT_DIMS, 32)
        self.hidden2 = torch.nn.Linear(32, 32)
        self.output = torch.nn.Linear(32, 1)
        self.relu = torch.nn.ReLU(inplace = True)
        self.sigmoid = torch.nn.Sigmoid()

        #torch.nn.init.zeros_(self.ft.weight)
        #torch.nn.init.zeros_(self.ft.bias)
        #torch.nn.init.uniform_(self.ft.weight, -1.5, 1.5)
        #torch.nn.init.uniform_(self.hidden1.weight, -1.5, 1.5)
        #torch.nn.init.uniform_(self.hidden2.weight, -1.5, 1.5)
        #torch.nn.init.uniform_(self.output.weight, -1.5, 1.5)

    def forward(self, features1, features2):
        f1 = self.ft(features1);
        f2 = self.ft(features2);
        accumulation = torch.cat([f1, f2], dim = 1)

        ft_out = accumulation.clamp_(min = 0, max = 1)
        hidden1_out = self.hidden1(ft_out).clamp_(min = 0, max = 1)
        hidden2_out = self.hidden2(hidden1_out).clamp_(min = 0, max = 1)
        return self.output(hidden2_out)

    def clamp(self):
        self.hidden1.weight.data = self.hidden1.weight.data.clamp(-weight_limit, weight_limit)
        self.hidden2.weight.data = self.hidden2.weight.data.clamp(-weight_limit, weight_limit)
        self.output.weight.data = self.output.weight.data.clamp(-weight_limit, weight_limit)
