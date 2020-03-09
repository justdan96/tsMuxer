#!/usr/bin/bash

cd ~
if [ "$MSYSTEM" == "MSYS" ] ; then
 pacman -Syu
 pacman -Sy --needed base-devel \
 flex \
 zlib-devel \
 git
 if [ ! -d tsmuxer ] ; then
  git clone https://github.com/justdan96/tsMuxer.git
 fi
else
 cd tsmuxer
 if [ ! -d build ] ; then
  pacman -Sy --needed $MINGW_PACKAGE_PREFIX-toolchain \
  $MINGW_PACKAGE_PREFIX-cmake \
  $MINGW_PACKAGE_PREFIX-freetype \
  $MINGW_PACKAGE_PREFIX-zlib \
  $MINGW_PACKAGE_PREFIX-ninja
  if [ ! -d $MINGW_PREFIX/qt5-static ] ; then
   echo If you intend to build the tsMuxerGUI enter Y
   pacman -S --needed $MINGW_PACKAGE_PREFIX-qt5-static
  fi
  if [ -d $MINGW_PREFIX/qt5-static ] ; then
   echo 'load(win32/windows_vulkan_sdk)' > $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
   echo 'QMAKE_LIBS_VULKAN       =' >>     $MINGW_PREFIX/qt5-static/share/qt5/mkspecs/common/windows-vulkan.conf
  fi
  mkdir build
 fi
 git pull
 cd build
 cmake ../ -G Ninja -DTSMUXER_STATIC_BUILD=true
 ninja && cp -u tsMuxer/tsmuxer.exe ../bin/
 if [ -d $MINGW_PREFIX/qt5-static ] ; then
  $MINGW_PREFIX/qt5-static/bin/qmake ../tsMuxerGUI
  make
  [ -f tsMuxerGUI.exe ] && cp -u tsMuxerGUI.exe ../bin/
  [ -f debug/tsMuxerGUI.exe ] && cp -u debug/tsMuxerGUI.exe ../bin/
 fi
 cd ..
fi
