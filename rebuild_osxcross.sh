export PATH=/usr/lib/osxcross/bin:$PATH
export MACOSX_DEPLOYMENT_TARGET=10.10
export PKG_CONFIG=/usr/lib/osxcross/bin/x86_64-apple-darwin14-pkg-config
export OSXCROSS_MP_INC=1
rm -rf build
mkdir build
cd build
x86_64-apple-darwin14-cmake ../ -DCMAKE_TOOLCHAIN_FILE=/usr/lib/osxcross/toolchain.cmake
make
