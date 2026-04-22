#!/bin/sh

invalid_arguments=invalid\ -e\ --h
valid_arguments=-h\ --help\ --usage\ -v\ --version

# The program must return an error
for a in $invalid_arguments ; do
    if "$SMOKE_WM" $a >/dev/null ; then
        echo "./build/smoke-wm $a: succeeded but invalid arguments should fail"
        exit 1
    fi
done

# The program must succeed
for a in $valid_arguments ; do
    if ! "$SMOKE_WM" $a >/dev/null ; then
        echo "./build/smoke-wm $a: failed but valid arguments should succeed"
        exit 1
    fi
done
