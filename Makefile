MAJOR = 0
MINOR = 2

CC = cc
CSTANDARD = -std=c99
CWARNINGS = -Wall -Wextra -Wshadow -pedantic
COPTIMIZE = -O2
CFLAGS += $(CSTANDARD) $(CWARNINGS) $(COPTIMIZE)

VERSION = -DVERSION=$(MAJOR).$(MINOR)

SOURCE_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
SRC = main.c bitboard.c magic_bitboard.c attack_gen.c move.c util.c position.c move_gen.c perft.c evaluate.c interface.c transposition_table.c init.c interrupt.c

OBJ = $(addprefix $(BUILD_DIR)/,$(SRC:.c=.o))

PREFIX = /usr/local
BINDIR = /bin

ifneq ($(TT), )
	SIZE = -DTT=$(TT)
endif

all: build bitbit

bitbit: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/main.o: $(SOURCE_DIR)/main.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@

$(BUILD_DIR)/bitboard.o: $(SOURCE_DIR)/bitboard.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@

$(BUILD_DIR)/magic_bitboard.o: $(SOURCE_DIR)/magic_bitboard.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@
	
$(BUILD_DIR)/attack_gen.o: $(SOURCE_DIR)/attack_gen.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@
	
$(BUILD_DIR)/move.o: $(SOURCE_DIR)/move.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@
	
$(BUILD_DIR)/util.o: $(SOURCE_DIR)/util.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@
	
$(BUILD_DIR)/position.o: $(SOURCE_DIR)/position.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@
	
$(BUILD_DIR)/move_gen.o: $(SOURCE_DIR)/move_gen.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@
	
$(BUILD_DIR)/perft.o: $(SOURCE_DIR)/perft.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@

$(BUILD_DIR)/evaluate.o: $(SOURCE_DIR)/evaluate.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@

$(BUILD_DIR)/interface.o: $(SOURCE_DIR)/interface.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -DVERSION=$(MAJOR).$(MINOR) $(SIZE) -c $^ -o $@

$(BUILD_DIR)/transposition_table.o: $(SOURCE_DIR)/transposition_table.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $(SIZE) -c $^ -o $@

$(BUILD_DIR)/init.o: $(SOURCE_DIR)/init.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@

$(BUILD_DIR)/interrupt.o: $(SOURCE_DIR)/interrupt.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $^ -o $@

install: all
	mkdir -p $(DESTDIR)$(PREFIX)$(BINDIR)
	cp -f bitbit $(DESTDIR)$(PREFIX)$(BINDIR)
	chmod 755 $(DESTDIR)$(PREFIX)$(BINDIR)/bitbit

install_script:
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

.PHONY: all clean install install_script uninstall options
