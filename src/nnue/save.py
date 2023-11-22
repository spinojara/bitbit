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
