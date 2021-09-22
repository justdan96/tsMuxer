export PATH=/usr/lib/osxcross/bin:$PATH
export MACOSX_DEPLOYMENT_TARGET=10.15

OSXCROSS_WRAPPER=$(ls -1 /usr/lib/osxcross/bin/x86_64-apple-darwin*-wrapper | xargs basename)
echo $OSXCROSS_WRAPPER
OSXCROSS_TRIPLE=$(echo $OSXCROSS_WRAPPER | sed "s/-wrapper//g") 
echo $OSXCROSS_TRIPLE

export PKG_CONFIG=/usr/lib/osxcross/bin/$OSXCROSS_TRIPLE-pkg-config
export OSXCROSS_MP_INC=1
export LD_LIBRARY_PATH=/usr/lib/osxcross/lib:$LD_LIBRARY_PATH:/usr/lib/osxcross/macports/pkgs/opt/local/lib/
rm -rf build
mkdir build
cd build
$OSXCROSS_TRIPLE-cmake ../  -DTSMUXER_STATIC_BUILD=ON -DCMAKE_TOOLCHAIN_FILE=/usr/lib/osxcross/toolchain.cmake 
make
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cd ..
rm -rf build
ls ./bin/tsMuxeR
