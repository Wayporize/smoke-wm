#!/bin/sh

make -f tests/toml/GNUmakefile || exit

gdb ./tests/toml/run
