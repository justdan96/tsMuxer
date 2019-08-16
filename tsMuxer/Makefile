BUILD=

ifeq ($(MAKECMDGOALS),debug) 
    BUILD=
endif 
ifeq ($(MAKECMDGOALS),release) 
    BUILD=-Os
endif 

DIRS=../bin ../install
CC=g++

ifdef OS
   DEFINES  = -DWIN32 -DLIBMEDIATION_API=  -DDISABLE_LOG_SUPPORT
   TARGET   = ../bin/tsMuxeR.exe
   LDFLAGS  = -lpthread -lz -lfreetype -lwinmm -lgdi32 -lgdiplus -m32 -static
   ifdef MSYSTEM
      INCPATH  = -I../libmediation -I/mingw32/include/freetype2
   else
      INCPATH  = -I../libmediation -I/usr/include/freetype2
   endif
   CFLAGS   = -c -fpic -fexceptions -m32
   LFLAGS   = -m32
   SOURCES= ./src/aac.cpp ./src/aacStreamReader.cpp ./src/AbstractDemuxer.cpp ./src/abstractMuxer.cpp ./src/ac3Codec.cpp ./src/ac3StreamReader.cpp \
./src/avCodecs.cpp ./src/base64.cpp ./src/bitStream.cpp ./src/blurayHelper.cpp ./src/bufferedFileReader.cpp ./src/bufferedFileWriter.cpp \
./src/bufferedReader.cpp ./src/bufferedReaderManager.cpp  ./src/convertUTF.cpp ./src/crc.cpp ./src/dtsStreamReader.cpp ./src/vc1StreamReader.cpp \
./src/dvbSubStreamReader.cpp ./src/h264StreamReader.cpp ./src/ioContextDemuxer.cpp ./src/iso_writer.cpp ./src/vod_common.cpp \
./src/lpcmStreamReader.cpp ./src/main.cpp ./src/matroskaDemuxer.cpp ./src/matroskaParser.cpp ./src/metaDemuxer.cpp ./src/hevcStreamReader.cpp ./src/hevc.cpp \
./src/movDemuxer.cpp ./src/mp3Codec.cpp ./src/mpeg2StreamReader.cpp ./src/mpegAudioStreamReader.cpp ./src/mpegStreamReader.cpp ./src/mpegVideo.cpp \
./src/muxerManager.cpp ./src/nalUnits.cpp ./src/pesPacket.cpp ./src/programStreamDemuxer.cpp ./src/psgStreamReader.cpp ./src/wave.cpp \
./src/simplePacketizerReader.cpp ./src/singleFileMuxer.cpp ./src/srtStreamReader.cpp ./src/stdafx.cpp ./src/textSubtitles.cpp ./src/textSubtitlesRender.cpp \
./src/textSubtitlesRenderWin32.cpp  ./src/tsDemuxer.cpp ./src/tsMuxer.cpp ./src/tsPacket.cpp ./src/utf8Converter.cpp ./src/vc1Parser.cpp ./src/combinedH264Demuxer.cpp
else
   DEFINES  = -DLINUX -DLIBMEDIATION_API=  -DDISABLE_LOG_SUPPORT
   TARGET   = ../bin/tsMuxeR
	 LDFLAGS  = -lpthread -lrt -lz -lfreetype
	 CFLAGS   = -c -fpic -fexceptions
	 LFLAGS   =
   INCPATH  = -I../libmediation -I/usr/include/freetype2
   SOURCES= ./src/aac.cpp ./src/aacStreamReader.cpp ./src/AbstractDemuxer.cpp ./src/abstractMuxer.cpp ./src/ac3Codec.cpp ./src/ac3StreamReader.cpp \
./src/avCodecs.cpp ./src/base64.cpp ./src/bitStream.cpp ./src/blurayHelper.cpp ./src/bufferedFileReader.cpp ./src/bufferedFileWriter.cpp \
./src/bufferedReader.cpp ./src/bufferedReaderManager.cpp  ./src/convertUTF.cpp ./src/crc.cpp ./src/dtsStreamReader.cpp ./src/vc1StreamReader.cpp \
./src/dvbSubStreamReader.cpp ./src/h264StreamReader.cpp ./src/ioContextDemuxer.cpp ./src/iso_writer.cpp ./src/vod_common.cpp \
./src/lpcmStreamReader.cpp ./src/main.cpp ./src/matroskaDemuxer.cpp ./src/matroskaParser.cpp ./src/metaDemuxer.cpp ./src/hevcStreamReader.cpp ./src/hevc.cpp \
./src/movDemuxer.cpp ./src/mp3Codec.cpp ./src/mpeg2StreamReader.cpp ./src/mpegAudioStreamReader.cpp ./src/mpegStreamReader.cpp ./src/mpegVideo.cpp \
./src/muxerManager.cpp ./src/nalUnits.cpp ./src/pesPacket.cpp ./src/programStreamDemuxer.cpp ./src/psgStreamReader.cpp ./src/wave.cpp \
./src/simplePacketizerReader.cpp ./src/singleFileMuxer.cpp ./src/srtStreamReader.cpp ./src/stdafx.cpp ./src/textSubtitles.cpp ./src/textSubtitlesRender.cpp \
./src/textSubtitlesRenderFT.cpp  ./src/tsDemuxer.cpp ./src/tsMuxer.cpp ./src/tsPacket.cpp ./src/utf8Converter.cpp ./src/vc1Parser.cpp ./src/combinedH264Demuxer.cpp
endif

STATIC_LIBS = ../libmediation/libmediationl.a

OBJECTS=$(SOURCES:.cpp=.o)

$(shell mkdir -p $(DIRS))

all: $(SOURCES) $(TARGET)
.PHONY : debug release 
debug: all 
release: all

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(STATIC_LIBS) $(LDFLAGS) -o $@
        
.cpp.o: 
	$(CC) $(DEFINES) $(BUILD) $(LFLAGS) $(CFLAGS)  $(INCPATH) $(BUILD) $? -o $@

clean:
	rm -f $(OBJECTS)
	rm -f ./tsMuxeR*
	rm -f $(TARGET)
	
install:
	cp ../bin/tsMuxeR* ../install/
	cp ../bin/tsMuxerGUI* ../install/
	cp ../CHANGELOG.md ../install/release_notes.txt
	
                
