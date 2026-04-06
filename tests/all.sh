#!/bin/sh

for f in usage home toml configuration ; do
    "./tests/$f.sh" && echo "$f tests succeeded" || exit
done
