#!/usr/bin/env python3

import numpy
import ctypes
import torch

import model

class batch(ctypes.Structure):
    _fields_ = [
            ('size', ctypes.c_int),
            ('requested_size', ctypes.c_int),
            ('ind_active', ctypes.c_int),
            ('ind1', ctypes.POINTER(ctypes.c_int32)),
            ('ind2', ctypes.POINTER(ctypes.c_int32)),
            ('eval', ctypes.POINTER(ctypes.c_float)),
    ]

    def get_tensors(self, device):
        eval = torch.from_numpy(numpy.ctypeslib.as_array(self.eval, shape = (self.size, 1))).pin_memory().to(device = device, non_blocking = True)

        val1 = torch.ones(self.ind_active).pin_memory().to(device = device, non_blocking = True)
        val2 = torch.ones(self.ind_active).pin_memory().to(device = device, non_blocking = True)

        ind1 = torch.transpose(torch.from_numpy(numpy.ctypeslib.as_array(self.ind1, shape = (self.ind_active, 2))), 0, 1).long().pin_memory().to(device = device, non_blocking = True)

        ind2 = torch.transpose(torch.from_numpy(numpy.ctypeslib.as_array(self.ind2, shape = (self.ind_active, 2))), 0, 1).long().pin_memory().to(device = device, non_blocking = True)

        f1 = torch._sparse_coo_tensor_unsafe(ind1, val1, (self.size, model.FT_IN_DIMS + model.VIRTUAL)).to(device = device, non_blocking = True)

        f2 = torch._sparse_coo_tensor_unsafe(ind2, val2, (self.size, model.FT_IN_DIMS + model.VIRTUAL)).to(device = device, non_blocking = True)

        f1._coalesced_(True)
        f2._coalesced_(True)

        return f1, f2, eval

    def is_empty(self):
        return (self.size == 0)


lib = ctypes.cdll.LoadLibrary('/usr/src/bitbit/libbatch.so')

lib.next_batch.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.next_batch.restype = ctypes.POINTER(batch)

lib.batch_open.argtypes = [ctypes.c_char_p]
lib.batch_open.restype = ctypes.c_void_p

lib.batch_reset.argtypes = [ctypes.c_void_p]
lib.batch_reset.restype = None

lib.free_batch.argtypes = [ctypes.POINTER(batch)]
lib.free_batch.restype = None

lib.batch_close.argtypes = [ctypes.c_void_p]
lib.batch_close.restype = None

lib.batch_total.argtypes = [ctypes.c_void_p]
lib.batch_total.restype = ctypes.c_uint64
