#!/bin/sh

for f in usage home toml configuration configuration-path binding ; do
    "./tests/$f.sh" && echo "$f tests succeeded" || exit
done
