#!/usr/bin/env python3

import sys
import subprocess
from datetime import datetime
from pathlib import Path

date = datetime.now()
date = date.strftime("%y%m%d-%H%M%S")

def perft(fen, depth):
    # arguments list
    depth = str(depth)
    fen_as_list = fen.split()
    args = [ "./bitbit", "setpos" ]
    args.extend(fen_as_list)
    args.extend([ ",", "perft", "-t", depth, ",", "exit" ])

    # start process
    bitbit = subprocess.run(args,
                            universal_newlines = True,
                            stdout = subprocess.PIPE)

    # get output
    out = "position: " + fen + "\n"
    out += "depth: " + depth + "\n"
    out += bitbit.stdout[bitbit.stdout.find("nodes"):]
    return out

def perft_bench(depth):
    # <https://www.chessprogramming.org/Perft_Results>
    perft_arr = [ 
                  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",                  0,
                  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",     -1,
                  "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                                 1,
                  "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",         -1,
                  "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",         -1,
                  "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",                -1,
                  "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", -1,
                ]
    
    Path("results").mkdir(exist_ok = True)
    file = open("results/perft-" + date + ".txt", "w")
    for i in range(int(len(perft_arr) / 2)):
        out = perft(perft_arr[2 * i], depth + perft_arr[2 * i + 1])
        file.write(out + "\n")

depth = 6
if len(sys.argv) > 1:
    if sys.argv[1].isdigit():
        if int(sys.argv[1]) > 1:
            depth = int(sys.argv[1])

perft_bench(depth)
