#!/usr/bin/env python3

import torch

import hashlib
import os

def save_model(model):
    torch.save(model.state_dict(), "out.pt")
    
    with open("out.pt", 'rb') as f:
        contents = f.read()
    
    hashvalue = hashlib.md5(contents).hexdigest()[:16]
    
    os.rename("out.pt", hashvalue + ".pt")

    print("saving network to file: " + hashvalue + ".pt")
