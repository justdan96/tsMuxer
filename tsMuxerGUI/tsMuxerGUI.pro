# this file exists only because of the problems with running CMake with Qt
# projects under MXE.
# you're free to use it for compiling the GUI if you don't want to use CMake for
# whatever reason, but keep in mind that it will be removed once the relevant
# bugs in MXE are resolved.

TEMPLATE = app
TARGET = tsMuxerGUI
QT = core gui widgets multimedia
CONFIG += c++14 strict_c++ lrelease embed_translations

HEADERS += tsmuxerwindow.h lang_codes.h muxForm.h checkboxedheaderview.h \
           codecinfo.h
SOURCES += main.cpp tsmuxerwindow.cpp muxForm.cpp checkboxedheaderview.cpp
FORMS += tsmuxerwindow.ui muxForm.ui

RESOURCES += images.qrc
TRANSLATIONS = translations/tsmuxergui_en.ts translations/tsmuxergui_ru.ts translations/tsmuxergui_fr.ts translations/tsmuxergui_zh.ts
win32 {
  RC_FILE += icon.rc
}

version_num = 2.6.16
tsmuxer_release = 0
equals(tsmuxer_release, 1) {
  tsmuxer_version = $${version_num}
} else {
  system(git status >/dev/null 2>&1) {
    git_rev_short = $$system(git rev-parse --short HEAD)
    tsmuxer_version = git-$${git_rev_short}
  } else {
    tsmuxer_version = $${version_num}-dev
  }
}

DEFINES += TSMUXER_VERSION=\\\"$${tsmuxer_version}\\\"
