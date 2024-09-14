# bitbit, a bitboard based chess engine written in c.
# Copyright (C) 2022-2024 Isak Ellmer
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

MAJOR      = 1
MINOR      = 2
VERSION   := $(MAJOR).$(MINOR)

MAKEFLAGS += -rR

ifeq (Windows_NT, $(OS))
	KERNEL := Windows_NT
	ARCH   := x86_64
else
	KERNEL    := $(shell uname -s)
	ARCH      := $(shell uname -m)
endif

ifneq ($(findstring ppc64, $(ARCH)), )
	ARCH = -mtune=native
else
	ARCH = -march=native -mtune=native
endif

ifeq (Windows_NT, $(KERNEL))
	LTO =
else
	LTO = -flto=auto
endif

MKDIR_P    = mkdir -p
RM         = rm
INSTALL    = install

CC         = cc
CSTANDARD  = -std=c11
CWARNINGS  = -Wall -Wextra -Wshadow -pedantic -Wno-unused-result -Wvla
COPTIMIZE  = -O3 $(ARCH) $(LTO)

ifeq ($(DEBUG), yes)
	CDEBUG = -g3 -ggdb
else ifeq ($(DEBUG), thread)
	CDEBUG = -g3 -ggdb -fsanitize=thread,undefined
else ifeq ($(DEBUG), address)
	CDEBUG = -g3 -ggdb -fsanitize=address,undefined
else ifeq ($(DEBUG), )
	CDEBUG = -DNDEBUG
endif

CFLAGS     = $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE) $(CDEBUG) -Iinclude
LDFLAGS    = $(CFLAGS)
LDLIBS     = -lm

ifeq ($(SIMD), avx2)
	CFLAGS += -DAVX2 -mavx2
endif

TT        ?= 256
NNUE      ?= etc/current.nnue

ifneq ($(SYZYGY), )
	LDLIBS  += -lfathom
	DSYZYGY := -DSYZYGY
endif

SRC_BASE      = bitboard.c magicbitboard.c attackgen.c move.c \
	        util.c position.c movegen.c
SRC           = $(SRC_BASE) perft.c search.c evaluate.c tables.c \
	        transposition.c init.c timeman.c pawn.c history.c \
		movepicker.c moveorder.c option.c endgame.c nnue.c \
		kpk.c kpkp.c krkp.c nnueweights.c io.c
SRC_ALL       = $(SRC_BASE) $(SRC) $(SRC_BIBIT) $(SRC_GENBIT) \
	        $(SRC_EPDBIT) $(SRC_HISTBIT) $(SRC_PGNBIT) \
	        $(SRC_TEXELBIT) $(SRC_BASEBIT) $(SRC_BATCHBIT) \
	        $(SRC_VISBIT) $(SRC_WNNUEBIT)
SRC_BITBIT    = bitbit.c interface.c thread.c bench.c $(SRC)
SRC_WEIGHTBIT = weightbit.c util.c io.c
SRC_GENBIT    = genbit.c $(SRC)
SRC_EPDBIT    = epdbit.c $(SRC)
SRC_HISTBIT   = histbit.c $(SRC)
SRC_PGNBIT    = pgnbit.c $(SRC)
SRC_TEXELBIT  = texelbit.c $(subst evaluate,texel-evaluate,\
	        $(subst pawn,texel-pawn,$(SRC)))
SRC_BASEBIT   = basebit.c $(SRC_BASE)
SRC_CONVBIT   = convbit.c io.c $(SRC_BASE)
SRC_BATCHBIT  = $(addprefix pic-,batchbit.c $(SRC_BASE) io.c)
SRC_VISBIT    = pic-visbit.c pic-util.c pic-io.c

DEP = $(sort $(patsubst %.c,dep/%.d,$(SRC_ALL)))

