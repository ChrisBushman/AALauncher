# Native Makefile for PowerPC / Mac OS X 10.4 "Tiger".
# Run this ON the Tiger box, or via remote-build.sh from a modern dev
# machine over SSH.
#
# Cocoa + WebKit.framework only -- both ship with the OS since 10.3/
# Safari 1.0, no Tigerbrew/third-party dependency needed (unlike the
# mac/ Qt6+WebEngine version, neither of which has ever existed for
# PowerPC).

REPO_ROOT := $(abspath ..)
SRC_DIR   := $(REPO_ROOT)/ppc
BUILD_DIR := build

CC ?= gcc
ARCH ?= ppc
MACOSX_VERSION_MIN ?= 10.4
SDKROOT ?=

ARCHFLAGS := -arch $(ARCH) -mmacosx-version-min=$(MACOSX_VERSION_MIN)
ifneq ($(strip $(SDKROOT)),)
ARCHFLAGS += -isysroot $(SDKROOT)
endif

OPT_FLAGS := -g -O2
# -fobjc-exceptions: this old GCC doesn't enable @try/@catch/@throw syntax
# by default (unlike modern Clang) -- needed for AppDelegate.m's
# launchProcess:arguments:.
OBJC_FLAGS := -fobjc-exceptions
FRAMEWORKS := -framework Cocoa -framework WebKit

TARGET := $(BUILD_DIR)/AALauncher
SOURCES := main.m AppDelegate.m NetworkIPController.m ScriptCompilerController.m
OBJS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.m=.m.o))

.PHONY: all clean run

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(ARCHFLAGS) -o $@ $(OBJS) $(FRAMEWORKS)

$(BUILD_DIR)/%.m.o: $(SRC_DIR)/%.m
	$(CC) $(ARCHFLAGS) $(OPT_FLAGS) $(OBJC_FLAGS) -x objective-c -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

run: all
	$(REPO_ROOT)/$(TARGET)
