cmake_minimum_required (VERSION 3.1)
project (tsmuxer)

FILE(GLOB tsmuxer_sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "tsMuxer/src/*.cpp"
  "tsMuxer/src/*.h"
)

LIST(REMOVE_ITEM tsmuxer_sources "tsMuxer/src/vodCoreContext.cpp" "tsMuxer/src/vodTransport.cpp")

# set different sources for Windows and *nix
if (MSVC OR MINGW)
    LIST(APPEND tsmuxer_sources "tsMuxer/src/osdep/textSubtitlesRenderWin32.cpp" "tsMuxer/src/osdep/textSubtitlesRenderWin32.h")
    # set it to be static
    set (CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
else()
    LIST(APPEND tsmuxer_sources "tsMuxer/src/osdep/textSubtitlesRenderFT.cpp" "tsMuxer/src/osdep/textSubtitlesRenderFT.h")
    # set it to be static, fix static linking errors for pthread and libpng
    set (CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static -pthread -Wl,--whole-archive -lpthread -lpng -Wl,--no-whole-archive")
endif()

# use C++ 11
set(CMAKE_CXX_STANDARD 11)
# find static libraries
set (CMAKE_FIND_LIBRARY_SUFFIXES ".a")

# create the executable for tsmuxer
add_executable (tsmuxer ${tsmuxer_sources})

# add libmediation
add_subdirectory (libmediation)

# use pkg-config for getting our dependencies
find_package (PkgConfig REQUIRED)

# freetype2 as a dependency
pkg_check_modules (FREETYPE2 REQUIRED freetype2)

# zlib as a dependency
pkg_check_modules (ZLIB REQUIRED zlib)

#libpng as a dependency
pkg_check_modules (LIBPNG libpng REQUIRED)

# add libmediation and freetype to the include folders
include_directories(tsmuxer "${PROJECT_SOURCE_DIR}/libmediation" ${FREETYPE2_INCLUDE_DIRS})

# set some common flags, such as permissive
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBMEDIATION_API= -DDISABLE_LOG_SUPPORT -fpermissive")
if (MSVC OR MINGW)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
  # add gdiplus manually
  target_link_libraries (tsmuxer pthread ${ZLIB_LIBRARIES} mediation ${FREETYPE2_LIBRARIES} gdiplus)
else()
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX")
  # no need for gdiplus, reorder the others
  target_link_libraries (tsmuxer ${LIBPNG_LIBRARIES} ${ZLIB_LIBRARIES} pthread mediation ${FREETYPE2_LIBRARIES})
endif()

install (TARGETS tsmuxer DESTINATION bin)
