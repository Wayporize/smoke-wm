CC := cc

PACKAGES := xkbcommon
C_FLAGS := -g -fsanitize=address -std=c99 -D_XOPEN_SOURCE=700 \
		   $(shell pkg-config --cflags $(PACKAGES))
C_LIBS := $(shell pkg-config --libs $(PACKAGES))
SOURCES := tests/toml/run.c src/utility/*.c src/configuration.c src/toml_*.c
RUN := build/tests/toml/run

.PHONY: default
default: $(RUN)

$(RUN): $(SOURCES)
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -Iinclude $(SOURCES) -o $@ $(C_LIBS)
