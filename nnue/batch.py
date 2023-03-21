#!/usr/bin/env python3

import numpy
import ctypes
import torch

import model

class batch(ctypes.Structure):
    _fields_ = [
            ('size', ctypes.c_int),
            ('ind_active', ctypes.c_int),
            ('ind1', ctypes.POINTER(ctypes.c_int32)),
            ('ind2', ctypes.POINTER(ctypes.c_int32)),
            ('eval', ctypes.POINTER(ctypes.c_float)),
    ]

    def get_tensors(self, device):
        eval = torch.from_numpy(numpy.ctypeslib.as_array(self.eval, shape = (self.size, 1))).to(device)

        val1 = torch.ones(self.ind_active)
        val2 = torch.ones(self.ind_active)

        ind1 = torch.transpose(torch.from_numpy(numpy.ctypeslib.as_array(self.ind1, shape = (self.ind_active, 2))), 0, 1).long()

        ind2 = torch.transpose(torch.from_numpy(numpy.ctypeslib.as_array(self.ind2, shape = (self.ind_active, 2))), 0, 1).long()

        f1 = torch.sparse_coo_tensor(ind1, val1, (self.size, model.FT_IN_DIMS)).to(device)

        f2 = torch.sparse_coo_tensor(ind2, val2, (self.size, model.FT_IN_DIMS)).to(device)

        return f1, f2, eval

lib = ctypes.cdll.LoadLibrary('/usr/src/bitbit/libbatch.so')

lib.next_batch.argtypes = [ctypes.c_int]
lib.next_batch.restype = ctypes.POINTER(batch)
lib.batch_open.argtypes = [ctypes.c_char_p]
lib.batch_open.restype = None
