#ifndef __AC3_STREAM_READER_H
#define __AC3_STREAM_READER_H

#include <deque>

#include "abstractDemuxer.h"
#include "ac3Codec.h"
#include "avPacket.h"
#include "simplePacketizerReader.h"

class AC3StreamReader : public SimplePacketizerReader, public AC3Codec
{
   public:
    AC3StreamReader() : SimplePacketizerReader(), m_useNewStyleAudioPES(false)
    {
        m_downconvertToAC3 = m_true_hd_mode = false;
        m_state = AC3State::stateDecodeAC3;
        m_waitMoreData = false;

        m_thdDemuxWaitAc3 = true;  // wait ac3 packet
        m_demuxedTHDSamples = 0;
        m_totalTHDSamples = 0;
        m_nextAc3Time = 0;
    };
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) override;
    void setNewStyleAudioPES(bool value) { m_useNewStyleAudioPES = value; }
    void setTestMode(bool value) override { AC3Codec::setTestMode(value); }
    int getFreq() override { return AC3Codec::m_sample_rate; }
    int getAltFreq() override
    {
        if (m_downconvertToAC3)
            return AC3Codec::m_sample_rate;
        else
            return mh.subType == MLPHeaderInfo::MlpSubType::stUnknown ? AC3Codec::m_sample_rate : mh.group1_samplerate;
    }
    int getChannels() override { return AC3Codec::m_channels; }
    bool isPriorityData(AVPacket* packet) override;
    bool isIFrame(AVPacket* packet) override { return isPriorityData(packet); }
    bool isSecondary() override;

   protected:
    int getHeaderLen() override { return AC3Codec::getHeaderLen(); }
    int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes) override
    {
        skipBeforeBytes = 0;
        return AC3Codec::decodeFrame(buff, end, skipBytes);
    }
    uint8_t* findFrame(uint8_t* buff, uint8_t* end) override { return AC3Codec::findFrame(buff, end); }
    double getFrameDurationNano() override { return (double)AC3Codec::getFrameDurationNano(); }
    const CodecInfo& getCodecInfo() override { return AC3Codec::getCodecInfo(); }
    const std::string getStreamInfo() override { return AC3Codec::getStreamInfo(); }
    void writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket) override;

    int readPacket(AVPacket& avPacket) override;
    int flushPacket(AVPacket& avPacket) override;
    int readPacketTHD(AVPacket& avPacket);

    bool needMPLSCorrection() const override;

   private:
    bool m_useNewStyleAudioPES;

    bool m_thdDemuxWaitAc3;

    MemoryBlock m_delayedAc3Buffer;
    AVPacket m_delayedAc3Packet;
    int m_demuxedTHDSamples;
    uint64_t m_totalTHDSamples;
    uint64_t m_nextAc3Time;
};

#endif
