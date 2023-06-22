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

#model.load_state_dict(torch.load("2.pt"))

optimizer = torch.optim.Adam(model.parameters(), lr = 1e-3)
scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size = 1, gamma = 0.992)

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

epochs = 400
save_every = 1
batch_size = 16384

batch.lib.batch_init()
training_data = batch.lib.batch_open(b'train.bin', batch_size, 0.8)
validation_data = batch.lib.batch_open(b'val.bin', batch_size, 0)

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
    loss **= (1 / loss_exponent)
    # Normalized loss is around 0 centipawns = 1 / 2 wdl
    # and is then the inverse of sigmoid.
    losscp = 2 * inverse_sigmoid(1 / 2 + loss / 2)
    print(f"loss is {round(loss, 5)} ({round(losscp)} cp) for validation data")
    print("learning rate is now {:.2e}".format(optimizer.param_groups[0]['lr']))

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

    scheduler.step()
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
