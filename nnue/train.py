#!/usr/bin/env python3

import torch
import ctypes
import time
import math
import hashlib

import batch
import model
import save

device = torch.device('cuda')

model = model.nnue().to(device = device, non_blocking = True)

model.load_state_dict(torch.load('1.pt'))

def lr_lambda(loss):
    ret = 0.001 + (0.05 - 0.001) * loss / sigmoid(1000 / 400)
    return ret

# learning rate from 0.1 to 0.005
optimizer = torch.optim.Adam(model.parameters(), lr = 1e-4)

sigmoid_scaling = math.log(10) / 400
scaling = (127 * 64 / 16)
def sigmoid(x):
    return 1 / (1 + math.exp(-sigmoid_scaling * x))

def inverse_sigmoid(y):
    return -math.log(1 / y - 1) / sigmoid_scaling

loss_exponent = 2
def loss_fn(output, target):
    wdl_output = torch.sigmoid(scaling * output * sigmoid_scaling)
    wdl_target = torch.sigmoid(scaling * target * sigmoid_scaling)
    return torch.sum(torch.pow(torch.abs(wdl_output - wdl_target), loss_exponent))

epochs = 2000
save_every = 1
batch_size = 16384

batch.lib.batch_init()
training_data = batch.lib.batch_open(b'out.bin', batch_size)
validation_data = batch.lib.batch_open(b'val.bin', batch_size)

while True:
    batchptr = batch.lib.next_batch(validation_data)
    if (batchptr.contents.is_empty()):
        break

total_time = time.time()
for epoch in range(1, epochs + 1):
    epoch_time = time.time()
    print(f"starting epoch {epoch} of {epochs}")

    batch.lib.batch_reset(validation_data)
    loss = 0
    total = 0
    while True:
        batchptr = batch.lib.next_batch(validation_data)
        if (batchptr.contents.is_empty()):
            break
        f1, f2, target = batchptr.contents.get_tensors(device)
        total += batchptr.contents.actual_size
        output = model(f1, f2)
        loss += loss_fn(output, target).item()
    loss /= total
    loss = loss ** (1 / loss_exponent)
    before_lr = optimizer.param_groups[0]['lr']
    #optimizer.param_groups[0]['lr'] = lr_lambda(loss)
    #optimizer.param_groups[0]['lr'] = 0.001
    after_lr = optimizer.param_groups[0]['lr']
    # Normalized loss is around 0 centipawns = 1 / 2 wdl
    # and is then the inverse of sigmoid.
    loss = 2 * inverse_sigmoid(1 / 2 + loss / 2)
    print("learning rate adjusted from {:.1e} to {:.1e}".format(before_lr, after_lr))
    print(f"normalized average centipawn loss is {round(loss)} for validation data")

    batch.lib.batch_reset(training_data)
    while True:
        batchptr = batch.lib.next_batch(training_data)
        if (batchptr.contents.is_empty()):
            break
        f1, f2, target = batchptr.contents.get_tensors(device)
        def closure():
            optimizer.zero_grad()
            output = model(f1, f2)
            loss = loss_fn(output, target) / batchptr.contents.actual_size
            loss.backward()
            return loss
        model.clamp_weights()
        optimizer.step(closure)

    epoch_time = time.time() - epoch_time
    eta = time.time() + (epochs - epoch) * epoch_time
    print(f"epoch elapsed {round(epoch_time, 2)} seconds")
    print(f"estimated time of arrival is {time.strftime('%Y-%m-%d %H:%M', time.localtime(eta))}\n")

    if (epoch % save_every == 0):
        print(f"saving network to temporary file: nnue.pt")
        torch.save(model.state_dict(), "nnue.pt")

batch.lib.batch_close(validation_data)
batch.lib.batch_close(training_data)
print(f"training elapsed {round(time.time() - total_time, 2)} seconds")

save.save_model(model)
