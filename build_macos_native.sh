#!/usr/bin/env bash

set -x
set -e

export MACOSX_DEPLOYMENT_TARGET=10.10

brew install pkg-config
brew install freetype
brew install zlib

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DTSMUXER_GUI=TRUE ..
make
cd tsMuxerGUI
macdeployqt -verbose=3 tsMuxerGUI.app
