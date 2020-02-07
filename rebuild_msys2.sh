#!/usr/bin/bash

pacman -Syu
if [ "$MSYSTEM" == "MSYS" ] ; then
 pacman -S base-devel --needed
 pacman -S flex --needed
 pacman -S zlib-devel --needed
else
 if [ ! -d build ] ; then
  pacman -S $MINGW_PACKAGE_PREFIX-toolchain --needed
  pacman -S $MINGW_PACKAGE_PREFIX-cmake --needed
  pacman -S $MINGW_PACKAGE_PREFIX-freetype --needed
  pacman -S $MINGW_PACKAGE_PREFIX-zlib --needed
  pacman -S $MINGW_PACKAGE_PREFIX-ninja --needed
  pacman -S $MINGW_PACKAGE_PREFIX-qt5-static --needed
  echo 'load(win32/windows_vulkan_sdk)' > $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
  echo 'QMAKE_LIBS_VULKAN       =' >>     $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
  mkdir build
 fi
 cd build
 cmake .. -G Ninja
 ninja && cp -u tsMuxer/tsmuxer.exe ../bin/
 $MINGW_PREFIX/qt5-static/bin/qmake ../tsMuxerGUI
 make && cp -u debug/tsMuxerGUI.exe ../bin/
 cd ..
fi
