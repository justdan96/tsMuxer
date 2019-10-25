cmake_minimum_required (VERSION 3.1)
project (tsmuxer_main)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

if(NOT MSVC)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive")
endif()

if (WIN32)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
else()
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX")
endif()

add_subdirectory (libmediation)
add_subdirectory (tsMuxer)
add_subdirectory (tsMuxerGUI)
