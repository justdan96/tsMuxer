#!/usr/bin/env bash

brew install pkg-config
brew install freetype

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DTSMUXER_GUI=TRUE ..
make
