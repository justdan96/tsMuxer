cmake_minimum_required (VERSION 3.1)
project (tsmuxer LANGUAGES CXX)

add_executable (tsmuxer
  aac.cpp
  aacStreamReader.cpp
  abstractDemuxer.cpp
  abstractMuxer.cpp
  ac3Codec.cpp
  ac3StreamReader.cpp
  avCodecs.cpp
  bitStream.cpp
  blurayHelper.cpp
  bufferedFileReader.cpp
  bufferedFileWriter.cpp
  bufferedReader.cpp
  bufferedReaderManager.cpp
  combinedH264Demuxer.cpp
  convertUTF.cpp
  dtsStreamReader.cpp
  dvbSubStreamReader.cpp
  h264StreamReader.cpp
  hevc.cpp
  hevcStreamReader.cpp
  ioContextDemuxer.cpp
  iso_writer.cpp
  lpcmStreamReader.cpp
  main.cpp
  matroskaDemuxer.cpp
  matroskaParser.cpp
  metaDemuxer.cpp
  mlpCodec.cpp
  mlpStreamReader.cpp
  movDemuxer.cpp
  mp3Codec.cpp
  mpeg2StreamReader.cpp
  mpegAudioStreamReader.cpp
  mpegStreamReader.cpp
  mpegVideo.cpp
  muxerManager.cpp
  nalUnits.cpp
  pesPacket.cpp
  programStreamDemuxer.cpp
  pgsStreamReader.cpp
  simplePacketizerReader.cpp
  singleFileMuxer.cpp
  srtStreamReader.cpp
  textSubtitles.cpp
  textSubtitlesRender.cpp
  tsDemuxer.cpp
  tsMuxer.cpp
  tsPacket.cpp
  utf8Converter.cpp
  vc1Parser.cpp
  vc1StreamReader.cpp
  vod_common.cpp
  vvc.cpp
  vvcStreamReader.cpp
  wave.cpp
)

if(TSMUXER_STATIC_BUILD)
  if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(tsmuxer "/MTd")
    else()
      target_compile_options(tsmuxer "/MT")
    endif()
  else()
    # static linking isn't supported on Mac
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
      set (CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
    endif()
  endif()
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
endif()

function(pkg_check_modules_with_static prefix req_or_opt package)
  pkg_check_modules (${prefix} ${req_or_opt} ${package})
  if (TSMUXER_STATIC_BUILD)
    set(static_libs "${${prefix}_STATIC_LIBRARIES}")
    # for osxcross if we want a mostly static build we need to append "-static" to the libraries
    if(DEFINED OSXCROSS_SDK)
      list(TRANSFORM static_libs APPEND "-static")
    endif()
    set(${prefix}_LIBRARIES "${static_libs}" CACHE INTERNAL "")
  endif()
endfunction()

find_package (Threads REQUIRED)
find_package (PkgConfig)

if (${PkgConfig_FOUND} AND NOT ${WITHOUT_PKGCONFIG})
  pkg_check_modules_with_static (ZLIB REQUIRED zlib)
  if (NOT WIN32)
    pkg_check_modules_with_static (FREETYPE REQUIRED freetype2)
  endif()
else()
  find_package(ZLIB REQUIRED)
  find_package(Freetype REQUIRED)
endif()

target_include_directories(tsmuxer PRIVATE
  "${PROJECT_SOURCE_DIR}/../libmediation"
  ${ZLIB_INCLUDE_DIRS}
)

# this part looks messy as it is working around a bug in pthread when static linking
SET(THREADSLIB Threads::Threads)
if(TSMUXER_STATIC_BUILD)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    message("-- Static Linux build, will link whole pthread!")
    set_target_properties(tsmuxer PROPERTIES LINK_SEARCH_START_STATIC 1)
    set_target_properties(tsmuxer PROPERTIES LINK_SEARCH_END_STATIC 1)
    SET(THREADSLIB -pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive)
  endif()
endif()

if (WIN32)
  target_sources(tsmuxer PRIVATE osdep/textSubtitlesRenderWin32.cpp)
  target_link_libraries(tsmuxer gdiplus)
else()
  target_sources(tsmuxer PRIVATE osdep/textSubtitlesRenderFT.cpp)
  # on osxcross use the static freetype library explicitly
  if(DEFINED OSXCROSS_SDK)
    list(TRANSFORM FREETYPE_LDFLAGS REPLACE "(-lfreetype)" "-lfreetype-static")
  endif()
  target_link_libraries(tsmuxer ${FREETYPE_LIBRARIES} ${FREETYPE_LDFLAGS})
  target_include_directories(tsmuxer PRIVATE ${FREETYPE_INCLUDE_DIRS})
endif()

target_link_libraries(tsmuxer mediation ${THREADSLIB} ${ZLIB_LIBRARIES})

install (TARGETS tsmuxer DESTINATION ${CMAKE_INSTALL_BINDIR})
