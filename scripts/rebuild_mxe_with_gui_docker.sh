export PATH=/usr/lib/mxe/usr/bin:$PATH
rm -rf build
mkdir build
mkdir ./bin/w64
cd build
export CCACHE_DISABLE=1
export MXE_USE_CCACHE=

x86_64-w64-mingw32.static-cmake ../
make
mv tsMuxer/tsmuxer.exe ../bin/w64/tsMuxeR.exe

x86_64-w64-mingw32.static-qmake-qt5 ../tsMuxerGUI
make
mv ./tsMuxerGUI.exe ../bin/w64/tsMuxerGUI.exe
cd ..
rm -rf build
zip -jr ./bin/w64.zip ./bin/w64
ls ./bin/w64/tsMuxeR.exe && ls ./bin/w64/tsMuxerGUI.exe && ls ./bin/w64.zip
