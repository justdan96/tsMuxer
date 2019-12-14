rm -rf build
mkdir build
cd build
cmake  -DTSMUXER_GUI=ON -DTSMUXER_STATIC_BUILD=ON ../
make
cp tsMuxer/tsmuxer ../bin/tsMuxeR
cp tsMuxerGUI/tsMuxerGUI ../bin/tsMuxerGUI
cd ..
rm -rf build
mkdir ./bin/lnx
mv ./bin/tsMuxeR ./bin/lnx/tsMuxeR
mv ./bin/tsMuxerGUI ./bin/lnx/tsMuxerGUI
zip -jr ./bin/lnx.zip ./bin/lnx
ls ./bin/lnx/tsMuxeR && ls ./bin/lnx/tsMuxerGUI && ls ./bin/lnx.zip
