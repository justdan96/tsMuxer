#!/usr/bin/env bash

set -e
set -x
set -o pipefail

readonly OSXCROSS_SYSROOT=/usr/lib/osxcross/macports/pkgs

OSXCROSS_WRAPPER=$(ls -1 /usr/lib/osxcross/bin/x86_64-apple-darwin*-wrapper | xargs basename)
OSXCROSS_TRIPLE="${OSXCROSS_WRAPPER/-wrapper/}" 

gui_lib_copy() {
  local dep_lib=${1##@rpath/}
  mkdir -p "$(dirname libs/${dep_lib})"
  cp "${OSXCROSS_SYSROOT}/opt/local/lib/${dep_lib}" "libs/${dep_lib}"
}

osxcross_lib_copy_change_path() {
  local dep_lib=$1
  local dep_lib_actual_path="${OSXCROSS_SYSROOT}${dep_lib}"
  local bin_path=$2
  local lib_fname=${dep_lib##*/}
  local lib_path_declared=${dep_lib##${OSXCROSS_SYSROOT}/}
  local target_lib_fname="../bin/${lib_fname}"

  $OSXCROSS_TRIPLE-install_name_tool -change "$lib_path_declared" "@executable_path/${lib_fname}" "$bin_path"

  [[ -e $target_lib_fname ]] && return
  cp "$dep_lib_actual_path" "$target_lib_fname"
  for_each_dep_lib "$target_lib_fname" '/opt/local/lib/' osxcross_lib_copy_change_path
}

tsmuxer_lib_copy_change_path() {
  local dep_lib=$1
  local bin_path=$2
  local lib_fname=${dep_lib##*/}
  local dep_lib_actual_path="${OSXCROSS_SYSROOT}${dep_lib}"
  local target_lib_path="../bin/${lib_fname}"
  cp "$dep_lib_actual_path" "$target_lib_path"
  $OSXCROSS_TRIPLE-install_name_tool -change "$dep_lib" "@executable_path/${lib_fname}" "$bin_path"
  for_each_dep_lib "$target_lib_path" '/opt/local/lib/' osxcross_lib_copy_change_path
}

for_each_dep_lib() {
  local bin_path=$1
  local wanted_pfx=$2
  local cbk=$3
  local declared_path=${bin_path##${OSXCROSS_SYSROOT}}
  for dep_lib in $($OSXCROSS_TRIPLE-otool -L "$bin_path" | awk '/^\t/ { print $1 }' | sort -u); do
    if [[ $dep_lib != $bin_path && $dep_lib != $declared_path && $dep_lib =~ ^${wanted_pfx} ]]; then
      "$cbk" "$dep_lib" "$bin_path"
    fi
  done
}

export PATH=/usr/lib/osxcross/bin:$PATH
export MACOSX_DEPLOYMENT_TARGET=10.15
export PKG_CONFIG=/usr/lib/osxcross/bin/$OSXCROSS_TRIPLE-pkg-config
export OSXCROSS_MP_INC=1
export LD_LIBRARY_PATH=/usr/lib/osxcross/lib:$LD_LIBRARY_PATH:/usr/lib/osxcross/macports/pkgs/opt/local/lib/

rm -rf build
mkdir build
cd build
$OSXCROSS_TRIPLE-cmake ../  -DTSMUXER_STATIC_BUILD=ON  -DCMAKE_TOOLCHAIN_FILE=/usr/lib/osxcross/toolchain.cmake -DTSMUXER_GUI=ON
make

cp -r tsMuxerGUI/tsMuxerGUI.app ../bin/tsMuxerGUI.app
mkdir ../bin/tsMuxerGUI.app/Contents/libs 2>/dev/null || :
# copy libs needed by the real binary and fixup its library paths
for_each_dep_lib tsMuxer/tsmuxer '/opt/local/lib/' tsmuxer_lib_copy_change_path
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cp tsMuxer/tsmuxer ../bin/tsMuxerGUI.app/Contents/MacOS/tsMuxeR
cp -t ../bin/tsMuxerGUI.app/Contents/MacOS/ ../bin/*.dylib 2>/dev/null || :
cd ..

pushd bin/tsMuxerGUI.app/Contents

# tell the GUI binary to look for libs in the packaged libs directory
$OSXCROSS_TRIPLE-install_name_tool -add_rpath @executable_path/../libs MacOS/tsMuxerGUI

# fixup tsMuxerGUI to use "local" Qt libs
for_each_dep_lib MacOS/tsMuxerGUI '@rpath/' gui_lib_copy

# copy in the cocoa plugin manually
mkdir -p plugins/platforms
cp "${OSXCROSS_SYSROOT}/opt/local/plugins/platforms/libqcocoa.dylib" plugins/platforms/

# fixup cocoa lib to use "local" Qt libs
for_each_dep_lib plugins/platforms/libqcocoa.dylib '@rpath/' gui_lib_copy

# add the Qt configuration file, so the plugin can be found
cat << EOF > Resources/qt.conf
[Paths]
Plugins=plugins
EOF

popd

rm -rf build

mkdir ./bin/mac
mv ./bin/tsMuxeR ./bin/mac/tsMuxeR
mv ./bin/tsMuxerGUI.app ./bin/mac/tsMuxerGUI.app
mv -t ./bin/mac/ ./bin/*.dylib 2>/dev/null || :
zip -r ./bin/mac.zip ./bin/mac
ls ./bin/mac/tsMuxeR && ls ./bin/mac/tsMuxerGUI.app/Contents/MacOS/tsMuxerGUI && ls ./bin/mac.zip
