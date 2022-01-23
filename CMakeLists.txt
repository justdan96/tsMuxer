cmake_minimum_required (VERSION 3.1)
project (tsmuxer_main CXX)

set(CMAKE_CXX_STANDARD 17)
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

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  include(GNUInstallDirs)
  mark_as_advanced(CLEAR
    CMAKE_INSTALL_BINDIR
    CMAKE_INSTALL_DATADIR
  )
else()
  set(CMAKE_INSTALL_BINDIR bin)
endif()

if(MSVC)
  add_compile_options("/utf-8")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
  add_definitions(-DUNICODE -D_UNICODE)
endif()

if(TSMUXER_STATIC_BUILD AND UNIX)
  list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .a)
  list(REMOVE_DUPLICATES CMAKE_FIND_LIBRARY_SUFFIXES)
endif()

set(TSMUXER_COVERAGE FALSE CACHE BOOL "Enable collecting coverage information")
if(${TSMUXER_COVERAGE})
  if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    add_compile_options("--coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
  elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    add_compile_options("-fprofile-instr-generate" "-fcoverage-mapping")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-instr-generate")
  else()
    message(FATAL_ERROR "Coverage not supported for compiler ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()

add_subdirectory(libmediation)
add_subdirectory(tsMuxer)
if(TSMUXER_GUI)
  add_subdirectory(tsMuxerGUI)
endif()
