#!/usr/bin/env python3

import sys
import torch

import model


def quantize(infile):
    if (not infile.endswith(".pt")):
        return
    m = model.nnue()
    m.load_state_dict(torch.load(infile))

    outfile = infile.replace("pt", "nnue")

    with open(outfile, 'wb') as f:
        tensor = 127 * 64 * m.ft.bias.view(-1)
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u2').tobytes()
        f.write(bytes)
    
        weight = 127 * 64 * m.ft.weight.t()
        tensor = weight[:model.FT_IN_DIMS, :]
        virtual = weight[-model.VIRTUAL:, :]
        for i in range(40960):
            tensor[i] += virtual[i % 640]
    
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u2').tobytes()
        f.write(bytes)
    
        tensor = 127 * 64 * m.hidden1.bias.view(-1)
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = 64 * m.hidden1.weight.view(-1)
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)
    
        tensor = 127 * 64 * m.hidden2.bias.view(-1)
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = 64 * m.hidden2.weight.view(-1)
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)
        
        tensor = 127 * 64 * m.output.bias.view(-1)
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = 64 * m.output.weight.view(-1)
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)

if len(sys.argv) > 1:
    quantize(sys.argv[1])
else:
    print("No filename provided")
