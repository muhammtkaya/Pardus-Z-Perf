# SPDX-License-Identifier: GPL-3.0-or-later
# Pardus Z-Performans - ust duzey derleme
#
# Kullanim:
#   make            -> zperfd'yi derler (build/zperfd)
#   sudo make run   -> servisi on planda calistirir (gelistirme/test)
#   make clean      -> derleme ciktilarini siler

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic -D_GNU_SOURCE
LDFLAGS ?=

SRCDIR  := src/zperfd
BUILD   := build
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))
BIN     := $(BUILD)/zperfd

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Derlendi: $(BIN)"

$(BUILD)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(BIN) --debug

clean:
	rm -rf $(BUILD)
