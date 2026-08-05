# Native Makefile for PowerPC G3 / Mac OS X 10.3 "Panther".
#
# Separate G3 platform (distinct from ppc/, which targets a G4 Tiger box). It
# reuses the same Objective-C sources in ../ppc but pins a Panther-native
# toolchain -- Apple's own gcc 4.0 against the 10.3.9 SDK, generic-G3 codegen
# (-mcpu=750 -mno-altivec) -- so the binary links Panther's system libSystem
# (current version 71.x), not Tigerbrew gcc-14's 10.4 runtime, and contains no
# AltiVec. Cocoa + WebKit both ship with Panther (Safari 1.x); building against
# the 10.3.9 SDK also verifies the WebKit APIs used are actually Panther-era
# (a Tiger-only symbol would fail to link here rather than at runtime).

REPO_ROOT := $(abspath ..)
SRC_DIR   := $(REPO_ROOT)/ppc
BUILD_DIR := build

# := not ?=: CC is a GNU-make built-in (cc), so ?= would never override it and
# the build would silently use Tigerbrew gcc-14 (=cc on PATH) instead of Apple
# gcc 4.0. := beats the built-in but still loses to a command-line CC= override.
CC := /usr/bin/gcc-4.0
ARCH ?= ppc
MACOSX_VERSION_MIN ?= 10.3
SDKROOT ?= /Developer/SDKs/MacOSX10.3.9.sdk
G3FLAGS ?= -mcpu=750 -mno-altivec

ARCHFLAGS := -arch $(ARCH) -mmacosx-version-min=$(MACOSX_VERSION_MIN) $(G3FLAGS)
ifneq ($(strip $(SDKROOT)),)
ARCHFLAGS += -isysroot $(SDKROOT)
endif

OPT_FLAGS := -g -O2
# -fobjc-exceptions: this old GCC doesn't enable @try/@catch/@throw syntax by
# default -- needed for AppDelegate.m's launchProcess:arguments:.
OBJC_FLAGS := -fobjc-exceptions
FRAMEWORKS := -framework Cocoa -framework WebKit

TARGET := $(BUILD_DIR)/AALauncher
SOURCES := main.m AppDelegate.m DisplaySettingsController.m NetworkIPController.m ScriptCompilerController.m
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
