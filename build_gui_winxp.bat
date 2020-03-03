mkdir C:\Qt\
powershell -Command "Invoke-WebRequest https://files.catbox.moe/y5f5be.7z -O qt-xp-static.7z"
7z x -oC:\Qt\ qt-xp-static.7z

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
set INCLUDE=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Include;%INCLUDE%
set PATH=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Bin;%PATH%
set LIB=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Lib;%LIB%

cd tsMuxerGUI
mkdir build
cd build
"C:\Qt\5.6.3-Static-XP\bin\qmake.exe" ..
nmake all
