#!/usr/bin/env bash

set -x
set -e

brew install pkg-config
brew install freetype
brew install zlib

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DTSMUXER_GUI=TRUE ..
make V=1 VERBOSE=1
