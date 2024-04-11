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
MINOR      = 0
VERSION   := $(MAJOR).$(MINOR)

MAKEFLAGS += -rR

KERNEL    := $(shell uname -s)
ARCH      := $(shell uname -m)
ifneq ($(findstring ppc64, $(ARCH)), )
	ARCH = -mtune=native
else
	ARCH = -march=native
endif

MKDIR_P    = mkdir -p
RM         = rm
INSTALL    = install

CC         = cc
CSTANDARD  = -std=c11
CWARNINGS  = -Wall -Wextra -Wshadow -pedantic -Wno-unused-result -Wvla
COPTIMIZE  = -O3 $(ARCH) -flto=auto

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

CFLAGS     = $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE) $(CDEBUG) -Iinclude
LDFLAGS    = $(CFLAGS)
LDLIBS     = -lm

ifeq ($(SIMD), avx2)
	CFLAGS += -DAVX2 -mavx2
endif
ifeq ($(SIMD), sse4)
	CFLAGS += -DSSE4 -msse4
endif
ifeq ($(SIMD), sse2)
	CFLAGS += -DSSE2 -msse2
endif

TT        ?= 64
NNUE      ?= etc/current.nnue

ifneq ($(SYZYGY), )
	LDLIBS  += -lfathom
	DSYZYGY := -DSYZYGY=$(SYZYGY)
endif

SRC_BASE      = bitboard.c magicbitboard.c attackgen.c move.c \
	        util.c position.c movegen.c
SRC           = $(SRC_BASE) perft.c search.c evaluate.c tables.c \
	        interface.c transposition.c init.c timeman.c \
	        interrupt.c pawn.c history.c movepicker.c \
	        moveorder.c option.c endgame.c nnue.c kpk.c \
	        kpkp.c krkp.c nnueweights.c
SRC_ALL       = $(SRC_BASE) $(SRC) $(SRC_BIBIT) $(SRC_GENBIT) \
	        $(SRC_EPDBIT) $(SRC_HISTBIT) $(SRC_PGNBIT) \
	        $(SRC_TEXELBIT) $(SRC_BASEBIT) $(SRC_BATCHBIT) \
	        $(SRC_VISBIT) $(SRC_WNNUEBIT)
SRC_BITBIT    = bitbit.c $(SRC)
SRC_GENBIT    = genbit.c $(SRC)
SRC_EPDBIT    = epdbit.c $(SRC)
SRC_HISTBIT   = histbit.c $(SRC)
SRC_PGNBIT    = pgnbit.c $(SRC)
SRC_TEXELBIT  = texelbit.c $(subst evaluate,texel-evaluate,\
	        $(subst pawn,texel-pawn,$(SRC)))
SRC_BASEBIT   = basebit.c $(SRC_BASE)
SRC_BATCHBIT  = $(addprefix pic-,batchbit.c $(SRC_BASE))
SRC_VISBIT    = pic-visbit.c pic-util.c
SRC_WEIGHTBIT = weightbit.c util.c

DEP = $(sort $(patsubst %.c,dep/%.d,$(SRC_ALL)))

OBJ_bitbit         = $(patsubst %.c,obj/%.o,$(SRC_BITBIT))
OBJ_genbit         = $(patsubst %.c,obj/%.o,$(SRC_GENBIT))
OBJ_epdbit         = $(patsubst %.c,obj/%.o,$(SRC_EPDBIT))
OBJ_weightbit      = $(patsubst %.c,obj/%.o,$(SRC_WEIGHTBIT))
OBJ_histbit        = $(patsubst %.c,obj/%.o,$(SRC_HISTBIT))
OBJ_pgnbit         = $(patsubst %.c,obj/%.o,$(SRC_PGNBIT))
OBJ_texelbit       = $(patsubst %.c,obj/%.o,$(SRC_TEXELBIT))
OBJ_basebit        = $(patsubst %.c,obj/%.o,$(SRC_BASEBIT))
OBJ_libbatchbit.so = $(patsubst %.c,obj/%.o,$(SRC_BATCHBIT))
OBJ_libvisbit.so   = $(patsubst %.c,obj/%.o,$(SRC_VISBIT))

BIN = bitbit weightbit genbit epdbit histbit pgnbit \
      texelbit basebit libbatchbit.so libvisbit.so

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

all: bitbit

everything: $(BIN)

.SECONDEXPANSION:
$(BIN): $$(OBJ_$$@)
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

src/nnueweights.c: weightbit Makefile
	./weightbit $(NNUE)

genbit:       LDLIBS += -pthread
%.so:           LDFLAGS += -shared

obj/genbit.o: CFLAGS += $(DSYZYGY) -pthread
obj/init.o:     CFLAGS += -DVERSION=$(VERSION)
obj/interface.o obj/option.o: CFLAGS += -DTT=$(TT)

dep/nnueweights.d:
	@$(MKDIR_P) dep
	@touch dep/nnueweights.d
dep/%.d: src/%.c Makefile
	@$(MKDIR_P) dep
	@$(CC) -MM -MT "$@ $(<:src/%.c=obj/%.o)" $(CFLAGS) $< -o $@

install: all
	$(MKDIR_P) $(DESTDIR)$(BINDIR)
	$(MKDIR_P) $(DESTDIR)$(MAN6DIR)
	$(INSTALL) -m 0755 {bitbit,epdbit,pgnbit} $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0644 man/{bitbit,epdbit,pgnbit}.6 $(DESTDIR)$(MAN6DIR)

uninstall:
	$(RM) -f $(DESTDIR)$(BINDIR)/{bitbit,epdbit,pgnbit}
	$(RM) -f $(DESTDIR)$(MAN6DIR)/{bitbit,epdbit,pgnbit}.6

clean:
	$(RM) -rf obj dep
	$(RM) -f src/nnueweights.c $(BIN)

-include $(DEP)
.PRECIOUS: dep/%.d
.SUFFIXES: .c .h .d
.PHONY: all everything clean install uninstall
