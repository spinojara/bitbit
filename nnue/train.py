#!/usr/bin/env python3

import torch
import ctypes
import time

import nnue
import batch
import model

#device = "cpu"
device = torch.device('cuda')

model = model.nnue().to(device)

optimizer = torch.optim.Adam(model.parameters(), lr = 0.001)#, weight_decay = 0.001)
#optimizer = torch.optim.SGD(model.parameters(), lr = 1e-4, momentum = 0.9)
loss_fn = torch.nn.MSELoss(reduction = 'sum')
#loss_fn = torch.nn.MSELoss(reduction = 'mean')

batch.lib.next_batch.restype = ctypes.POINTER(batch.batch)


batch.lib.batch_init()
batch.lib.batch_open(b'train.bin')

scaling = 400

start_time = time.time()
for i in range(1000):
    batchptr = batch.lib.next_batch(8192)
    f1, f2, target = batchptr.contents.get_tensors(device)
    def closure():
        optimizer.zero_grad()
        output = model(f1, f2)
        loss = loss_fn(output, target)
        #wdl_model = torch.sigmoid(output / scaling)
        #wdl_target = torch.sigmoid(target / scaling)
        #loss = loss_fn(wdl_model, wdl_target)
        #loss = torch.sum(torch.abs(wdl_model - wdl_target) ** 3)
        loss.backward()
        if i % 30 == 0:
            print(output)
            print(target)
            print(loss)
    optimizer.step(closure)
    batch.lib.free_batch(batchptr)

print(time.time() - start_time)

print(f"ft {torch.max(model.ft.weight).item()}, {torch.min(model.ft.weight).item()}")
print(f"ft {torch.max(model.ft.bias).item()}, {torch.min(model.ft.bias).item()}")
print(f"hidden1 {torch.max(model.hidden1.weight).item()}, {torch.min(model.hidden1.weight).item()}")
print(f"hidden1 {torch.max(model.hidden1.bias).item()}, {torch.min(model.hidden1.bias).item()}")
print(f"hidden2 {torch.max(model.hidden2.weight).item()}, {torch.min(model.hidden2.weight).item()}")
print(f"hidden2 {torch.max(model.hidden2.bias).item()}, {torch.min(model.hidden2.bias).item()}")
print(f"output {torch.max(model.output.weight).item()}, {torch.min(model.output.weight).item()}")
print(f"output {torch.max(model.output.bias).item()}, {torch.min(model.output.bias).item()}")


batch.lib.batch_close()
batch.lib.batch_term()
