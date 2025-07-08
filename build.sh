#!/bin/bash
gcc odv9.c                                 \
    -o odv9                                \
    -std=c11                               \
    -ggdb                                  \
    -Wall                                  \
    -Wextra                                \
    -Werror                                \
    -Wpedantic                             \
    -Wfatal-errors                         \
    -Wno-format-truncation                 \
    `pkg-config --cflags --libs sdl2`  -lm \
