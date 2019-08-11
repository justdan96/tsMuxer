cmake_minimum_required (VERSION 3.4)
project (tsmuxer)

FILE(GLOB tsmuxer_sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "tsMuxer/src/*.cpp"
  "tsMuxer/src/*.h"
)

LIST(REMOVE_ITEM tsmuxer_sources "tsMuxer/src/vodCoreContext.cpp" "tsMuxer/src/vodTransport.cpp")

if (MSVC OR MINGW)
    LIST(APPEND tsmuxer_sources "tsMuxer/src/osdep/textSubtitlesRenderWin32.cpp" "tsMuxer/src/osdep/textSubtitlesRenderWin32.h")
else()
    LIST(APPEND tsmuxer_sources "tsMuxer/src/osdep/textSubtitlesRenderFT.cpp" "tsMuxer/src/osdep/textSubtitlesRenderFT.h")
endif()


add_executable (tsmuxer ${tsmuxer_sources})
add_subdirectory (libmediation)

find_package (PkgConfig REQUIRED)
pkg_check_modules (FREETYPE2 REQUIRED freetype2)

include_directories(tsmuxer "${PROJECT_SOURCE_DIR}/libmediation" ${FREETYPE2_INCLUDE_DIRS})
target_link_libraries (tsmuxer pthread z mediation ${FREETYPE2_LIBRARIES})

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBMEDIATION_API= -DDISABLE_LOG_SUPPORT")
if (MSVC OR MINGW)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
else()
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX")
endif()

install (TARGETS tsmuxer DESTINATION bin)
