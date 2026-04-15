#!/bin/sh

g++-10 *.cpp -o maze-server -O3 -Wall -pedantic -std=c++20 -no-pie -Iinclude -Llib -lSDL2 -lSDL2_net -lpthread -lz
