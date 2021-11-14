cmake_minimum_required (VERSION 3.1)
project(mediation)

add_library(mediation STATIC
  types/types.cpp
  system/terminatablethread.cpp
)

IF(WIN32)
    target_sources(mediation PRIVATE fs/osdep/file_win32.cpp fs/osdep/directory_win32.cpp)
ELSE()
    target_compile_definitions(mediation PRIVATE "-D_FILE_OFFSET_BITS=64")
    target_sources(mediation PRIVATE fs/osdep/file_unix.cpp fs/osdep/directory_unix.cpp)
ENDIF()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
