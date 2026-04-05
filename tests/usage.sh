#!/bin/sh

make || exit

invalid_arguments=invalid\ -e\ --h
valid_arguments=-h\ --help\ --usage\ -v\ --version

for a in $invalid_arguments ; do
    if ./build/smoke-wm $a >/dev/null ; then
        echo "./build/smoke-wm $a: succeeded but invalid arguments should fail"
        exit 1
    fi
done

for a in $valid_arguments ; do
    if ! ./build/smoke-wm $a >/dev/null ; then
        echo "./build/smoke-wm $a: failed but valid arguments should succeed"
        exit 1
    fi
done

echo "Usage tests succeded"
