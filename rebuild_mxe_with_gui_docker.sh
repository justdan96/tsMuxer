export PATH=/usr/lib/mxe/usr/bin:$PATH
rm -rf build
mkdir build
cd build
export CCACHE_DISABLE=1
export MXE_USE_CCACHE=
x86_64-w64-mingw32.static-cmake ../
make
cp tsMuxer/tsmuxer.exe ../bin/tsMuxeR.exe

# cmake with the static Qt5 in MXE has major issues - we have to use qmake as a workaround
cat << EOF > ../tsMuxerGUI/tsMuxerGUI.pro
TEMPLATE = app
TARGET = tsMuxerGUI
QT = core gui widgets multimedia

HEADERS += tsmuxerwindow.h lang_codes.h muxForm.h
SOURCES += main.cpp tsmuxerwindow.cpp muxForm.cpp
FORMS += tsmuxerwindow.ui muxForm.ui
RESOURCES += images.qrc
TRANSLATIONS = 

win32 {
	RC_FILE += icon.rc
}
EOF
x86_64-w64-mingw32.static-qmake-qt5 ../tsMuxerGUI
make
cp ./release/tsMuxerGUI.exe ../bin/
cd ..
rm -rf build
rm -f ./tsMuxerGUI/tsMuxerGUI.pro

mkdir ./bin/w64
mv ./bin/tsMuxeR.exe ./bin/w64/tsMuxeR.exe
mv ./bin/tsMuxerGUI.exe ./bin/w64/tsMuxerGUI.exe
zip -jr ./bin/w64.zip ./bin/w64
ls ./bin/w64/tsMuxeR.exe && ls ./bin/w64/tsMuxerGUI.exe && ls ./bin/w64.zip
