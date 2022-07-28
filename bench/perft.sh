#!/bin/sh

function bench_perft {
	./bitbit set position "$1", go perft "$2" -t, exit
}

POSITION_ARR=("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
              "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
              "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
              "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
              "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
              "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10")

DEPTH_ARR=(0
           -1
           1
           -1
           -1
	   -1)

if [[ ! -f bench/perft.sh ]]; then
	echo "Run perft.sh from the root directory."
	exit 1
elif [[ ! -f bitbit ]]; then
	echo -e "You must first compile a bitbit\nexecutable by running make."
	exit 1
elif [[ ! -d results ]]; then
	mkdir results
fi

echo -e "\n#######################################################" >> results/log.txt
echo "#                                                     #" >> results/log.txt
echo "#  New Benchmark at $(date)  #" >> results/log.txt
echo "#                                                     #" >> results/log.txt
echo -e "#######################################################\n" >> results/log.txt

for i in {0..5}
do
	bench_perft "${POSITION_ARR[i]}" "$((6 + ${DEPTH_ARR[i]}))" >> results/log.txt
done
