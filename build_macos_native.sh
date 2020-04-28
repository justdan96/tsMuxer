#!/usr/bin/env bash

set -x
set -e

export MACOSX_DEPLOYMENT_TARGET=10.10

brew install pkg-config
brew install freetype
brew install zlib

builddir=$PWD

mkdir build

pushd build
cmake -DCMAKE_BUILD_TYPE=Release -DTSMUXER_GUI=TRUE ..
make -j$(sysctl -n hw.logicalcpu)

pushd tsMuxerGUI.app/Contents
defaults write "$PWD/Info.plist" NSPrincipalClass -string NSApplication
defaults write "$PWD/Info.plist" NSHighResolutionCapable -string True
plutil -convert xml1 Info.plist
popd

pushd tsMuxerGUI
macdeployqt tsMuxerGUI.app
popd

mv tsMuxer/tsmuxer "$PWD/tsMuxeR"
mv tsMuxerGUI/tsMuxerGUI.app "$PWD"
BZIP2=-9 tar cjf mac.tar.bz2 tsMuxeR tsMuxerGUI.app
popd
