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

SRC_BITBIT = bitboard.c magic_bitboard.c attack_gen.c \
             move.c util.c position.c move_gen.c perft.c \
             search.c evaluate.c interface.c \
             transposition_table.c init.c time_man.c \
             interrupt.c pawn.c history.c bitbit.c

SRC_AVA    = bitboard.c magic_bitboard.c attack_gen.c \
             move.c util.c position.c move_gen.c \
             transposition_table.c init.c history.c \
             ava.c

OBJ_BITBIT = $(addprefix $(LOCAL_OBJDIR)/,$(SRC_BITBIT:.c=.o))
OBJ_AVA = $(addprefix $(LOCAL_OBJDIR)/,$(SRC_AVA:.c=.o))

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANPREFIX = $(PREFIX)/share
MANDIR = $(MANPREFIX)/man
MAN6DIR = $(MANDIR)/man6

ifneq ($(TT), )
	SIZE = -DTT=$(TT)
endif

all: bitbit ava

bitbit: $(OBJ_BITBIT)
	$(CC) $(LDFLAGS) $^ -o $@

ava: $(OBJ_AVA)
	$(CC) $(LDFLAGS) -lpthread $^ -o $@

$(LOCAL_OBJDIR)/interface.o: $(LOCAL_SRCDIR)/interface.c
	@mkdir -p $(LOCAL_OBJDIR)
	$(CC) $(CFLAGS) -I$(LOCAL_INCDIR) $(DVERSION) -c $< -o $@

$(LOCAL_OBJDIR)/transposition_table.o: $(LOCAL_SRCDIR)/transposition_table.c
	@mkdir -p $(LOCAL_OBJDIR)
	$(CC) $(CFLAGS) -I$(LOCAL_INCDIR) $(SIZE) -c $< -o $@

$(LOCAL_OBJDIR)/ava.o: $(LOCAL_SRCDIR)/ava.c
	@mkdir -p $(LOCAL_OBJDIR)
	$(CC) $(CFLAGS) -I$(LOCAL_INCDIR) -D_POSIX_C_SOURCE -c $< -o $@

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
	cp -f ava $(DESTDIR)$(BINDIR)/ava
	chmod 755 $(DESTDIR)$(BINDIR)/ava
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
