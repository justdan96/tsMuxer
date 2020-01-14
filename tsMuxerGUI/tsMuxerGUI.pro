# this file exists only because of the problems with running CMake with Qt
# projects under MXE.
# you're free to use it for compiling the GUI if you don't want to use CMake for
# whatever reason, but keep in mind that it will be removed once the relevant
# bugs in MXE are resolved.

TEMPLATE = app
TARGET = tsMuxerGUI
QT = core gui widgets multimedia
CONFIG += c++14 strict_c++

HEADERS += tsmuxerwindow.h lang_codes.h muxForm.h checkboxedheaderview.h \
           codecinfo.h
SOURCES += main.cpp tsmuxerwindow.cpp muxForm.cpp checkboxedheaderview.cpp
FORMS += tsmuxerwindow.ui muxForm.ui

RESOURCES += images.qrc
TRANSLATIONS = 
win32 {
  RC_FILE += icon.rc
}
