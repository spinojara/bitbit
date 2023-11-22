#!/usr/bin/env python3

# bitbit, a bitboard based chess engine written in c.
# Copyright (C) 2022 Isak Ellmer
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import sys
import torch

import model

def quantize(infile):
    if (not infile.endswith(".pt")):
        return
    m = model.nnue()
    m.load_state_dict(torch.load(infile))
    m.clamp_weights()

    outfile = infile.replace("pt", "nnue")

    with open(outfile, 'wb') as f:
        tensor = 127 * (2 ** model.FT_SHIFT) * m.ft.bias.view(-1)
        mean = torch.mean(torch.abs(tensor)).round().long().item()
        tensor = tensor.round().long()
        print(torch.min(tensor).item(), "<= ft_biases <=", torch.max(tensor).item(), "absolute mean: ", mean)
        bytes = tensor.detach().numpy().astype('<u2').tobytes()
        f.write(bytes)
    
        weight = 127 * (2 ** model.FT_SHIFT) * m.ft.weight.t()
        tensor = weight[:model.FT_IN_DIMS, :]
        virtual = weight[-model.VIRTUAL:, :]
        for i in range(40960):
            tensor[i] += virtual[i % 640]
        mean = torch.mean(torch.abs(tensor[:, :256])).round().long().item()
        tensor = tensor.round().long()
        print(torch.min(tensor[:, :256]).item(), "<= ft_weights <=", torch.max(tensor[:, :256]).item(), "absolute mean: ", mean)
        bytes = tensor.detach().numpy().astype('<u2').tobytes()
        f.write(bytes)
    
        tensor = 127 * (2 ** model.SHIFT) * m.hidden1.bias.view(-1)
        mean = torch.mean(torch.abs(tensor)).round().long().item()
        tensor = tensor.round().long()
        print(torch.min(tensor).item(), "<= hidden1_biases <=", torch.max(tensor).item(), "absolute mean: ", mean)
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = (2 ** model.SHIFT) * m.hidden1.weight.view(-1)
        mean = torch.mean(torch.abs(tensor)).round().long().item()
        tensor = tensor.round().long()
        print(torch.min(tensor).item(), "<= hidden1_weights <=", torch.max(tensor).item(), "absolute mean: ", mean)
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)
    
        tensor = 127 * (2 ** model.SHIFT) * m.hidden2.bias.view(-1)
        mean = torch.mean(torch.abs(tensor)).round().long().item()
        tensor = tensor.round().long()
        print(torch.min(tensor).item(), "<= hidden2_biases <=", torch.max(tensor).item(), "absolute mean: ", mean)
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = (2 ** model.SHIFT) * m.hidden2.weight.view(-1)
        mean = torch.mean(torch.abs(tensor)).round().long().item()
        tensor = tensor.round().long()
        print(torch.min(tensor).item(), "<= hidden2_weights <=", torch.max(tensor).item(), "absolute mean: ", mean)
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)
        
        tensor = 127 * (2 ** model.SHIFT) * m.output.bias.view(-1)
        mean = torch.mean(torch.abs(tensor)).round().long().item()
        tensor = tensor.round().long()
        print(torch.min(tensor).item(), "<= output_biases <=", torch.max(tensor).item(), "absolute mean: ", mean)
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = (2 ** model.SHIFT) * m.output.weight.view(-1)
        mean = torch.mean(torch.abs(tensor)).round().long().item()
        tensor = tensor.round().long()
        print(torch.min(tensor).item(), "<= output_weights <=", torch.max(tensor).item(), "absolute mean: ", mean)
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)

        print("\npiece values: ")

        for piece in range(1, 6):
            average = 0
            for square in range(0, 64):
                average += weight[model.make_index_virtual(1, square, piece), model.K_HALF_DIMENSIONS]
                average -= weight[model.make_index_virtual(0, square, piece), model.K_HALF_DIMENSIONS]
            print((average / (2 * 64)).round().long().item())

if len(sys.argv) > 1:
    quantize(sys.argv[1])
else:
    print("No filename provided")
