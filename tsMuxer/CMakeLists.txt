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

if(TSMUXER_STATIC_BUILD AND MSVC)
  target_compile_options(tsmuxer "/MT$<$<CONFIG:Debug>:d>")
endif()

find_package (Threads REQUIRED)

# needed by the Matroska demuxer
find_package (ZLIB REQUIRED)

target_include_directories(tsmuxer PRIVATE
  "${PROJECT_SOURCE_DIR}/../libmediation"
  ${ZLIB_INCLUDE_DIRS}
)

set(TSMUXER_FONT_RENDERER_BACKEND "" CACHE STRING "Font renderer to use")
set_property(CACHE TSMUXER_FONT_RENDERER_BACKEND PROPERTY STRINGS freetype gdi)

if(WIN32)
    set(TSMUXER_FONT_RENDERER_BACKEND gdi)
else()
    set(TSMUXER_FONT_RENDERER_BACKEND freetype)
endif()

if (${TSMUXER_FONT_RENDERER_BACKEND} STREQUAL "gdi")
  target_sources(tsmuxer PRIVATE osdep/textSubtitlesRenderWin32.cpp)
  target_link_libraries(tsmuxer gdiplus)
elseif(${TSMUXER_FONT_RENDERER_BACKEND} STREQUAL "freetype")
  target_sources(tsmuxer PRIVATE osdep/textSubtitlesRenderFT.cpp)
  find_package(Freetype REQUIRED)
  target_link_libraries(tsmuxer ${FREETYPE_LIBRARIES} ${FREETYPE_EXTRA_LIBRARIES})
  target_include_directories(tsmuxer PRIVATE ${FREETYPE_INCLUDE_DIRS})
endif()

target_link_libraries(tsmuxer mediation Threads::Threads ${ZLIB_LIBRARIES})
install (TARGETS tsmuxer DESTINATION ${CMAKE_INSTALL_BINDIR})
