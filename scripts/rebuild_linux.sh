rm -rf build
mkdir build
cd build
cmake ../ -G Ninja
ninja
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cd ..
rm -rf build
