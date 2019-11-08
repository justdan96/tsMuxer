rm -rf build
mkdir build
cd build
cmake ../
make
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cd ..
rm -rf build
