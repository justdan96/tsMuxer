cd tsMuxerGUI

rm -fR build
mkdir build
cd build
qmake ..
make -j$(nproc --all)

cd ..

rm -fR build
mkdir build
cd build
cmake -DTSMUXER_GUI=TRUE -DQT_VERSION=6 -GNinja ..
ninja tsMuxerGUI
