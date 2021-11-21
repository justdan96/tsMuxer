#!/usr/bin/env bash

set -x
set -e

export MACOSX_DEPLOYMENT_TARGET=10.15

if ! num_cores=$(sysctl -n hw.logicalcpu); then
  num_cores=1
fi

topdir=$PWD
deps_installdir="$topdir/.deps_install"
mkdir -p "${deps_installdir}"

download()
{
  curl -O -L "$1"
}

build_libpng()
{
  local png_ver='1.6.37'
  local pkg_name="libpng-${png_ver}"
  local pkg_fname="${pkg_name}.tar.xz"

  download "https://download.sourceforge.net/libpng/${pkg_fname}"
  tar xf "$pkg_fname"
  pushd "$pkg_name"

  ./configure --prefix="${deps_installdir}" --enable-shared=no
  make -j${num_cores}
  make install

  popd
}

build_freetype()
{
  local ft_ver='2.11.0'
  local pkg_name="freetype-${ft_ver}"
  local pkg_fname="${pkg_name}.tar.xz"

  mkdir freetype_install

  download "https://download.savannah.gnu.org/releases/freetype/${pkg_fname}"
  tar xf "${pkg_fname}"
  pushd "${pkg_name}"

  mkdir build
  pushd build

  # this aims to replicate the brew package's functionality, which uses
  # system-provided bzip2 and zlib, and requires libpng which we provide
  # ourselves as a static library.
  cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=FALSE \
    -DFT_WITH_PNG=TRUE -DFT_WITH_ZLIB=TRUE -DFT_WITH_BZIP2=TRUE \
    -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE \
    -DCMAKE_DISABLE_FIND_PACKAGE_BrotliDec=TRUE \
    -DCMAKE_INSTALL_PREFIX="${deps_installdir}" \
    -DCMAKE_PREFIX_PATH="${deps_installdir}" ..
  make -j${num_cores}
  make install

  popd
  popd
}

build_tsmuxer()
{
  mkdir build

  pushd build
  cmake -DCMAKE_BUILD_TYPE=Release -DTSMUXER_GUI=TRUE \
    "-DCMAKE_PREFIX_PATH=${deps_installdir}" \
    "-DFREETYPE_EXTRA_LIBRARIES=bz2;${deps_installdir}/lib/libpng.a" ..
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
}

mkdir .deps
pushd .deps
build_libpng
build_freetype
popd

build_tsmuxer
