# Native Makefile for IRIX 6.5 / SGI O2 (IP32, MIPS R12000).
# Run this ON the O2 (TGCware's GCC 4.5.2 is the toolchain), or via
# remote-build.sh from a modern dev machine over SSH.
#
# Xt + Xaw only -- both are part of any IRIX X11R6 install (base OS),
# no Nekoware/TGCware dependency needed.

REPO_ROOT := $(shell cd .. && pwd)
SRC_DIR   := $(REPO_ROOT)/irix
BUILD_DIR := build

CC := /usr/tgcware/gcc45/bin/gcc

OPT_FLAGS := -g -O2
STD_FLAGS := -std=gnu99
LIBS      := -lXaw -lXt -lX11 -lSM -lICE

TARGET := $(BUILD_DIR)/AALauncher

.PHONY: all clean run

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRC_DIR)/main.c
	$(CC) $(OPT_FLAGS) $(STD_FLAGS) -o $@ $< $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

run: all
	$(REPO_ROOT)/$(TARGET)
