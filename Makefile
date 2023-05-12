MAJOR = 1
MINOR = 0
VERSION = $(MAJOR).$(MINOR)

CC = cc
CSTANDARD = -std=c11
CWARNINGS = -Wall -Wextra -Wshadow -pedantic
ARCH = native
COPTIMIZE = -O2 -march=$(ARCH) -flto
CFLAGS = $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE)
LDFLAGS = $(CFLAGS)

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

DVERSION = -DVERSION=$(VERSION)

SRC_BITBIT    = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c perft.c \
                search.c evaluate.c tables.c interface.c \
                transposition.c init.c timeman.c interrupt.c \
                pawn.c history.c nnue.c moveorder.c \
                material.c bitbit.c

SRC_NNUEGEN   = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c evaluate.c \
                tables.c init.c timeman.c interrupt.c pawn.c \
                moveorder.c material.c nnuegen.c

SRC_TEXELGEN  = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c init.c \
                texelgen.c

SRC_TEXELTUNE = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c evaluate.c \
                tables.c init.c timeman.c interrupt.c pawn.c \
                moveorder.c texeltune.c

SRC_BATCH     = bitboard.c magicbitboard.c attackgen.c \
                move.c util.c position.c movegen.c init.c \
                batch.c

SRC = $(SRC_BITBIT) $(SRC_NNUEGEN) $(SRC_TEXELGEN) $(SRC_TEXELTUNE) $(SRC_BATCH)

OBJ_BITBIT    = $(addprefix obj/,$(SRC_BITBIT:.c=.o)) obj/nnueincbin.o
OBJ_NNUEGEN   = $(addprefix obj/,$(SRC_NNUEGEN:.c=.o)) obj/gensearch.o
OBJ_TEXELGEN  = $(addprefix obj/,$(SRC_TEXELGEN:.c=.o))
OBJ_TEXELTUNE = $(addprefix obj/,$(SRC_TEXELTUNE:.c=.o)) obj/gensearch.o
OBJ_BATCH     = $(addprefix obj/pic,$(SRC_BATCH:.c=.o))

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

all: bitbit nnuegen texelgen texeltune libbatch.so

bitbit: $(OBJ_BITBIT)
	$(CC) $(LDFLAGS) -lm $^ -o $@

nnuegen: $(OBJ_NNUEGEN)
	$(CC) $(LDFLAGS) -lm -pthread $^ -o $@

texelgen: $(OBJ_TEXELGEN)
	$(CC) $(LDFLAGS) -lm -pthread $^ -o $@

texeltune: $(OBJ_TEXELTUNE)
	$(CC) $(LDFLAGS) -lm -pthread $^ -o $@

libbatch.so: $(OBJ_BATCH)
	$(CC) $(LDFLAGS) -shared $^ -o $@

obj/init.o: src/init.c dep/init.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude $(DVERSION) -c $< -o $@

obj/interface.o: src/interface.c dep/interface.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

obj/transposition.o: src/transposition.c dep/transposition.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DTT=$(TT) -Iinclude -c $< -o $@

obj/nnueincbin.o: src/incbin.S Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DFILE=\"$(NNUE)\" -c $< -o $@

obj/search.o: src/search.c dep/search.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DNNNUE -DTRANSPOSITION -Iinclude -c $< -o $@

obj/moveorder.o: src/moveorder.c dep/moveorder.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DTRANSPOSITION -Iinclude -c $< -o $@

obj/gensearch.o: src/search.c dep/search.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

obj/pic%.o: src/%.c dep/%.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -fPIC -Iinclude -c $< -o $@

obj/%.o: src/%.c dep/%.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

dep/%.d: src/%.c
	@mkdir -p dep
	@$(CC) -MM -MT "$(<:src/%.c=obj/%.o)" $(CFLAGS) -DNNUE -DTRANSPOSITION -Iinclude $< -o $@

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
