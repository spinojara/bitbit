#!/usr/bin/env python3

import torch

K_HALF_DIMENSIONS = 256
FT_IN_DIMS = 64 * (10 * 64 + 1)
FT_OUT_DIMS = 2 * K_HALF_DIMENSIONS

class nnue(torch.nn.Module):
    def __init__(self):
        super(nnue, self).__init__()
        self.ft = torch.nn.Linear(FT_IN_DIMS, K_HALF_DIMENSIONS)
        self.hidden1 = torch.nn.Linear(FT_OUT_DIMS, 32)
        self.hidden2 = torch.nn.Linear(32, 32)
        self.output = torch.nn.Linear(32, 1)
        self.relu = torch.nn.ReLU(inplace = True)
        self.sigmoid = torch.nn.Sigmoid()

        torch.nn.init.zeros_(self.ft.weight)
        torch.nn.init.zeros_(self.ft.bias)
        #torch.nn.init.uniform_(self.hidden1.weight, -1.5, 1.5)
        #torch.nn.init.uniform_(self.hidden2.weight, -1.5, 1.5)
        #torch.nn.init.uniform_(self.output.weight, -1.5, 1.5)

    def forward(self, features1, features2):
        f1 = self.ft(features1);
        f2 = self.ft(features2);
        accumulation = torch.cat([f1, f2], dim = 1)

        #ft_out = self.sigmoid(accumulation)
        ft_out = accumulation.clamp_(min = 0, max = 1)
        #ft_out = self.relu(accumulation)
        #hidden1_out = self.sigmoid(self.hidden1(ft_out))
        hidden1_out = self.hidden1(ft_out).clamp_(min = 0, max = 1)
        #hidden1_out = self.relu(self.hidden1(ft_out))
        #hidden2_out = self.sigmoid(self.hidden2(hidden1_out))
        hidden2_out = self.hidden2(hidden1_out).clamp_(min = 0, max = 1)
        #hidden2_out = self.relu(self.hidden2(hidden1_out))
        return self.output(hidden2_out)

    def _forward(self, features1, features2):
        f1 = self.ft(features1);
        f2 = self.ft(features2);
        accumulation = torch.cat([f1, f2], dim = 1)

        #ft_out = self.sigmoid(accumulation)
        ft_out = accumulation.clamp_(min = 0, max = 1)
        #ft_out = self.relu(accumulation)
        print(ft_out[0])
        #hidden1_out = self.sigmoid(self.hidden1(ft_out))
        hidden1_out = self.hidden1(ft_out).clamp_(min = 0, max = 1)
        #hidden1_out = self.relu(self.hidden1(ft_out))
        #print(hidden1_out[0])
        #print(hidden1_out)
        #hidden2_out = self.sigmoid(self.hidden2(hidden1_out))
        hidden2_out = self.hidden2(hidden1_out).clamp_(min = 0, max = 1)
        #hidden2_out = self.relu(self.hidden2(hidden1_out))
        #print(hidden2_out[0])
        #print(hidden2_out)
        return self.output(hidden2_out)