OBJ_BITBIT    = $(patsubst %.c,obj/%.o,$(SRC_BITBIT))
OBJ_WEIGHTBIT = $(patsubst %.c,obj/%.o,$(SRC_WEIGHTBIT))
OBJ_GENBIT    = $(patsubst %.c,obj/%.o,$(SRC_GENBIT))
OBJ_EPDBIT    = $(patsubst %.c,obj/%.o,$(SRC_EPDBIT))
OBJ_HISTBIT   = $(patsubst %.c,obj/%.o,$(SRC_HISTBIT))
OBJ_PGNBIT    = $(patsubst %.c,obj/%.o,$(SRC_PGNBIT))
OBJ_TEXELBIT  = $(patsubst %.c,obj/%.o,$(SRC_TEXELBIT))
OBJ_BASEBIT   = $(patsubst %.c,obj/%.o,$(SRC_BASEBIT))
OBJ_CONVBIT   = $(patsubst %.c,obj/%.o,$(SRC_CONVBIT))
OBJ_BATCHBIT  = $(patsubst %.c,obj/%.o,$(SRC_BATCHBIT))
OBJ_VISBIT    = $(patsubst %.c,obj/%.o,$(SRC_VISBIT))
OBJ_TUNEBIT   = $(patsubst %.c,obj/%.o,$(subst search,tune-search,\
		$(subst option,tune-option,$(subst timeman,tune-timeman,\
		$(SRC_BITBIT) tune.c))))

BIN = bitbit weightbit genbit epdbit histbit pgnbit \
      texelbit basebit libbatchbit.so libvisbit.so tunebit convbit

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib64
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

all: bitbit

everything: $(BIN)

bitbit genbit libbatchbit.so: LDLIBS += -lpthread
bitbit: $(OBJ_BITBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
weightbit: $(OBJ_WEIGHTBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
genbit: $(OBJ_GENBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
epdbit: $(OBJ_EPDBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
histbit: $(OBJ_HISTBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
pgnbit: $(OBJ_PGNBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
texelbit: $(OBJ_TEXELBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
basebit: $(OBJ_BASEBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
convbit: $(OBJ_CONVBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
libbatchbit.so: $(OBJ_BATCHBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
libvisbit.so: $(OBJ_VISBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
tunebit: $(OBJ_TUNEBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

obj/%.o: src/%.c dep/%.d
	@$(MKDIR_P) obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/pic-%.o: src/%.c dep/%.d
	@$(MKDIR_P) obj
	$(CC) $(CFLAGS) -fPIC -c $< -o $@
obj/texel-%.o: src/%.c dep/%.d
	@$(MKDIR_P) obj
	$(CC) $(CFLAGS) -DTRACE -c $< -o $@
obj/tune-%.o: src/%.c dep/%.d
	@$(MKDIR_P) obj
	$(CC) $(CFLAGS) -DTUNE -c $< -o $@

src/nnueweights.c: weightbit Makefile
	./weightbit $(NNUE)

%.so:                            LDFLAGS += -shared

obj/thread.o obj/pic-batchbit.o: CFLAGS += -pthread
obj/genbit.o:                    CFLAGS += $(DSYZYGY) -pthread
obj/init.o obj/interface.o:      CFLAGS += -DVERSION=$(VERSION)
obj/interface.o obj/option.o obj/tune-option.o: CFLAGS += -DTT=$(TT)

dep/nnueweights.d:
	@$(MKDIR_P) dep
	@touch dep/nnueweights.d
dep/%.d: src/%.c Makefile
	@$(MKDIR_P) dep
	@$(CC) -MM -MT "$@ $(<:src/%.c=obj/%.o)" $(CFLAGS) $< -o $@

install: all
	$(MKDIR_P) $(DESTDIR)$(BINDIR)
	$(MKDIR_P) $(DESTDIR)$(MAN6DIR)
	$(INSTALL) -m 0755 bitbit $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0644 man/bitbit.6 $(DESTDIR)$(MAN6DIR)

install-everything: everything install
	$(INSTALL) -m 0755 {epd,pgn,texel}bit $(DESTDIR)$(BINDIR)
	$(MKDIR_P) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0755 lib{batch,vis}bit.so $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0644 man/{epd,pgn,texel}bit.6 $(DESTDIR)$(MAN6DIR)

uninstall:
	$(RM) -f $(DESTDIR)$(BINDIR)/{bit,epd,pgn,texel}bit
	$(RM) -f $(DESTDIR)$(MAN6DIR)/{bit,epd,pgn,texel}bit.6
	$(RM) -f $(DESTDIR)$(LIBDIR)/lib{batch,vis}bit.so

clean:
	$(RM) -rf obj dep
	$(RM) -f src/nnueweights.c $(BIN)

-include $(DEP)
.PRECIOUS: dep/%.d
.SUFFIXES: .c .h .d
.PHONY: all everything clean install uninstall
