cd tsMuxerGUI

mkdir qmake_build
cd qmake_build
qmake ..
make -j$(nproc --all)

cd ../..

mkdir cmake_build
cd cmake_build
cmake -DTSMUXER_GUI=TRUE -DQT_VERSION=6 ..
make -j$(nproc --all) tsMuxerGUI
