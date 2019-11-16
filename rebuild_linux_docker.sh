rm -rf build
mkdir build
cd build
cmake -DTSMUXER_STATIC_BUILD=ON ../
make
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cd ..
rm -rf build
