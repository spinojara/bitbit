MAJOR = 0
MINOR = 3
VERSION = $(MAJOR).$(MINOR)

CC = cc
CSTANDARD = -std=c99
CWARNINGS = -Wall -Wextra -Wshadow -pedantic
ARCH = native
COPTIMIZE = -O2 -march=$(ARCH) -flto
CFLAGS = $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE)
LDFLAGS = $(CFLAGS)

DVERSION = -DVERSION=$(VERSION)

LOCAL_SRCDIR = src
LOCAL_INCDIR = include
LOCAL_OBJDIR = obj
LOCAL_DEPDIR = dep
LOCAL_MANDIR = man

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

OBJ = $(addprefix $(LOCAL_OBJDIR)/,$(SRC:.c=.o))
OBJ_BITBIT = $(addprefix $(LOCAL_OBJDIR)/,$(SRC_BITBIT:.c=.o))
OBJ_UCI = $(addprefix $(LOCAL_OBJDIR)/,$(SRC_UCI:.c=.o))

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

$(LOCAL_OBJDIR)/interface.o: $(LOCAL_SRCDIR)/interface.c
	@mkdir -p $(LOCAL_OBJDIR)
	$(CC) $(CFLAGS) -I$(LOCAL_INCDIR) $(DVERSION) -c $< -o $@

$(LOCAL_OBJDIR)/transposition_table.o: $(LOCAL_SRCDIR)/transposition_table.c
	@mkdir -p $(LOCAL_OBJDIR)
	$(CC) $(CFLAGS) -I$(LOCAL_INCDIR) $(SIZE) -c $< -o $@

$(LOCAL_OBJDIR)/%.o: $(LOCAL_SRCDIR)/%.c
	@mkdir -p $(LOCAL_OBJDIR)
	$(CC) $(CFLAGS) -I$(LOCAL_INCDIR) -c $< -o $@

include $(addprefix $(LOCAL_DEPDIR)/,$(SRC:.c=.d))

$(LOCAL_DEPDIR)/%.d: $(LOCAL_SRCDIR)/%.c
	@mkdir -p $(LOCAL_DEPDIR)
	@$(CC) -MM -MT "$@ $(<:$(LOCAL_SRCDIR)/%.c=$(LOCAL_OBJDIR)/%.o)" $(CFLAGS) -I$(LOCAL_INCDIR) $< -o $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f bitbit $(DESTDIR)$(BINDIR)/bitbit
	chmod 755 $(DESTDIR)$(BINDIR)/bitbit
	mkdir -p $(DESTDIR)$(MAN6DIR)
	sed "s/VERSION/$(VERSION)/g" < $(LOCAL_MANDIR)/bitbit.6 > $(DESTDIR)$(MAN6DIR)/bitbit.6
	chmod 644 $(DESTDIR)$(MAN6DIR)/bitbit.6

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/bitbit $(DESTDIR)$(MAN6DIR)/bitbit.6

clean:
	rm -rf $(LOCAL_OBJDIR) $(LOCAL_DEPDIR)

options:
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"

.PHONY: all clean install uninstall options
