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

MAJOR      = 1
MINOR      = 0
VERSION   := $(MAJOR).$(MINOR)

KERNEL     = $(shell uname -s)
ARCH       = $(shell uname -m)
ifneq ($(findstring x86, $(ARCH)), )
	ARCH = -march=native -mtune=native
else ifneq ($(findstring arm, $(ARCH)), )
	ARCH = -march=native -mtune=native
else ifneq ($(findstring ppc64, $(ARCH)), )
	ARCH = -mtune=native
endif

CC         = cc
CSTANDARD  = -std=c11
CWARNINGS  = -Wall -Wextra -Wshadow -pedantic -Wno-unused-result -Wvla
COPTIMIZE := -O2 $(ARCH) -flto

ifeq ($(DEBUG), 1)
	CDEBUG    = -g3 -ggdb
else ifeq ($(DEBUG), 2)
	CDEBUG    = -g3 -ggdb -fsanitize=address,undefined
else ifeq ($(DEBUG), 3)
	CDEBUG    = -g3 -ggdb -fsanitize=address,undefined
	COPTIMIZE =
else
	CDEBUG    = -DNDEBUG
endif

CFLAGS    := $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE) $(CDEBUG) -pthread
LDFLAGS    = $(CFLAGS)
LDLIBS     = -lm -lsqlite3

ifeq ($(SIMD), avx2)
	CFLAGS  += -DAVX2 -mavx2
endif
ifeq ($(SIMD), sse4)
	CFLAGS  += -DSSE4 -msse4
endif
ifeq ($(SIMD), sse2)
	CFLAGS  += -DSSE2 -msse2
endif

ifeq ($(TT), )
	TT = 64
endif

ifneq ($(SYZYGY), )
	LDLIBS  += -lfathom
	DSYZYGY := -DSYZYGY=$(SYZYGY)
endif

ifeq ($(NNUE), )
	NNUE = etc/current.nnue
endif

SRC_BITBIT     = bitbit.c bitboard.c magicbitboard.c attackgen.c \
                 move.c util.c position.c movegen.c perft.c \
                 search.c evaluate.c tables.c interface.c \
                 transposition.c init.c timeman.c interrupt.c \
                 pawn.c history.c movepicker.c moveorder.c \
                 option.c endgame.c nnue.c kpk.c kpkp.c krkp.c \
                 nnueweights.c

SRC_GENNNUE    = gennnue.c bitboard.c magicbitboard.c attackgen.c \
                 move.c util.c position.c movegen.c evaluate.c \
                 tables.c timeman.c interrupt.c pawn.c \
                 moveorder.c transposition.c movepicker.c \
                 history.c option.c nnue.c search.c \
                 endgame.c nnueweights.c kpk.c kpkp.c krkp.c

SRC_GENEPD     = genepd.c bitboard.c magicbitboard.c attackgen.c \
                 move.c util.c position.c movegen.c option.c

SRC_HISTOGRAM  = histogram.c bitboard.c magicbitboard.c move.c \
                 position.c interrupt.c util.c \
                 option.c evaluate.c pawn.c endgame.c tables.c \
                 attackgen.c nnue.c nnueweights.c kpk.c kpkp.c krkp.c \
                 movegen.c transposition.c

SRC_PGNBIN     = pgnbin.c bitboard.c magicbitboard.c attackgen.c \
                 move.c util.c position.c movegen.c \
                 evaluate.c option.c transposition.c search.c \
                 movepicker.c nnue.c pawn.c tables.c moveorder.c \
	         endgame.c kpk.c kpkp.c krkp.c nnueweights.c interrupt.c \
                 timeman.c history.c

SRC_TEXELTUNE  = texeltune.c bitboard.c magicbitboard.c attackgen.c \
                 move.c util.c position.c movegen.c \
                 tables.c timeman.c history.c interrupt.c \
                 moveorder.c transposition.c movepicker.c option.c \
                 search.c nnue.c endgame.c nnueweights.c kpk.c kpkp.c krkp.c

SRC_GENBITBASE = genbitbase.c bitboard.c magicbitboard.c attackgen.c \
                 move.c util.c position.c movegen.c \

SRC_BATCH      = batch.c bitboard.c magicbitboard.c attackgen.c \
                 move.c util.c position.c movegen.c

SRC_VISUALIZE  = visualize.c util.c

SRC_NNUESOURCE = nnuesource.c util.c

SRC_TESTBIT    = testbit.c testbitshared.c util.c
SRC_TESTBITD   = testbitd.c testbitshared.c util.c sprt.c
SRC_TESTBITN   = testbitn.c testbitshared.c util.c sprt.c

