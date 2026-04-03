CC := cc

PACKAGES := xkbcommon
C_FLAGS := -g -std=c99 -D_XOPEN_SOURCE=700 \
		   $(shell pkg-config --cflags $(PACKAGES))
C_LIBS := $(shell pkg-config --libs $(PACKAGES))
SOURCES := tests/toml/run.c src/utility/*.c src/configuration.c src/toml_*.c

.PHONY: default
default: tests/toml/run

tests/toml/run: $(SOURCES)
	$(CC) $(C_FLAGS) -Iinclude $(SOURCES) -o $@ $(C_LIBS)
