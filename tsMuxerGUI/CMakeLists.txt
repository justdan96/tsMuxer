cmake_minimum_required (VERSION 3.1)
project(tsMuxerGUI CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(Qt5 COMPONENTS Widgets Multimedia LinguistTools REQUIRED)

set(lang_qrc "translations.qrc")
configure_file(${lang_qrc} ${lang_qrc} COPYONLY)
# create_translation is not used due to QTBUG-41736
qt5_add_translation(QM_FILES translations/tsmuxergui_en.ts translations/tsmuxergui_ru.ts translations/tsmuxergui_fr.ts translations/tsmuxergui_zh.ts)

set(tsmuxer_gui_sources
  main.cpp
  tsmuxerwindow.cpp
  muxForm.cpp
  tsmuxerwindow.ui
  muxForm.ui
  checkboxedheaderview.cpp
  images.qrc
  ${QM_FILES}
  ${CMAKE_CURRENT_BINARY_DIR}/${lang_qrc}
)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(GUI_OPTIONS "")
if(WIN32)
  set(GUI_OPTIONS WIN32)
elseif(APPLE)
  set(GUI_OPTIONS MACOSX_BUNDLE)
endif()

add_executable(tsMuxerGUI ${GUI_OPTIONS} ${tsmuxer_gui_sources})
target_link_libraries(tsMuxerGUI Qt5::Widgets Qt5::Multimedia)

if(NOT MSVC)
  install(TARGETS tsMuxerGUI DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES tsMuxerGUI.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES images/icon.png
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps/
		  RENAME tsMuxerGUI.png)
endif(NOT MSVC)

if (WIN32)
  target_sources(tsMuxerGUI PRIVATE icon.rc)
elseif (APPLE)
  set(MACOSX_BUNDLE_ICON_FILE tsMuxerGUI.icns)
  set_source_files_properties(tsMuxerGUI.icns PROPERTIES
         MACOSX_PACKAGE_LOCATION "Resources")
  set_target_properties(tsMuxerGUI PROPERTIES MACOSX_BUNDLE TRUE)
  target_sources(tsMuxerGUI PRIVATE tsMuxerGUI.icns)
endif()
