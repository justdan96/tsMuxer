#ifndef __DVB_SUB_STREAM_READRE
#define __DVB_SUB_STREAM_READRE

#include "simplePacketizerReader.h"

#if 1

class DVBSubStreamReader : public SimplePacketizerReader
{
   public:
    DVBSubStreamReader() : SimplePacketizerReader(), m_big_offsets(0), m_frameDuration(0), m_firstFrame(true) {}
    int getTSDescriptor(uint8_t* dstBuff) override;

   protected:
    unsigned getHeaderLen() override { return 10; }
    uint8_t* findFrame(uint8_t* buff, uint8_t* end) override;
    int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes) override;
    double getFrameDurationNano() override;
    const CodecInfo& getCodecInfo() override { return dvbSubCodecInfo; }
    const std::string getStreamInfo() override;
    void setStreamType(int streamType) {}
    int getChannels() override { return 6; }  // fake. need refactor this class
    int getFreq() override { return 48000; }  // fake. need refactor this class
   private:
    bool m_firstFrame;
    int m_big_offsets;
    int m_offset_size;
    int64_t m_start_display_time;
    int64_t m_end_display_time;
    int64_t m_frameDuration;
    int intDecodeFrame(uint8_t* buff, uint8_t* end);
};

#endif

#endif