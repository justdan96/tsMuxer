#ifndef MLP_STREAM_READER_H_
#define MLP_STREAM_READER_H_

#include "avPacket.h"
#include "mlpCodec.h"
#include "simplePacketizerReader.h"

class MLPStreamReader final : public SimplePacketizerReader, public MLPCodec
{
   public:
    MLPStreamReader()
    {
        m_demuxedTHDSamples = 0;
        m_totalTHDSamples = 0;
    }
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) override;
    int getFreq() override { return m_samplerate; }
    uint8_t getChannels() override { return m_channels; }

   protected:
    int getHeaderLen() override;
    int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes) override;
    uint8_t* findFrame(uint8_t* buff, uint8_t* end) override { return MLPCodec::findFrame(buff, end); }
    double getFrameDuration() override { return static_cast<double>(MLPCodec::getFrameDuration()); }
    const CodecInfo& getCodecInfo() override { return mlpCodecInfo; }
    const std::string getStreamInfo() override;

    int readPacket(AVPacket& avPacket) override;
    int flushPacket(AVPacket& avPacket) override;

   private:
    int m_demuxedTHDSamples;
    uint64_t m_totalTHDSamples;
};

#endif
