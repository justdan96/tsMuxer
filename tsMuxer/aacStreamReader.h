#ifndef AAC_STREAM_READER_H_
#define AAC_STREAM_READER_H_

#include "aac.h"
#include "simplePacketizerReader.h"

class AACStreamReader final : public SimplePacketizerReader, public AACCodec
{
   public:
    AACStreamReader() = default;
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) override;
    int getFreq() override { return m_sample_rate; }
    uint8_t getChannels() override { return m_channels; }

   protected:
    int getHeaderLen() override;
    int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes) override;
    uint8_t* findFrame(uint8_t* buff, uint8_t* end) override { return findAacFrame(buff, end); }
    double getFrameDuration() override { return static_cast<double>(m_samples) * INTERNAL_PTS_FREQ / m_sample_rate; }
    const CodecInfo& getCodecInfo() override { return aacCodecInfo; }
    const std::string getStreamInfo() override;
};

#endif
