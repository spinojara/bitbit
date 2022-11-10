MAJOR = 0
MINOR = 3
VERSION = $(MAJOR).$(MINOR)

CC = cc
CSTANDARD = -std=c99
CWARNINGS = -Wall -Wextra -Wshadow -pedantic
COPTIMIZE = -O2 -march=native -flto
CFLAGS = $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE)
LDFLAGS = $(CFLAGS)

DVERSION = -DVERSION=$(VERSION)

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
DEP_DIR = dep
MAN_DIR = man

SRC = bitboard.c magic_bitboard.c attack_gen.c \
      move.c util.c position.c move_gen.c perft.c \
      evaluate.c interface.c transposition_table.c \
      init.c interrupt.c bitbit.c uci.c
SRC_BITBIT = bitboard.c magic_bitboard.c attack_gen.c \
	     move.c util.c position.c move_gen.c perft.c \
	     evaluate.c interface.c transposition_table.c \
	     init.c interrupt.c bitbit.c
SRC_UCI = bitboard.c magic_bitboard.c attack_gen.c \
	  move.c util.c position.c move_gen.c perft.c \
	  evaluate.c interface.c transposition_table.c \
	  init.c interrupt.c uci.c

OBJ = $(addprefix $(OBJ_DIR)/,$(SRC:.c=.o))
OBJ_BITBIT = $(addprefix $(OBJ_DIR)/,$(SRC_BITBIT:.c=.o))
OBJ_UCI = $(addprefix $(OBJ_DIR)/,$(SRC_UCI:.c=.o))

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

ifneq ($(TT), )
	SIZE = -DTT=$(TT)
endif

all: bitbit

bitbit: $(OBJ_BITBIT)
	$(CC) $(LDFLAGS) $^ -o $@

uci: $(OBJ_UCI)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/interface.o: $(SRC_DIR)/interface.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $(DVERSION) -c $< -o $@

$(OBJ_DIR)/transposition_table.o: $(SRC_DIR)/transposition_table.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $(SIZE) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

include $(addprefix $(DEP_DIR)/,$(SRC:.c=.d))

$(DEP_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(DEP_DIR)
	@$(CC) -MM -MT "$@ $(<:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)" $(CFLAGS) -I$(INC_DIR) $< -o $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f bitbit $(DESTDIR)$(BINDIR)/bitbit
	chmod 755 $(DESTDIR)$(BINDIR)/bitbit
	mkdir -p $(DESTDIR)$(MAN6DIR)
	sed "s/VERSION/$(VERSION)/g" < $(MAN_DIR)/bitbit.6 > $(DESTDIR)$(MAN6DIR)/bitbit.6
	chmod 644 $(DESTDIR)$(MAN6DIR)/bitbit.6

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/bitbit $(DESTDIR)$(MAN6DIR)/bitbit.6

clean:
	rm -rf $(OBJ_DIR) $(DEP_DIR)

options:
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"

.PHONY: all clean install uninstall options
