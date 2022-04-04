rm -rf build
mkdir build
cd build
cmake ../ -G Ninja -DTSMUXER_GUI=ON
ninja
