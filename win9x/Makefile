# Cross-compiled Makefile for Windows 95/98/ME (i686). See the
# AmuletsArmor repo's Build/Win9x/Makefile for the full toolchain
# rationale (Ubuntu's gcc-mingw-w64-i686, not Homebrew's UCRT-locked
# one). Run on Linux/macOS with a mingw-w64 cross toolchain; there is no
# way to build (or run) this target on the box it's actually for.
#
# Lives directly in win9x/ (source + Makefile together), matching this
# repo's own established per-platform layout (irix/Makefile, mac/
# CMakeLists.txt) rather than the sibling AmuletsArmor/AAServer repos'
# separate Build/<Platform>/ split -- also sidesteps a real trap: this
# repo already has a gitignored local CMake output directory literally
# named build/ (lowercase), and macOS's default case-insensitive
# filesystem silently merges a same-named Build/ into it, making
# anything placed there invisible to git without any error or warning.
#
# Unlike AA.exe/AAServer.exe, this has no SDL/SDL_net dependency at all
# -- see main.c's own header comment for why (plain Win32 GUI,
# dependency-free by design, since this exists specifically to replace
# the .NET-based AALauncher.exe that can't run on 9x at all).

THIS_DIR  := $(shell pwd)
WIN9X_DIR := $(THIS_DIR)

BUILD_DIR := build

CC := i686-w64-mingw32-gcc

# See AmuletsArmor's Build/Win9x/Makefile for why 0x0410/0x0400/0x0410,
# __USE_MINGW_ANSI_STDIO=0, and -fno-stack-protector -- this launcher
# uses no stdio at all, but real Windows 95 hardware still crashed on it.
#
# -march=i486/-mtune=i486 is THE actual fix, confirmed by a second round
# of real-hardware testing: this launcher (zero shared code with AA.exe/
# AAServer.exe) crashed with "invalid instruction" at the *exact same
# bytes* as those two -- proof it's shared mingw-generated startup code,
# not application logic. 0f 44 is CMOVE, a Pentium Pro/P6-family
# instruction (1995+) absent on 486/Pentium/Pentium MMX;
# i686-w64-mingw32-gcc defaults to -march=pentiumpro with no explicit
# -march at all.
DEFINES := -DWINVER=0x0410 -D_WIN32_WINNT=0x0400 -D_WIN32_WINDOWS=0x0410 \
           -D__USE_MINGW_ANSI_STDIO=0 -fno-stack-protector \
           -march=i486 -mtune=i486

TARGET := $(BUILD_DIR)/AALauncher.exe

.PHONY: all clean package

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(WIN9X_DIR)/main.c | $(BUILD_DIR)
	$(CC) -O2 $(DEFINES) -o $@ $< -mwindows

clean:
	rm -rf $(BUILD_DIR)

package: all
	mkdir -p $(BUILD_DIR)/package
	cp $(TARGET) $(BUILD_DIR)/package/
