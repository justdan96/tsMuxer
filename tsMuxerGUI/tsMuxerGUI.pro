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
	LIBS += "C:\\Program Files (x86)\\Windows Kits\\8.1\\Lib\\winv6.3\\um\\x86\\User32.Lib"
	DESTDIR = ..\\bin
	QMAKE_POST_LINK +=$$quote(cmd /c copy /y C:\\Qt\\qt-4.8.7-vs2017-32\\bin\\QtCore4.dll ..\\bin\\ $$escape_expand(\\n\\t))
	QMAKE_POST_LINK +=$$quote(cmd /c copy /y C:\\Qt\\qt-4.8.7-vs2017-32\\bin\\QtGui4.dll ..\\bin\\ $$escape_expand(\\n\\t))
}

mac {
	ICON = tsmuxerGUI.icns
}
