MAJOR = 0
MINOR = 0

CC = cc
CSTANDARD = -std=c99
CWARNINGS = -Wall -Wextra -Wshadow -pedantic
COPTIMIZE = -O2
CFLAGS += $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE)

VERSION = -DVERSION=$(MAJOR).$(MINOR)

SOURCE_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
SRC = main.c bitboard.c magic_bitboard.c attack_gen.c move.c util.c position.c move_gen.c perft.c evaluate.c interface.c hash_table.c init.c

ifneq ($(HASH), )
	CFLAGS += -DHASH=$(HASH)
endif

ifeq ($(UNICODE), 1)
	CFLAGS += -DUNICODE
endif

OBJ = $(addprefix $(BUILD_DIR)/,$(SRC:.c=.o))

PREFIX = /usr/local
BINDIR = /bin

all: build bitbit

bitbit: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	$(CC) $(CFLAGS) $(VERSION) -I$(INCLUDE_DIR) -c $^ -o $@

install: all
	mkdir -p $(DESTDIR)$(PREFIX)$(BINDIR)
	cp -f bitbit $(DESTDIR)$(PREFIX)$(BINDIR)
	chmod 755 $(DESTDIR)$(PREFIX)$(BINDIR)/bitbit

install_startup_script:
	mkdir -p $(DESTDIR)$(PREFIX)$(BINDIR)
	cp -f scripts/start/bitbitpos.sh $(DESTDIR)$(PREFIX)$(BINDIR)/bitbitpos
	chmod 755 $(DESTDIR)$(PREFIX)$(BINDIR)/bitbitpos

uninstall:
	rm -f $(DESTDIR)$(PREFIX)$(BINDIR)/bitbit
	rm -f $(DESTDIR)$(PREFIX)$(BINDIR)/bitbitpos

build:
	mkdir $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

options:
	@echo "CC     = $(CC)    "
	@echo "CFLAGS = $(CFLAGS)"

.PHONY: all clean install uninstall options
