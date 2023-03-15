#ifndef __AAC_STREAM_READER_H
#define __AAC_STREAM_READER_H

#include "aac.h"
#include "avPacket.h"
#include "simplePacketizerReader.h"

class AACStreamReader : public SimplePacketizerReader, public AACCodec
{
   public:
    AACStreamReader() : SimplePacketizerReader(){};
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) override;
    int getFreq() override { return m_sample_rate; }
    int getChannels() override { return m_channels; }

   protected:
    int getHeaderLen() override;
    int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes) override;
    uint8_t* findFrame(uint8_t* buff, uint8_t* end) override { return findAacFrame(buff, end); }
    double getFrameDurationNano() override { return (INTERNAL_PTS_FREQ * m_samples) / (double)m_sample_rate; }
    const CodecInfo& getCodecInfo() override { return aacCodecInfo; }
    const std::string getStreamInfo() override;
};

#endif
