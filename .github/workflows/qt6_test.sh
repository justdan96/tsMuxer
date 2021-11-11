cd tsMuxerGUI
rm -fR build
mkdir build
cd build
qmake ..
make -j$(nproc --all)