DEP = $(addprefix dep/,$(addsuffix .d,$(basename $(notdir $(wildcard src/*.c)))))

OBJ_BITBIT     = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_BITBIT))))
OBJ_GENNNUE    = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_GENNNUE))))
OBJ_GENEPD     = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_GENEPD))))
OBJ_NNUESOURCE = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_NNUESOURCE))))
OBJ_HISTOGRAM  = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_HISTOGRAM))))
OBJ_PGNBIN     = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_PGNBIN))))
OBJ_TEXELTUNE  = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_TEXELTUNE)))) obj/texelevaluate.o obj/texelpawn.o
OBJ_GENBITBASE = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_GENBITBASE))))
OBJ_BATCH      = $(addprefix obj/pic,$(addsuffix .o,$(basename $(SRC_BATCH))))
OBJ_VISUALIZE  = $(addprefix obj/pic,$(addsuffix .o,$(basename $(SRC_VISUALIZE))))
OBJ_TESTBIT    = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_TESTBIT))))
OBJ_TESTBITD   = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_TESTBITD))))
OBJ_TESTBITN   = $(addprefix obj/,$(addsuffix .o,$(basename $(SRC_TESTBITN))))

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

all: bitbit gennnue genepd histogram pgnbin texeltune genbitbase libbatch.so libvisualize.so testbit testbitd testbitn

bitbit: $(OBJ_BITBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

gennnue: $(OBJ_GENNNUE)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

genepd: $(OBJ_GENEPD)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

histogram: $(OBJ_HISTOGRAM)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

pgnbin: $(OBJ_PGNBIN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

texeltune: $(OBJ_TEXELTUNE)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

genbitbase: $(OBJ_GENBITBASE)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

libbatch.so: $(OBJ_BATCH)
	$(CC) $(LDFLAGS) -shared $^ $(LDLIBS) -o $@

libvisualize.so: $(OBJ_VISUALIZE)
	$(CC) $(LDFLAGS) -shared $^ $(LDLIBS) -o $@

testbit: $(OBJ_TESTBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

testbitd: $(OBJ_TESTBITD)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

testbitn: $(OBJ_TESTBITN)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

nnuesource: $(OBJ_NNUESOURCE)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

obj/nnueweights.o: src/nnueweights.c Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

obj/init.o: src/init.c dep/init.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -DVERSION=$(VERSION) -c $< -o $@

obj/interface.o: src/interface.c dep/interface.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -DTT=$(TT) -c $< -o $@

obj/texelevaluate.o: src/evaluate.c dep/evaluate.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -DTRACE -c $< -o $@

obj/gennnue.o: src/gennnue.c dep/gennnue.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude $(DSYZYGY) -c $< -o $@

obj/texelpawn.o: src/pawn.c dep/pawn.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -DTRACE -c $< -o $@

obj/texeltune.o: src/texeltune.c dep/texeltune.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -DTRACE -c $< -o $@

obj/pic%.o: src/%.c dep/%.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -fPIC -Iinclude -c $< -o $@

obj/%.o: src/%.c dep/%.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

src/nnueweights.c: nnuesource
	./nnuesource $(NNUE)

dep/nnueweights.d:

dep/%.d: src/%.c
	@mkdir -p dep
	@$(CC) -MM -MT "$(<:src/%.c=obj/%.o)" $(CFLAGS) -Iinclude $< -o $@

-include $(DEP)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f bitbit $(DESTDIR)$(BINDIR)/bitbit
	chmod 755 $(DESTDIR)$(BINDIR)/bitbit
	mkdir -p $(DESTDIR)$(MAN6DIR)
	sed "s/VERSION/$(VERSION)/g" < man/bitbit.6 > $(DESTDIR)$(MAN6DIR)/bitbit.6
	chmod 644 $(DESTDIR)$(MAN6DIR)/bitbit.6

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/bitbit $(DESTDIR)$(MAN6DIR)/bitbit.6

clean:
	rm -rf obj dep src/nnueweights.c

doc: doc/maximumlikelihood.pdf doc/elo.pdf

doc/%.pdf: doc/src/%.tex doc/src/%.bib
	latexmk -pdf -cd $< -output-directory=../../doc

options:
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "LDLIBS  = $(LDLIBS)"

.PHONY: all clean install uninstall doc options dep/nnueweights.d
.PRECIOUS: dep/%.d
.SUFFIXES: .c .h .tex .pdf
