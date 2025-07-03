#!/bin/bash
gcc odv9.c         \
    -o odv9        \
    -std=c11       \
    -ggdb          \
    -Wall          \
    -Wextra        \
    -Wpedantic     \
    -Wfatal-errors \
    `pkg-config --cflags --libs sdl2`  -lm \
