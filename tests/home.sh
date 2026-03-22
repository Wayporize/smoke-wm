#!/bin/sh

make

check_home() {
    case $1 in
    user\ home:\ *)
        if [ "${1#user home: }" != "$2" ] ; then
            echo home is not $2
            exit 1
        fi
        break
    esac
}

for h in a b c LOL /home/what $HOME ; do
    while read -r line ; do
        check_home $line $h
    done < <(HOME=$h ./build/smoke-wm)

done

while read -r line ; do
    check_home $line $HOME
done < <(HOME= ./build/smoke-wm)

echo home is correctly interpreted
