# bitbit, a bitboard based chess engine written in c.
# Copyright (C) 2022-2025 Isak Ellmer
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
MINOR      = 5
VERSION   := $(MAJOR).$(MINOR)

MAKEFLAGS += -rR

ARCH      ?= native
ifneq ($(ARCH), )
	CARCH = -march=$(ARCH)
endif

MKDIR_P    = mkdir -p
RM         = rm
INSTALL    = install

CC         = clang
CSTANDARD  = -std=c11
CWARNINGS  = -Wall -Wextra -Wshadow -pedantic -Wno-unused-result -Wvla
COPTIMIZE  = -O3 $(CARCH)

ifeq ($(DEBUG), yes)
	CDEBUG = -g3 -ggdb
else ifeq ($(DEBUG), thread)
	CDEBUG = -g3 -ggdb -fsanitize=thread,undefined
else ifeq ($(DEBUG), address)
	CDEBUG = -g3 -ggdb -fsanitize=address,undefined
else ifeq ($(DEBUG), )
	CDEBUG = -DNDEBUG
endif

ifneq ($(TARGET), )
	CTARGET = -target $(TARGET)
endif

CFLAGS     = $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE) $(CDEBUG) $(CTARGET) -Iinclude -pthread $(EXTRACFLAGS)

ifeq ($(SIMD), avx2)
	CFLAGS += -DAVX2 -mavx2
else ifeq ($(SIMD), vnni)
	CFLAGS += -DVNNI -mavxvnni -mavx2
endif

ifeq ($(CC), clang)
	CFLAGS += -flto=full
else ifeq ($(CC), gcc)
	CFLAGS += -flto -flto-partition=one
endif

LDFLAGS    = $(CFLAGS) $(EXTRALDFLAGS)

ifneq ($(DEBUG), )
	LDFLAGS += -rdynamic
endif

ifneq ($(STATIC), )
	LDFLAGS += -static
endif

LDLIBS     = -lm -lpthread

TT        ?= 256
NNUE      ?= etc/current.nnue

ifneq ($(SYZYGY), )
	LDLIBS += -lfathom
	DSYZYGY = -DSYZYGY
endif

SRC_BASE      = bitboard.c magicbitboard.c attackgen.c move.c \
	        util.c position.c movegen.c
SRC           = $(SRC_BASE) perft.c search.c evaluate.c \
	        transposition.c init.c timeman.c history.c \
		movepicker.c moveorder.c option.c endgame.c nnue.c \
		nnuefile.c kpk.c kpkp.c krkp.c nnueweights.c io.c
SRC_ALL       = $(SRC_BASE) $(SRC) $(SRC_BIBIT) \
	        $(SRC_EPDBIT) $(SRC_HISTBIT) $(SRC_PGNBIT) \
	        $(SRC_BASEBIT) $(SRC_BATCHBIT) \
	        $(SRC_VISBIT) $(SRC_WNNUEBIT)
SRC_BITBIT    = bitbit.c interface.c thread.c bench.c $(SRC)
SRC_WEIGHTBIT = weightbit.c util.c io.c nnuefile.c
SRC_EPDBIT    = epdbit.c $(SRC)
SRC_HISTBIT   = histbit.c $(SRC)
SRC_PGNBIT    = pgnbit.c $(SRC)
SRC_BASEBIT   = basebit.c $(SRC_BASE)
SRC_PLAYBIT   = playbit.c polyglot.c $(SRC)
SRC_CONVBIT   = convbit.c io.c $(SRC_BASE)
SRC_BATCHBIT  = $(addprefix pic-,batchbit.c io.c $(SRC_BASE))
SRC_VISBIT    = $(addprefix pic-,visbit.c io.c)
SRC_CHECKBIT  = checkbit.c io.c $(SRC_BASE)

DEP           = $(sort $(patsubst %.c,dep/%.d,$(SRC_ALL)))

OBJ_BITBIT    = $(patsubst %.c,obj/%.o,$(SRC_BITBIT))
OBJ_WEIGHTBIT = $(patsubst %.c,obj/%.o,$(SRC_WEIGHTBIT))
OBJ_EPDBIT    = $(patsubst %.c,obj/%.o,$(SRC_EPDBIT))
OBJ_HISTBIT   = $(patsubst %.c,obj/%.o,$(SRC_HISTBIT))
OBJ_PGNBIT    = $(patsubst %.c,obj/%.o,$(SRC_PGNBIT))
OBJ_BASEBIT   = $(patsubst %.c,obj/%.o,$(SRC_BASEBIT))
OBJ_PLAYBIT   = $(patsubst %.c,obj/%.o,$(SRC_PLAYBIT))
OBJ_CONVBIT   = $(patsubst %.c,obj/%.o,$(SRC_CONVBIT))
OBJ_BATCHBIT  = $(patsubst %.c,obj/%.o,$(SRC_BATCHBIT))
OBJ_VISBIT    = $(patsubst %.c,obj/%.o,$(SRC_VISBIT))
OBJ_TUNEBIT   = $(patsubst %.c,obj/%.o,$(subst search,tune-search,\
		$(subst option,tune-option,$(subst timeman,tune-timeman,\
		$(subst movepicker,tune-movepicker,\
		$(SRC_BITBIT) tune.c)))))
