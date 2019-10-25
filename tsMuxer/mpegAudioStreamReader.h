#ifndef __MPEG_AUDIO_STREAM_READER
#define __MPEG_AUDIO_STREAM_READER

#include "simplePacketizerReader.h"
#include "mp3Codec.h"

class MpegAudioStreamReader: public SimplePacketizerReader, MP3Codec {
public:
	const static uint32_t DTS_HD_PREFIX = 0x64582025;
	MpegAudioStreamReader(): SimplePacketizerReader() {
	}
	virtual int getTSDescriptor(uint8_t* dstBuff);
	int getLayer() {return m_layer;}
	virtual int getFreq() {return m_sample_rate;}
	virtual int getChannels() {return 2;}
protected:
	virtual unsigned getHeaderLen() {return MPEG_AUDIO_HEADER_SIZE;}; 
	virtual uint8_t* findFrame(uint8_t* buff, uint8_t* end); 
	virtual int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes); 
	virtual double getFrameDurationNano(); 
	virtual const CodecInfo& getCodecInfo() 
	{
		return mpegAudioCodecInfo;
	}
	virtual const std::string getStreamInfo(); 
private:
	static const int MPEG_AUDIO_HEADER_SIZE = 4;
};

#endif
