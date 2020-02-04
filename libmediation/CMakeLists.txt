cmake_minimum_required (VERSION 3.1)
project(mediation)

add_library(mediation STATIC
  fs/directory.cpp
  types/types.cpp
  system/terminatablethread.cpp
)

IF(WIN32)
    target_compile_definitions(mediation PRIVATE "-DUNICODE")
    target_sources(mediation PRIVATE fs/osdep/file_win32.cpp)
ELSE()
    target_sources(mediation PRIVATE fs/osdep/file_unix.cpp)
ENDIF()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
