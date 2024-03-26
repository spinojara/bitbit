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
COPTIMIZE  = -O3 $(ARCH) -flto

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
	CFLAGS  += -DAVX2 -mavx2
endif
ifeq ($(SIMD), sse4)
	CFLAGS  += -DSSE4 -msse4
endif
ifeq ($(SIMD), sse2)
	CFLAGS  += -DSSE2 -msse2
endif

TT        ?= 64
NNUE      ?= etc/current.nnue

ifneq ($(SYZYGY), )
	LDLIBS  += -lfathom
	DSYZYGY := -DSYZYGY=$(SYZYGY)
endif

SRC_BASE       = bitboard.c magicbitboard.c attackgen.c move.c \
		 util.c position.c movegen.c
SRC            = $(SRC_BASE) perft.c search.c evaluate.c tables.c \
		 interface.c transposition.c init.c timeman.c \
		 interrupt.c pawn.c history.c movepicker.c \
		 moveorder.c option.c endgame.c nnue.c kpk.c \
		 kpkp.c krkp.c nnueweights.c
SRC_ALL        = $(SRC_BASE) $(SRC) $(SRC_BIBIT) $(SRC_GENNNUE) \
		 $(SRC_GENEPD) $(SRC_HISTOGRAM) $(SRC_PGNBIN) \
		 $(SRC_TEXELTUNE) $(SRC_GENBITBASE) $(SRC_BATCH) \
		 $(SRC_VISUALIZE) $(SRC_NNUESOURCE) $(SRC_TESTBIT) \
		 $(SRC_TESTBITD) $(SRC_TESTBITN)
SRC_BITBIT     = bitbit.c $(SRC)
SRC_GENNNUE    = gennnue.c $(SRC)
SRC_GENEPD     = genepd.c $(SRC)
SRC_HISTOGRAM  = histogram.c $(SRC)
SRC_PGNBIN     = pgnbin.c $(SRC)
SRC_TEXELTUNE  = texeltune.c $(subst evaluate,texel-evaluate,\
		 $(subst pawn,texel-pawn,$(SRC)))
SRC_GENBITBASE = genbitbase.c $(SRC_BASE)
SRC_BATCH      = $(addprefix pic-,batch.c $(SRC_BASE))
SRC_VISUALIZE  = pic-visualize.c pic-util.c
SRC_NNUESOURCE = nnuesource.c util.c
SRC_TESTBIT    = testbit.c testbitshared.c util.c
SRC_TESTBITD   = testbitd.c testbitshared.c util.c sprt.c
SRC_TESTBITN   = testbitn.c testbitshared.c util.c sprt.c

DEP = $(sort $(patsubst %.c,dep/%.d,$(SRC_ALL)))

OBJ_bitbit          = $(patsubst %.c,obj/%.o,$(SRC_BITBIT))
OBJ_gennnue         = $(patsubst %.c,obj/%.o,$(SRC_GENNNUE))
OBJ_genepd          = $(patsubst %.c,obj/%.o,$(SRC_GENEPD))
OBJ_nnuesource      = $(patsubst %.c,obj/%.o,$(SRC_NNUESOURCE))
OBJ_histogram       = $(patsubst %.c,obj/%.o,$(SRC_HISTOGRAM))
OBJ_pgnbin          = $(patsubst %.c,obj/%.o,$(SRC_PGNBIN))
OBJ_texeltune       = $(patsubst %.c,obj/%.o,$(SRC_TEXELTUNE))
OBJ_genbitbase      = $(patsubst %.c,obj/%.o,$(SRC_GENBITBASE))
OBJ_libbatch.so     = $(patsubst %.c,obj/%.o,$(SRC_BATCH))
OBJ_libvisualize.so = $(patsubst %.c,obj/%.o,$(SRC_VISUALIZE))
OBJ_testbit         = $(patsubst %.c,obj/%.o,$(SRC_TESTBIT))
OBJ_testbitd        = $(patsubst %.c,obj/%.o,$(SRC_TESTBITD))
OBJ_testbitn        = $(patsubst %.c,obj/%.o,$(SRC_TESTBITN))

BIN            = bitbit nnuesource gennnue genepd histogram pgnbin \
		 texeltune genbitbase libbatch.so libvisualize.so \
		 testbit testbitd testbitn

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
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/pic-%.o: src/%.c dep/%.d
	@mkdir -p obj
	$(CC) $(CFLAGS) -fPIC -c $< -o $@
obj/texel-%.o: src/%.c dep/%.d
	@mkdir -p obj
	$(CC) $(CFLAGS) -DTRACE -c $< -o $@

src/nnueweights.c: nnuesource Makefile
	./nnuesource $(NNUE)

gennnue:         LDLIBS += -pthread
testbit:         LDLIBS += -lssl -lcrypto
testbitd:        LDLIBS += -lssl -lcrypto -lsqlite3
testbitn:        LDLIBS += -lssl -lcrypto
%.so:            LDFLAGS += -shared

obj/gennnue.o:   CFLAGS += $(DSYZYGY) -pthread
obj/init.o:      CFLAGS += -DVERSION=$(VERSION)
obj/interface.o obj/option.o: CFLAGS += -DTT=$(TT)

dep/nnueweights.d:
	@mkdir -p dep
	@touch dep/nnueweights.d
dep/%.d: src/%.c Makefile
	@mkdir -p dep
	@$(CC) -MM -MT "$@ $(<:src/%.c=obj/%.o)" $(CFLAGS) $< -o $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f bitbit $(DESTDIR)$(BINDIR)/bitbit
	chmod 755 $(DESTDIR)$(BINDIR)/bitbit
	mkdir -p $(DESTDIR)$(MAN6DIR)
	sed "s/VERSION/$(VERSION)/g" < man/bitbit.6 > $(DESTDIR)$(MAN6DIR)/bitbit.6
	chmod 644 $(DESTDIR)$(MAN6DIR)/bitbit.6

install-everything: everything install
	mkdir -p $(DESTDIR)/var/lib/testbit
	cp -f testbit $(DESTDIR)$(BINDIR)/testbit
	chmod 755 $(DESTDIR)$(BINDIR)/testbit
	cp -f testbitd $(DESTDIR)$(BINDIR)/testbitd
	chmod 755 $(DESTDIR)$(BINDIR)/testbitd
	cp -f testbitn $(DESTDIR)$(BINDIR)/testbitn
	chmod 755 $(DESTDIR)$(BINDIR)/testbitn

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/{bitbit,testbit,testbitd,testbitn}
	rm -f $(DESTDIR)$(MAN6DIR)/bitbit.6
	rm -rf $(DESTDIR)/var/lib/testbit

clean:
	rm -rf obj dep
	rm -f src/nnueweights.c $(BIN)

-include $(DEP)
.PRECIOUS: dep/%.d
.SUFFIXES: .c .h .d
.PHONY: all everything clean install install-everything uninstall
