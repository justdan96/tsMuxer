export PATH=/usr/lib/osxcross/bin:$PATH
export MACOSX_DEPLOYMENT_TARGET=10.10
export PKG_CONFIG=/usr/lib/osxcross/bin/x86_64-apple-darwin14-pkg-config
export OSXCROSS_MP_INC=1
rm -rf build
mkdir build
cd build
x86_64-apple-darwin14-cmake ../ -DCMAKE_TOOLCHAIN_FILE=/usr/lib/osxcross/toolchain.cmake -DTSMUXER_GUI=ON
make
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cp -r tsMuxerGUI/tsMuxerGUI.app ../bin/tsMuxerGUI.app
cp tsMuxer/tsmuxer ../bin/tsMuxerGUI.app/Contents/MacOS/tsMuxeR
cd ..

# only copy all the libraries for MacOS if we actually built the GUI successfully
if test -f ./bin/tsMuxerGUI.app/Contents/MacOS/tsMuxerGUI; then
  # add a new relative path to the executable, to the libs folder we create
  x86_64-apple-darwin14-install_name_tool -add_rpath @executable_path/../libs ./bin/tsMuxerGUI.app/Contents/MacOS/tsMuxerGUI
  mkdir -p ./bin/tsMuxerGUI.app/Contents/libs/

  # paths to the libraries on this machine
  for LIB in `x86_64-apple-darwin14-otool -L ./bin/tsMuxerGUI.app/Contents/MacOS/tsMuxerGUI | awk '/@rpath/ {gsub(/@rpath\//, "", $1) ; print $1 }'`
  do
    mkdir -p `dirname ./bin/tsMuxerGUI.app/Contents/libs/$LIB`
    cp /usr/lib/osxcross/macports/pkgs/opt/local/lib/$LIB ./bin/tsMuxerGUI.app/Contents/libs/$LIB
  done

  # copy in the cocoa plugin manually
  mkdir -p ./bin/tsMuxerGUI.app/Contents/plugins/platforms/
  cp /usr/lib/osxcross/macports/pkgs/opt/local/plugins/platforms/libqcocoa.dylib ./bin/tsMuxerGUI.app/Contents/plugins/platforms/libqcocoa.dylib

  # paths to the libraries on this machine
  for LIB in `x86_64-apple-darwin14-otool -L ./bin/tsMuxerGUI.app/Contents/plugins/platforms/libqcocoa.dylib| awk '/@rpath/ {gsub(/@rpath\//, "", $1) ; print $1 }'`
  do
    mkdir -p `dirname ./bin/tsMuxerGUI.app/Contents/libs/$LIB`
    cp /usr/lib/osxcross/macports/pkgs/opt/local/lib/$LIB ./bin/tsMuxerGUI.app/Contents/libs/$LIB
  done

  # add the Qt configuration file, so the plugins can be found
  cat << EOF > ./bin/tsMuxerGUI.app/Contents/Resources/qt.conf
[Paths]
Plugins=plugins
EOF
fi

rm -rf build

mkdir ./bin/mac
mv ./bin/tsMuxeR ./bin/mac/tsMuxeR
mv ./bin/tsMuxerGUI.app ./bin/mac/tsMuxerGUI.app
zip -r ./bin/mac.zip ./bin/mac
ls ./bin/mac/tsMuxeR && ls ./bin/mac/tsMuxerGUI.app/Contents/MacOS/tsMuxerGUI && ls ./bin/lnx.zip
