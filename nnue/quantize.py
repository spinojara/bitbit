#!/usr/bin/env python3

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
        tensor = torch.round(tensor)
        bytes = tensor.detach().numpy().astype('<u2').tobytes()
        f.write(bytes)
    
        weight = 127 * (2 ** model.FT_SHIFT) * m.ft.weight.t()
        tensor = weight[:model.FT_IN_DIMS, :]
        virtual = weight[-model.VIRTUAL:, :]
        for i in range(40960):
            tensor[i] += virtual[i % 640]
    
        tensor = torch.round(tensor)
        print(torch.max(tensor))
        print(torch.min(tensor))
        bytes = tensor.detach().numpy().astype('<u2').tobytes()
        f.write(bytes)
    
        tensor = 127 * (2 ** model.SHIFT) * m.hidden1.bias.view(-1)
        tensor = torch.round(tensor)
        print(torch.max(tensor))
        print(torch.min(tensor))
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = (2 ** model.SHIFT) * m.hidden1.weight.view(-1)
        tensor = torch.round(tensor)
        print(torch.max(tensor))
        print(torch.min(tensor))
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)
    
        tensor = 127 * (2 ** model.SHIFT) * m.hidden2.bias.view(-1)
        tensor = torch.round(tensor)
        print(torch.max(tensor))
        print(torch.min(tensor))
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = (2 ** model.SHIFT) * m.hidden2.weight.view(-1)
        tensor = torch.round(tensor)
        print(torch.max(tensor))
        print(torch.min(tensor))
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)
        
        tensor = 127 * (2 ** model.SHIFT) * m.output.bias.view(-1)
        tensor = torch.round(tensor)
        print(torch.max(tensor))
        print(torch.min(tensor))
        bytes = tensor.detach().numpy().astype('<u4').tobytes()
        f.write(bytes)
    
        tensor = (2 ** model.SHIFT) * m.output.weight.view(-1)
        tensor = torch.round(tensor)
        print(torch.max(tensor))
        print(torch.min(tensor))
        bytes = tensor.detach().numpy().astype('<u1').tobytes()
        f.write(bytes)

if len(sys.argv) > 1:
    quantize(sys.argv[1])
else:
    print("No filename provided")
