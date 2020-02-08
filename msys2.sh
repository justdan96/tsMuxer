#!/usr/bin/bash

if [ "$MSYSTEM" == "MSYS" ] ; then
 pacman -Syu
 pacman -Sy --needed base-devel \
 flex \
 zlib-devel \
 git
else
 if [ ! -d build ] ; then
  pacman -Sy --needed $MINGW_PACKAGE_PREFIX-toolchain \
  $MINGW_PACKAGE_PREFIX-cmake \
  $MINGW_PACKAGE_PREFIX-freetype \
  $MINGW_PACKAGE_PREFIX-zlib \
  $MINGW_PACKAGE_PREFIX-ninja
  echo If you intend to build the tsMuxerGUI enter Y
  pacman -S --needed $MINGW_PACKAGE_PREFIX-qt5-static
  if [ -d $MINGW_PREFIX/qt5-static ] ; then
   echo 'load(win32/windows_vulkan_sdk)' > $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
   echo 'QMAKE_LIBS_VULKAN       =' >>     $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
  fi
  mkdir build
 fi
 git pull
 cd build
 cmake .. -G Ninja
 ninja && cp -u tsMuxer/tsmuxer.exe ../bin/
 if [ -d $MINGW_PREFIX/qt5-static ] ; then
  $MINGW_PREFIX/qt5-static/bin/qmake ../tsMuxerGUI
  make && cp -u debug/tsMuxerGUI.exe ../bin/
 fi
 cd ..
fi
