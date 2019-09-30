#!/bin/bash
mkdir -p cmake-build-release
cd cmake-build-release
echo Compiling server
cmake ..
make