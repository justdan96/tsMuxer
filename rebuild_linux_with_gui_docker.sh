rm -rf build
mkdir build
cd build
cmake  -DTSMUXER_GUI=ON -DTSMUXER_STATIC_BUILD=ON ../
make
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cp tsMuxerGUI/tsMuxerGUI ../bin/tsMuxerGUI
cd ..
rm -rf build
ls ./bin/tsMuxeR && ls ./bin/tsMuxerGUI
