cmake_minimum_required (VERSION 3.1)
project(tsMuxerGUI CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

set(QT_VERSION 5 CACHE STRING "Qt version to use")
set_property(CACHE QT_VERSION PROPERTY STRINGS 5 6)

set(qt_ver_pfx "Qt${QT_VERSION}")

find_package(${qt_ver_pfx}
  REQUIRED COMPONENTS Widgets LinguistTools
  OPTIONAL_COMPONENTS Multimedia)
set(tsmuxer_gui_libs ${qt_ver_pfx}::Widgets)

if(${${qt_ver_pfx}Multimedia_FOUND})
  list(APPEND tsmuxer_gui_libs ${qt_ver_pfx}::Multimedia)
endif()

set(tsmuxer_ts_files
  translations/tsmuxergui_en.ts
  translations/tsmuxergui_ru.ts
  translations/tsmuxergui_fr.ts
  translations/tsmuxergui_zh.ts
  translations/tsmuxergui_de.ts
  translations/tsmuxergui_he.ts
  translations/tsmuxergui_es.ts
)

set(lang_qrc "translations.qrc")
configure_file(${lang_qrc} ${lang_qrc} COPYONLY)

if(${QT_VERSION} EQUAL 5)
  # create_translation is not used due to QTBUG-41736
  qt5_add_translation(QM_FILES ${tsmuxer_ts_files})
elseif(${QT_VERSION} EQUAL 6)
  # should use qt6_add_translations once Qt6 is the only supported version.
  # this will also make ${lang_qrc} and all QM_FILES shenanigans unnecessary as
  # that command turns the compiled qrcs into resources automatically, and adds
  # them to the target's sources so they're embedded in the final binary.
  qt6_add_translation(QM_FILES ${tsmuxer_ts_files})
endif()

set(tsmuxer_gui_sources
  main.cpp
  tsmuxerwindow.cpp
  muxForm.cpp
  lang_codes.cpp
  tsmuxerwindow.ui
  muxForm.ui
  checkboxedheaderview.cpp
  images.qrc
  fontsettingstablemodel.cpp
  ${QM_FILES}
  ${CMAKE_CURRENT_BINARY_DIR}/${lang_qrc}
)

if (WIN32)
  add_executable(tsMuxerGUI WIN32 ${tsmuxer_gui_sources} icon.rc)
elseif (APPLE)
  set(MACOSX_BUNDLE_ICON_FILE tsMuxerGUI.icns)
  set_source_files_properties(tsMuxerGUI.icns PROPERTIES
         MACOSX_PACKAGE_LOCATION "Resources")
  add_executable(tsMuxerGUI MACOSX_BUNDLE ${tsmuxer_gui_sources} tsMuxerGUI.icns)
# UNIX must be last here, as it's going to be true for APPLE and CYGWIN as well,
# but we want those to be handled by the code above.
elseif (UNIX)
  add_executable(tsMuxerGUI ${tsmuxer_gui_sources})
  install(TARGETS tsMuxerGUI DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES tsMuxerGUI.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES images/icon.png
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps/
          RENAME tsMuxerGUI.png)
endif()

target_link_libraries(tsMuxerGUI ${tsmuxer_gui_libs})
