cmake_minimum_required (VERSION 3.1)
project (tsmuxer_main CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE GIT_REV_SHORT_RV
  OUTPUT_VARIABLE GIT_REV_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(TSMUXER_RELEASE FALSE)
set(VERSION_NUMBER "2.6.16")
if(${TSMUXER_RELEASE})
  set(TSMUXER_VERSION ${VERSION_NUMBER})
elseif(${GIT_REV_SHORT_RV} EQUAL 0)
  set(TSMUXER_VERSION "git-${GIT_REV_SHORT}")
else()
  set(TSMUXER_VERSION "${VERSION_NUMBER}-dev")
endif()
add_definitions("-DTSMUXER_VERSION=\"${TSMUXER_VERSION}\"")

if(NOT MSVC)
  include(GNUInstallDirs)
  mark_as_advanced(CLEAR
    CMAKE_INSTALL_BINDIR
	CMAKE_INSTALL_DATADIR
  )
else(NOT MSVC)
  set(CMAKE_INSTALL_BINDIR bin)
endif(NOT MSVC)

add_subdirectory (libmediation)
add_subdirectory (tsMuxer)
if(TSMUXER_GUI)
	add_subdirectory (tsMuxerGUI)
endif()
