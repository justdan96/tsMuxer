#!/usr/bin/env bash

set -x
set -e

export MACOSX_DEPLOYMENT_TARGET=10.15

if ! num_cores=$(sysctl -n hw.logicalcpu); then
  num_cores=1
fi

topdir=$PWD
ft_installdir="$topdir/ft_install"
mkdir -p "$ft_installdir"

build_freetype()
{
  local ft_ver='2.11.0'
  local pkg_name="freetype-${ft_ver}"
  local pkg_fname="${pkg_name}.tar.xz"

  mkdir freetype_install

  curl -O -L "https://download.savannah.gnu.org/releases/freetype/${pkg_fname}"
  tar xf "${pkg_fname}"
  pushd "${pkg_name}"

  mkdir build
  pushd build

  # the default flags cause the build to depend on bzip2 and zlib only. this is
  # in line with the package that's build by homebrew.
  cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=FALSE
    "-DCMAKE_INSTALL_PREFIX=${ft_installdir}" ..
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
    "-DCMAKE_PREFIX_PATH=${ft_installdir}" ..
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

build_freetype
build_tsmuxer
