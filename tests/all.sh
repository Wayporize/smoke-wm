#!/bin/sh

for f in usage home toml configuration configuration-path bindings ; do
    "./tests/$f.sh" && echo "$f tests succeeded" || exit
done
