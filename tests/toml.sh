#!/bin/sh

make -f tests/toml/GNUmakefile || exit

run=build/tests/toml/run

for f in tests/toml/valid/* ; do
    if ! "./$run" "$f" ; then
        echo "test failed on '$f'"
        exit 1
    fi
done

for f in tests/toml/invalid/* ; do
    if "./$run" "$f" >/dev/null ; then
        echo "test failed on '$f'"
        exit 1
    fi
done

echo "toml tests succeeded"
