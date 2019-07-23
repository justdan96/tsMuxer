BUILD=

ifeq ($(MAKECMDGOALS),debug) 
    BUILD=
endif 
ifeq ($(MAKECMDGOALS),release) 
    BUILD=-Os
endif 

DIRS=../bin ../install
CC=g++
DEFINES  = -DLINUX -DLIBMEDIATION_API=  -DDISABLE_LOG_SUPPORT
CFLAGS   = -c -fpic -fexceptions -m32
INCPATH  = -I../libmediation -I/usr/include/freetype2
STATIC_LIBS = ../libmediation/libmediationl.a
LFLAGS   = -m32
LDFLAGS  = -lpthread -lrt -lz -lfreetype -m32
TARGET   = ../bin/tsMuxeR

SOURCES= ./src/aac.cpp ./src/aacStreamReader.cpp ./src/AbstractDemuxer.cpp ./src/abstractMuxer.cpp ./src/ac3Codec.cpp ./src/ac3StreamReader.cpp \
./src/avCodecs.cpp ./src/base64.cpp ./src/bitStream.cpp ./src/blurayHelper.cpp ./src/bufferedFileReader.cpp ./src/bufferedFileWriter.cpp \
./src/bufferedReader.cpp ./src/bufferedReaderManager.cpp  ./src/convertUTF.cpp ./src/crc.cpp ./src/dtsStreamReader.cpp ./src/vc1StreamReader.cpp \
./src/dvbSubStreamReader.cpp ./src/h264StreamReader.cpp ./src/ioContextDemuxer.cpp ./src/iso_writer.cpp ./src/vod_common.cpp \
./src/lpcmStreamReader.cpp ./src/main.cpp ./src/matroskaDemuxer.cpp ./src/matroskaParser.cpp ./src/metaDemuxer.cpp ./src/hevcStreamReader.cpp ./src/hevc.cpp \
./src/movDemuxer.cpp ./src/mp3Codec.cpp ./src/mpeg2StreamReader.cpp ./src/mpegAudioStreamReader.cpp ./src/mpegStreamReader.cpp ./src/mpegVideo.cpp \
./src/muxerManager.cpp ./src/nalUnits.cpp ./src/pesPacket.cpp ./src/programStreamDemuxer.cpp ./src/psgStreamReader.cpp ./src/wave.cpp \
./src/simplePacketizerReader.cpp ./src/singleFileMuxer.cpp ./src/srtStreamReader.cpp ./src/stdafx.cpp ./src/textSubtitles.cpp ./src/textSubtitlesRender.cpp \
./src/textSubtitlesRenderFT.cpp  ./src/tsDemuxer.cpp ./src/tsMuxer.cpp ./src/tsPacket.cpp ./src/utf8Converter.cpp ./src/vc1Parser.cpp ./src/combinedH264Demuxer.cpp

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
	
install:
	cp ../bin/tsMuxeR ../install/
	cp ../bin/tsMuxerGUI ../install/
	cp ../CHANGELOG.md ../install/release_notes.txt
	upx --lzma ../install/tsMuxeR
	upx --lzma ../install/tsMuxerGUI
	tar -czf ../tsMuxeR.tar.gz -C ../install .
                
