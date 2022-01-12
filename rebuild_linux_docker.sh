rm -rf build
mkdir build
cd build
cmake -DTSMUXER_STATIC_BUILD=ON -DFREETYPE_LDFLAGS=png ../
make
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cd ..
rm -rf build
ls ./bin/tsMuxeR
