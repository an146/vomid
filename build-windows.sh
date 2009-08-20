#!/bin/sh
make clean
cd libvomid
./configure -h i686-mingw32
cd ..
qmake -spec win32-g++
make
