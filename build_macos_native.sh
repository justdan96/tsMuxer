#!/usr/bin/env bash

set -x
set -e

export MACOSX_DEPLOYMENT_TARGET=10.10

brew install pkg-config
brew install freetype
brew install zlib

builddir=$PWD

mkdir -p bin/mac
mkdir -p build

pushd build
cmake -DCMAKE_BUILD_TYPE=Release -DTSMUXER_GUI=TRUE ..
make

pushd tsMuxerGUI
macdeployqt tsMuxerGUI.app -verbose=3
popd

mv tsMuxer/tsmuxer "${builddir}/bin/mac/tsMuxeR"
mv tsMuxerGUI/tsMuxerGUI.app "${builddir}/bin/mac"
popd
