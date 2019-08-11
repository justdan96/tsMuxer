CONFIG += qt
CONFIG += precompile_header
TEMPLATE = app
TARGET = ../bin/tsMuxerGUI
QT = core gui

HEADERS += tsmuxerwindow.h lang_codes.h muxForm.h
SOURCES += main.cpp tsmuxerwindow.cpp muxForm.cpp
FORMS += tsmuxerwindow.ui muxForm.ui
RESOURCES += images.qrc
TRANSLATIONS =

# Qt 5+ adjustments
greaterThan(QT_MAJOR_VERSION, 4) { # QT5+
    QT += widgets # for all widgets
    QT += multimedia # for QSound
}

mac {
 ICON = tsmuxerGUI.icns
}

win32 {
   RC_ICONS = tsMuxerGUI.ico
   CONFIG += static
}