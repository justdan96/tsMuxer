export PATH=/usr/lib/mxe/usr/bin:$PATH
rm -rf build
mkdir build
cd build
export CCACHE_DISABLE=1
export MXE_USE_CCACHE=
x86_64-w64-mingw32.static-cmake ../ -DTSMUXER_GUI=ON
make
cp tsMuxer/tsmuxer.exe ../bin/tsMuxeR.exe
cp tsMuxer/tsmuxergui.exe ../bin/tsMuxerGUI.exe
cd ..
rm -rf build
