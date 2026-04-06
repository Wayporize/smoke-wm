#!/bin/sh

run=build/tests/toml/run

make -f tests/GNUmakefile "$run" || exit

for f in tests/toml/valid/*.toml ; do
    if ! "./$run" "$f" ; then
        echo "test failed on '$f'"
        exit 1
    fi
done

for f in tests/toml/invalid/*.toml ; do
    if "./$run" "$f" >/dev/null ; then
        echo "test failed on '$f'"
        exit 1
    fi
done
