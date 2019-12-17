cmake_minimum_required (VERSION 3.1)
project (tsmuxer_main)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

add_subdirectory (libmediation)
add_subdirectory (tsMuxer)
if(TSMUXER_GUI)
	add_subdirectory (tsMuxerGUI)
endif()
