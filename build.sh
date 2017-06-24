#!/bin/bash
debug='-O3'
if [ "$1" == "debug" ]; then
    debug='-g'
fi

clang++ *.cpp imgui/*.cpp -std=c++11 $debug -lGL -lglfw -o ImDbg