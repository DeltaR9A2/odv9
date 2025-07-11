#!/bin/bash

# Set up basic paths
OUTFILE="odv9.exe"
SRC="odv9.c"
CC="x86_64-w64-mingw32-gcc"
PC="x86_64-w64-mingw32-pkg-config"

FLAGS="-O2 `$PC --cflags --libs sdl2` -mwindows" 

# Compile
echo "Compiling $SRC for Windows..."
$CC -o $OUTFILE $SRC $FLAGS 

if [ $? -eq 0 ]; then
    echo "Build successful: $OUTFILE"
else
    echo "Build failed."
fi
