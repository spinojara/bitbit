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

def lr_lambda(loss):
    ret = 0.001 + (0.05 - 0.001) * loss / sigmoid(1000 / 400)
    return ret

# learning rate from 0.1 to 0.005
optimizer = torch.optim.Adam(model.parameters(), lr = 0.00007)#, weight_decay = 0.0001)
loss_mean = torch.nn.MSELoss(reduction = 'mean')
loss_sum = torch.nn.MSELoss(reduction = 'sum')

def sigmoid(x):
    return 1 / (1 + math.exp(-x))

def inverse_sigmoid(y):
    return -math.log(1 / y - 1)

batch.lib.batch_init()
training_data = batch.lib.batch_open(b'train.bin')
validation_data = batch.lib.batch_open(b'train.bin')

scaling = 400 * 16 / (127 * 64)

epochs = 2

#batch_size = 32768
batch_size = 16384

total_time = time.time()
for epoch in range(1, epochs + 1):
    epoch_time = time.time()
    print(f"starting epoch {epoch} of {epochs}")

    batch.lib.batch_reset(validation_data)
    loss = 0
    while True:
        batchptr = batch.lib.next_batch(validation_data, batch_size)
        if (batchptr.contents.is_empty()):
            batch.lib.free_batch(batchptr)
            break
        f1, f2, target = batchptr.contents.get_tensors(device)
        output = model(f1, f2)
        wdl_output = torch.sigmoid(output / scaling)
        wdl_target = torch.sigmoid(target / scaling)
        loss += loss_sum(wdl_output, wdl_target).item()
        batch.lib.free_batch(batchptr)
    loss /= batch.lib.batch_total(validation_data)
    loss = math.sqrt(loss)
    before_lr = optimizer.param_groups[0]['lr']
    #optimizer.param_groups[0]['lr'] = lr_lambda(loss)
    #optimizer.param_groups[0]['lr'] = 0.001
    after_lr = optimizer.param_groups[0]['lr']
    # Normalized loss is around 0 centipawns = 1 / 2 wdl
    # and is then the inverse of sigmoid.
    loss = scaling * 2 * inverse_sigmoid(1 / 2 + loss / 2)
    # And to centipawns.
    loss *= 127 * 64 / 16
    print("learning rate adjusted from {:.1e} to {:.1e}".format(before_lr, after_lr))
    print(f"normalized average centipawn loss is {round(loss)} for validation data")

    batch.lib.batch_reset(training_data)
    while True:
        batchptr = batch.lib.next_batch(training_data, batch_size)
        if (batchptr.contents.is_empty()):
            batch.lib.free_batch(batchptr)
            break
        f1, f2, target = batchptr.contents.get_tensors(device)
        def closure():
            global loss_s
            optimizer.zero_grad()
            output = model(f1, f2)
            wdl_output = torch.sigmoid(output / scaling)
            wdl_target = torch.sigmoid(target / scaling)
            loss = loss_mean(wdl_output, wdl_target)
            loss.backward()
            optimizer.step()
        optimizer.step(closure)
        model.clamp()
        batch.lib.free_batch(batchptr)
    
    epoch_time = time.time() - epoch_time
    eta = time.time() + (epochs - epoch) * epoch_time
    print(f"epoch elapsed {round(epoch_time, 2)} seconds")
    print(f"estimated time of arrival is {time.strftime('%Y-%m-%d %H:%M', time.localtime(eta))} \n")

print(f"training elapsed {round(time.time() - total_time, 2)} seconds")

save.save_model(model)

batch.lib.batch_close(validation_data)
batch.lib.batch_close(training_data)
