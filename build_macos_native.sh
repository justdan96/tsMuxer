#!/usr/bin/env bash

set -x
set -e

export MACOSX_DEPLOYMENT_TARGET=10.15

brew install pkg-config
brew install freetype
brew install zlib

builddir=$PWD

mkdir build

pushd build
cmake -DCMAKE_BUILD_TYPE=Release -DTSMUXER_GUI=TRUE ..

if ! num_cores=$(sysctl -n hw.logicalcpu); then
  num_cores=1
fi

make -j${num_cores}

pushd tsMuxerGUI
pushd tsMuxerGUI.app/Contents
defaults write "$PWD/Info.plist" NSPrincipalClass -string NSApplication
defaults write "$PWD/Info.plist" NSHighResolutionCapable -string True
plutil -convert xml1 Info.plist
popd
macdeployqt tsMuxerGUI.app
popd

mkdir bin
pushd bin
mv ../tsMuxer/tsmuxer tsMuxeR
mv ../tsMuxerGUI/tsMuxerGUI.app .
cp tsMuxeR tsMuxerGUI.app/Contents/MacOS/
zip -9 -r mac.zip tsMuxeR tsMuxerGUI.app
popd
popd
