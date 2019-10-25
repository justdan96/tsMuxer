cmake_minimum_required (VERSION 3.1)
project (tsmuxer LANGUAGES CXX)

add_executable (tsmuxer
  aac.cpp
  aacStreamReader.cpp
  AbstractDemuxer.cpp
  abstractMuxer.cpp
  ac3Codec.cpp
  ac3StreamReader.cpp
  avCodecs.cpp
  base64.cpp
  bitStream.cpp
  blurayHelper.cpp
  bufferedFileReader.cpp
  bufferedFileWriter.cpp
  bufferedReader.cpp
  bufferedReaderManager.cpp
  combinedH264Demuxer.cpp
  convertUTF.cpp
  crc.cpp
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
  psgStreamReader.cpp
  simplePacketizerReader.cpp
  singleFileMuxer.cpp
  srtStreamReader.cpp
  stdafx.cpp
  textSubtitles.cpp
  textSubtitlesRender.cpp
  tsDemuxer.cpp
  tsMuxer.cpp
  tsPacket.cpp
  utf8Converter.cpp
  vc1Parser.cpp
  vc1StreamReader.cpp
  vod_common.cpp
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
    set (CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
  endif()
endif()

function(pkg_check_modules_with_static prefix req_or_opt package)
  pkg_check_modules (${prefix} ${req_or_opt} ${package})
  if (TSMUXER_STATIC_BUILD)
    set(static_libs "${${prefix}_STATIC_LIBRARIES}")
    set(${prefix}_LIBRARIES "${static_libs}" CACHE INTERNAL "")
  endif()
endfunction()

find_package (Threads REQUIRED)

if (WIN32)
  target_sources(tsmuxer PRIVATE osdep/textSubtitlesRenderWin32.cpp)
else()
  find_package (PkgConfig REQUIRED)
  pkg_check_modules_with_static (FREETYPE2 REQUIRED freetype2)
  pkg_check_modules_with_static (ZLIB REQUIRED zlib)
  target_sources(tsmuxer PRIVATE osdep/textSubtitlesRenderFT.cpp)
endif()

target_include_directories(tsmuxer PRIVATE
  "${PROJECT_SOURCE_DIR}/../libmediation"
  ${ZLIB_INCLUDE_DIRS}
  ${FREETYPE2_INCLUDE_DIRS}
)
target_link_libraries(tsmuxer
  mediation
  Threads::Threads
  ${ZLIB_LIBRARIES}
)

if (WIN32)
  target_link_libraries (tsmuxer gdiplus)
else ()
  target_link_libraries (tsmuxer ${FREETYPE2_LIBRARIES})
endif()

install (TARGETS tsmuxer DESTINATION bin)
