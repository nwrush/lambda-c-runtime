#!/bin/bash
set -euo pipefail

mkdir -p build build/src build/include build/obj build/bin

rm -f build/src/*
rm -f build/include/*
rm -f build/obj/*
rm -f build/bin/*

INVOKE_LOC="$(pwd)"
BUILD_ROOT="$(pwd)/build"

cp -r ./include/* "$BUILD_ROOT/include"
cp -r ./src/* "$BUILD_ROOT/src"

cd "$BUILD_ROOT/obj"

gcc -std=c99 -g -Wall -c -I../include/ ../src/*.c

gcc -std=c99 -g -Wall -lm -ldl -o ../bin/c-runtime *.o

cp ../bin/c-runtime $INVOKE_LOC
