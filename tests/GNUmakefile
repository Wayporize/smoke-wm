# C compiler
CC := cc

# Packages
PACKAGES := x11

# Compiler flags
C_FLAGS := -Iinclude -std=c99 -g -Wall -Wextra -Wpedantic \
		   $(shell pkg-config --cflags $(PACKAGES))

# Libraries
C_LIBRARIES := $(shell pkg-config --libs $(PACKAGES))

# Get all source files and their binaries
SOURCES := $(shell find tests/windows -name '*.c') tests/tool.c
BINARIES := $(patsubst %.c,build/%,$(SOURCES))

.PHONY: default
default: build

build: $(BINARIES)

build/tests/%: tests/%.c
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) $< -o $@ $(C_LIBRARIES)
