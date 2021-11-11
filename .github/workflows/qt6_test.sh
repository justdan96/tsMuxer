cd tsMuxerGUI
rm -fR build
mkdir build
qmake ..
make -j$(nproc --all)
