#ifndef __AAC_STREAM_READER_H
#define __AAC_STREAM_READER_H

#include "avPacket.h"
#include "simplePacketizerReader.h"
#include "aac.h"

class AACStreamReader: public SimplePacketizerReader, public AACCodec {
public:
public:
	AACStreamReader(): SimplePacketizerReader() {};
	virtual int getTSDescriptor(uint8_t* dstBuff) {return 0;}
	virtual int getFreq() {return m_sample_rate;}
	virtual int getChannels() {return m_channels;}
protected:
	virtual unsigned getHeaderLen();
	virtual int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes);
	virtual uint8_t* findFrame(uint8_t* buff, uint8_t* end) {
		return findAacFrame(buff,end);
	}
	virtual double getFrameDurationNano() {return (INTERNAL_PTS_FREQ * m_samples) / (double) m_sample_rate;}
	virtual const CodecInfo& getCodecInfo() {return aacCodecInfo;}
	virtual const std::string getStreamInfo(); 
};

#endif
