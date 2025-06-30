#!/bin/bash
gcc odv9.c -o odv9 -std=c11 -ggdb -Wfatal-errors `pkg-config --cflags --libs sdl2`  -lm 
