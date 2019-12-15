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

# create AppImage of the GUI

mkdir -p ./bin/lnx/AppDir/usr/share/applications
mkdir -p ./bin/lnx/AppDir/usr/share/icons/hicolor/32x32/apps
mkdir -p ./bin/lnx/AppDir/usr/bin

mv ./bin/tsMuxerGUI ./bin/lnx/AppDir/usr/bin/tsMuxerGUI

cp ./tsMuxerGUI/images/icon.png ./bin/lnx/AppDir/usr/share/icons/hicolor/32x32/apps/tsMuxerGUI.png

cat << EOF > ./bin/lnx/AppDir/usr/share/applications/tsmuxergui.desktop
[Desktop Entry]
Name=tsMuxerGUI
Comment=tsMuxerGUI
Exec=tsMuxerGUI
Icon=tsMuxerGUI
Terminal=false
Type=Application
Categories=AudioVideo;
StartupNotify=true
EOF

cd ./bin/lnx

/opt/linuxdeploy/usr/bin/linuxdeploy --appdir AppDir --plugin qt --output appimage

cd ../..

rm -rf ./bin/lnx/AppDir

zip -r ./bin/lnx.zip ./bin/lnx
ls ./bin/lnx/tsMuxeR && ls ./bin/lnx/tsMuxerGUI-*-x86_64.AppImage && ls ./bin/lnx.zip