OBJ_CHECKBIT  = $(patsubst %.c,obj/%.o,$(SRC_CHECKBIT))

BIN = bitbit weightbit epdbit histbit pgnbit basebit \
      libbatchbit.so libvisbit.so tunebit convbit checkbit playbit

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib64
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

all: bitbit

nnue: nnueclean bitbit

bitbit-pgo: objclean pgoclean
	$(MAKE) CC=$(CC) ARCH=$(ARCH) DEBUG=$(DEBUG) TARGET=$(TARGET) $(CC)-pgo

clang-pgo:
	$(MAKE) CC=clang ARCH=$(ARCH) DEBUG=$(DEBUG) TARGET=$(TARGET) EXTRACFLAGS="-fprofile-generate" bitbit
	./bitbit bench , quit > /dev/null
	llvm-profdata merge *.profraw -output=bitbit.profdata
	$(MAKE) CC=clang ARCH=$(ARCH) DEBUG=$(DEBUG) TARGET=$(TARGET) objclean
	$(MAKE) CC=clang ARCH=$(ARCH) DEBUG=$(DEBUG) TARGET=$(TARGET) EXTRACFLAGS="-fprofile-use=bitbit.profdata" bitbit

gcc-pgo:
	$(MAKE) CC=gcc ARCH=$(ARCH) DEBUG=$(DEBUG) TARGET=$(TARGET) EXTRACFLAGS="-fprofile-generate=profdir" bitbit
	./bitbit bench , quit > /dev/null
	$(MAKE) CC=gcc ARCH=$(ARCH) DEBUG=$(DEBUG) TARGET=$(TARGET) objclean
	$(MAKE) CC=gcc ARCH=$(ARCH) DEBUG=$(DEBUG) TARGET=$(TARGET) EXTRACFLAGS="-fprofile-use=profdir" bitbit

everything: $(BIN)

bitbit: $(OBJ_BITBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
weightbit: $(OBJ_WEIGHTBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
epdbit: $(OBJ_EPDBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
histbit: $(OBJ_HISTBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
pgnbit: $(OBJ_PGNBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
basebit: $(OBJ_BASEBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
playbit: $(OBJ_PLAYBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
convbit: $(OBJ_CONVBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
libbatchbit.so: $(OBJ_BATCHBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
libvisbit.so: $(OBJ_VISBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
tunebit: $(OBJ_TUNEBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
checkbit: $(OBJ_CHECKBIT)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

obj/%.o: src/%.c dep/%.d
	@$(MKDIR_P) obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/pic-%.o: src/%.c dep/%.d
	@$(MKDIR_P) obj
	$(CC) $(CFLAGS) -fPIC -c $< -o $@
obj/tune-%.o: src/%.c dep/%.d
	@$(MKDIR_P) obj
	$(CC) $(CFLAGS) -DTUNE -c $< -o $@

src/nnueweights.c: weightbit Makefile
	./weightbit $(NNUE)

%.so:                            LDFLAGS += -shared

obj/playbit.o:                   CFLAGS += $(DSYZYGY)
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
	$(INSTALL) -m 0755 {epd,pgn,check}bit $(DESTDIR)$(BINDIR)
	$(MKDIR_P) $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0755 lib{batch,vis}bit.so $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0644 man/{epd,pgn}bit.6 $(DESTDIR)$(MAN6DIR)

uninstall:
	$(RM) -f $(DESTDIR)$(BINDIR)/{bit,epd,pgn,check}bit
	$(RM) -f $(DESTDIR)$(MAN6DIR)/{bit,epd,pgn}bit.6
	$(RM) -f $(DESTDIR)$(LIBDIR)/lib{batch,vis}bit.so

clean: objclean nnueclean pgoclean

nnueclean:
	$(RM) -f src/nnueweights.c

pgoclean:
	$(RM) -rf bitbit.profdata *.profraw profdir

objclean:
	$(RM) -rf obj dep $(BIN)

-include $(DEP)
.PRECIOUS: dep/%.d
.SUFFIXES: .c .h .d
.PHONY: all everything clean nnueclean pgoclean objclean install uninstall bitbit-pgo clang-pgo gcc-pgo
