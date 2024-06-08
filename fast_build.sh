#!/usr/bin/env bash

#rebuilds all the cmake stuff

#rm -r ./build
#mkdir build
cd build
cmake ..
make
echo build done!
