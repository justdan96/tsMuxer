TEMPLATE = app
TARGET = ../bin/tsMuxerGUI
QT = core gui 

HEADERS += tsmuxerwindow.h lang_codes.h muxForm.h
SOURCES += main.cpp tsmuxerwindow.cpp muxForm.cpp
FORMS += tsmuxerwindow.ui muxForm.ui
RESOURCES += images.qrc
TRANSLATIONS = 

win32 {
	RC_FILE += icon.rc
}

mac {
	ICON = tsmuxerGUI.icns
}
