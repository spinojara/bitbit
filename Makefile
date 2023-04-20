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

SRC_BITBIT = bitboard.c magic_bitboard.c attack_gen.c \
             move.c util.c position.c move_gen.c perft.c \
             search.c evaluate.c interface.c \
             transposition_table.c init.c time_man.c \
             interrupt.c pawn.c history.c nnue.c bitbit.c

SRC_GENFEN = bitboard.c magic_bitboard.c attack_gen.c \
             move.c util.c position.c move_gen.c perft.c \
             evaluate.c init.c time_man.c \
             interrupt.c pawn.c history.c genfen.c

SRC_BATCH  = bitboard.c magic_bitboard.c attack_gen.c \
             move.c util.c position.c move_gen.c init.c \
             batch.c

SRC = $(SRC_BITBIT) $(SRC_GENFEN) $(SRC_BATCH)

OBJ_BITBIT = $(addprefix obj/,$(SRC_BITBIT:.c=.o)) obj/incbin.o
OBJ_GENFEN = $(addprefix obj/,$(SRC_GENFEN:.c=.o)) obj/genfensearch.o
OBJ_BATCH  = $(addprefix obj/pic,$(SRC_BATCH:.c=.o))

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

all: bitbit genfen libbatch.so

bitbit: $(OBJ_BITBIT)
	$(CC) $(LDFLAGS) -lm $^ -o $@

genfen: $(OBJ_GENFEN)
	$(CC) $(LDFLAGS) -lm -pthread $^ -o $@

libbatch.so: $(OBJ_BATCH)
	$(CC) $(LDFLAGS) -shared $^ -o $@

obj/init.o: src/init.c Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude $(DVERSION) -c $< -o $@

obj/interface.o: src/interface.c dep/interface.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

obj/transposition_table.o: src/transposition_table.c dep/transposition_table.d Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DTT=$(TT) -Iinclude -c $< -o $@

obj/incbin.o: src/incbin.S Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DNNUE=\"$(NNUE)\" -c $< -o $@

obj/search.o: src/search.c Makefile
	@mkdir -p obj
	$(CC) $(CFLAGS) -DNNUE -DTRANSPOSITION -Iinclude -c $< -o $@

obj/genfensearch.o: src/search.c Makefile
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
