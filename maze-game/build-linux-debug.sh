#!/bin/sh

g++-10 *.cpp -o maze-game -g -Wall -pedantic -std=c++20 -no-pie -Iinclude -Llib -lSDL2 -lSDL2_image -lSDL2_net -lGL -lGLU -lGLEW -lnfd -lz -pthread `pkg-config --cflags --libs gtk+-3.0`
