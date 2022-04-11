#!/usr/bin/env bash

mkdir build

pushd build
cmake -DCMAKE_BUILD_TYPE=Release -DTSMUXER_STATIC_BUILD=TRUE \
  "-DFREETYPE_LDFLAGS=bz2;$(brew --prefix)/lib/libpng.a" -DTSMUXER_GUI=TRUE \
  -DWITHOUT_PKGCONFIG=TRUE "${EXTRA_CMAKE_ARGS[@]}" -DQT_VERSION=$qtver ..

if ! num_cores=$(sysctl -n hw.logicalcpu); then
  num_cores=1
fi

make -j${num_cores}

pushd tsMuxerGUI
pushd tsMuxerGUI.app/Contents
# avoid permission denied errors with Info.plist
chmod 664 "$PWD/Info.plist"
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
