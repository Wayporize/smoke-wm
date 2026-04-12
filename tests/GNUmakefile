CC := cc

PACKAGES := xcb xcb-xkb xkbcommon xkbcommon-x11
C_FLAGS := -g -fsanitize=address -std=c99 -D_XOPEN_SOURCE=700 \
		   $(shell pkg-config --cflags $(PACKAGES))
C_LIBS := $(shell pkg-config --libs $(PACKAGES))
TOML_SOURCES := src/x11.c src/utility/*.c src/binding.c src/configuration.c src/toml_*.c

build/tests/toml/run: tests/toml/run.c $(TOML_SOURCES)
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -Iinclude $^ -o $@ $(C_LIBS)

build/tests/configuration/run: tests/configuration/run.c $(TOML_SOURCES)
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -Iinclude $^ -o $@ $(C_LIBS)
