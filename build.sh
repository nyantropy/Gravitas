#!/usr/bin/env bash

# Rebuilds all the CMake stuff safely

set -e  # Exit immediately if any command fails

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
else
    echo "Build directory already exists."
fi

# Enter the build directory
cd build

# Run CMake and Make
cmake ..
make

echo "Build done!"

