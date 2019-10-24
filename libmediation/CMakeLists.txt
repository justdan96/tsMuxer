# cmake_minimum_required (VERSION 3.4)
cmake_minimum_required (VERSION 3.0)
project(mediation)

FILE(GLOB mediation_sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "containers/safequeue.h"
  "fs/directory.cpp"
  "fs/directory.h"
  "fs/file.h"
  "fs/logstream.h"
  "fs/systemlog.h"
  "fs/textfile.h"
  "system/terminatablethread.h"
  "types/types.cpp"
  "types/types.h"
)

IF(MSVC OR MINGW)
    LIST(APPEND mediation_sources "fs/osdep/file_win32.cpp" "system/osdep/common_win32.cpp" "system/osdep/terminatablethread_win32.cpp")
ELSE()
    LIST(APPEND mediation_sources "fs/osdep/file_unix.cpp" "system/osdep/terminatablethread_linux.cpp")
ENDIF()

add_library (mediation ${mediation_sources})
include_directories (mediation "${PROJECT_SOURCE_DIR}")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBMEDIATION_API= -DDISABLE_LOG_SUPPORT -DDEBUG_NEW12=new -DNO_SYSTEM_EXCEPT")
if (MSVC OR MINGW)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
else()
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX")
endif()
