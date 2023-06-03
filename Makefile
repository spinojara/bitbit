MAJOR = 1
MINOR = 0
VERSION = $(MAJOR).$(MINOR)

CC = cc
CSTANDARD = -std=c11
CWARNINGS = -Wall -Wextra -Wshadow -pedantic -Wno-unused-result
ARCH = native

ifeq ($(DEBUG), yes)
	CDEBUG = -fsanitize=address -fsanitize=undefined -g3 -ggdb
	COPTIMIZE =
else
	CDEBUG = -DNDEBUG
	COPTIMIZE = -O2 -march=$(ARCH) -flto
endif

CFLAGS = $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE) $(CDEBUG)

ifeq ($(SIMD), avx2)
	CFLAGS += -DAVX2 -mavx2
endif
ifeq ($(SIMD), sse4)
	CFLAGS += -DSSE4 -msse4
endif
ifeq ($(SIMD), sse2)
	CFLAGS += -DSSE2 -msse2
endif

ifeq ($(TT), )
	TT = 26
endif
ifeq ($(NNUE), )
	NNUE = files/current.nnue
endif

LDFLAGS = $(CFLAGS)

DVERSION = -DVERSION=$(VERSION)

SRC_BITBIT    = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c perft.c \
                search.c evaluate.c tables.c interface.c \
                transposition.c init.c timeman.c interrupt.c \
                pawn.c history.c movepicker.c material.c \
                moveorder.c option.c bitbit.c

SRC_NNUEGEN   = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c evaluate.c \
                tables.c init.c timeman.c interrupt.c pawn.c \
                moveorder.c material.c transposition.c \
                movepicker.c history.c option.c nnue.c search.c \
                nnuegen.c

SRC_HISTOGRAM = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c evaluate.c \
                tables.c init.c timeman.c interrupt.c pawn.c \
                moveorder.c material.c transposition.c \
                movepicker.c history.c option.c nnue.c search.c \
                histogram.c

SRC_PGNBIN    = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c init.c \
                option.c transposition.c pgnbin.c

SRC_TEXELTUNE = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c evaluate.c \
                tables.c init.c timeman.c history.c \
                interrupt.c pawn.c moveorder.c transposition.c \
                movepicker.c option.c search.c nnue.c texeltune.c

SRC_BATCH     = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c init.c \
                batch.c

SRC = $(SRC_BITBIT) $(SRC_NNUEGEN) $(SRC_HISTOGRAM) $(SRC_PGNBIN) $(SRC_TEXELTUNE) $(SRC_BATCH)

OBJ_BITBIT    = $(addprefix obj/,$(SRC_BITBIT:.c=.o)) obj/incbin.o obj/incbinnnue.o
OBJ_NNUEGEN   = $(addprefix obj/,$(SRC_NNUEGEN:.c=.o))
OBJ_HISTOGRAM = $(addprefix obj/,$(SRC_HISTOGRAM:.c=.o))
OBJ_PGNBIN    = $(addprefix obj/,$(SRC_PGNBIN:.c=.o))
OBJ_TEXELTUNE = $(addprefix obj/,$(SRC_TEXELTUNE:.c=.o))
OBJ_BATCH     = $(addprefix obj/pic,$(SRC_BATCH:.c=.o))

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

all: bitbit nnuegen histogram pgnbin texeltune libbatch.so

bitbit: $(OBJ_BITBIT)
	$(CC) $(LDFLAGS) -lm $^ -o $@

nnuegen: $(OBJ_NNUEGEN)
	$(CC) $(LDFLAGS) -lm -pthread $^ -o $@

histogram: $(OBJ_HISTOGRAM)
	$(CC) $(LDFLAGS) -lm $^ -o $@

pgnbin: $(OBJ_PGNBIN)
	$(CC) $(LDFLAGS) -lm -pthread $^ -o $@

texeltune: $(OBJ_TEXELTUNE)
	$(CC) $(LDFLAGS) -lm -pthread $^ -o $@

libbatch.so: $(OBJ_BATCH)
	$(CC) $(LDFLAGS) -shared $^ -o $@

batch: $(OBJ_BATCH)
	$(CC) $(LDFLAGS) $^ -o $@

obj/init.o: src/init.c dep/init.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude $(DVERSION) -c $< -o $@

obj/transposition.o: src/transposition.c dep/transposition.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DTT=$(TT) -Iinclude -c $< -o $@

obj/incbin.o: src/incbin.S Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DFILE=\"$(NNUE)\" -c $< -o $@

obj/incbinnnue.o: src/nnue.c Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DINCBIN -Iinclude -c $< -o $@

obj/pic%.o: src/%.c dep/%.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -fPIC -Iinclude -c $< -o $@

obj/%.o: src/%.c dep/%.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

dep/%.d: src/%.c
	@mkdir -p dep
	@$(CC) -MM -MT "$(<:src/%.c=obj/%.o)" $(CFLAGS) -Iinclude $< -o $@

-include $(addprefix dep/,$(SRC:.c=.d))

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
	rm -rf obj dep

options:
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"

.PHONY: all clean install uninstall options
.SECONDARY:
