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
macdeployqt tsMuxerGUI.app
pushd tsMuxerGUI.app/Contents
defaults write "$PWD/Info.plist" NSPrincipalClass -string NSApplication
defaults write "$PWD/Info.plist" NSHighResolutionCapable -string True
plutil -convert xml1 Info.plist
popd
popd

mv tsMuxer/tsmuxer "${builddir}/bin/mac/tsMuxeR"
mv tsMuxerGUI/tsMuxerGUI.app "${builddir}/bin/mac"
popd

