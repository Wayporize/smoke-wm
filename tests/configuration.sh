#!/bin/bash

run=build/tests/configuration/run

make -f tests/GNUmakefile "$run" "$compare" || exit

for f in tests/toml/valid/*.toml ; do
    output="$(dirname "$f")/$(basename "$f" .toml).output"
    if ! cmp -s "$output" ; then
        echo "compare failed on $f"
        exit 1
    fi < <("./$run" "$f")
done
