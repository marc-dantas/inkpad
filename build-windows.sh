#!/bin/sh

CFLAGS="-Wall -Wextra"
MAIN="src/main.c"
INCLUDE="/usr/local/include"
OUT="bin/inkpad-x86_64-windows.exe"
set -xe
mkdir -p bin

x86_64-w64-mingw32-gcc-win32 $MAIN -o $OUT -I$INCLUDE $CFLAGS -L./lib -lraylib -lwinmm -lgdi32
