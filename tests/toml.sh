#!/bin/sh

make -f tests/toml/GNUmakefile || exit

for f in tests/toml/valid/* ; do
    if ! ./tests/toml/run "$f" ; then
        echo "test failed on '$f'"
        exit 1
    fi
done

for f in tests/toml/invalid/* ; do
    if ./tests/toml/run "$f" >/dev/null ; then
        echo "test failed on '$f'"
        exit 1
    fi
done

echo "toml tests succeeded"
