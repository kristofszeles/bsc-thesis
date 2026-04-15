#!/bin/sh

clang++-12 *.cpp -o maze-game -O3 -Wall -pedantic -std=c++20 -no-pie -Iinclude -Llib -lSDL2 -lSDL2_image -lSDL2_net -lGL -lGLU -lGLEW -lnfd -lz -pthread `pkg-config --cflags --libs gtk+-3.0`
