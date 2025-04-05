#!/bin/sh
CFLAGS="-Wall -Wextra"
MAIN="src/main.c"
INCLUDE="/usr/share/include"
OUT="bin/inkpad"
set -xe
mkdir -p bin
gcc $MAIN -o $OUT -I$INCLUDE $CFLAGS -lraylib -lm
