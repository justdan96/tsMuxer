#ifndef __VC1_STREAM_READER_H
#define __VC1_STREAM_READER_H

#include "avCodecs.h"
#include "mpegStreamReader.h"
#include "vc1Parser.h"

class VC1StreamReader : public MPEGStreamReader
{
   public:
    VC1StreamReader() : MPEGStreamReader()
    {
        m_isFirstFrame = true;
        m_spsFound = 0;
        m_frame.pict_type = VC1PictType::I_TYPE;
        m_lastIFrame = false;
        m_decodedAfterSeq = false;
        m_firstFileFrame = false;
        m_prevDtsInc = false;
        m_longCodesAllowed = false;
        m_nextFrameAddr = 0;
    }
    ~VC1StreamReader() override {}
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) override;
    virtual CheckStreamRez checkStream(uint8_t* buffer, int len);
    bool skipNal(uint8_t* nal) override;
    bool needSPSForSplit() const override { return true; }

   protected:
    const CodecInfo& getCodecInfo() override { return vc1CodecInfo; };
    int intDecodeNAL(uint8_t* buff) override;
    void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen) override;
    void writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket) override;
    int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                          PriorityDataInfo* priorityData) override;
    double getStreamFPS(void* curNalUnit) override { return m_sequence.getFPS(); };
    int getStreamWidth() const override { return m_sequence.coded_width; }
    int getStreamHeight() const override { return m_sequence.coded_height; }
    int getStreamHDR() const override { return 0; }
    bool getInterlaced() override { return m_sequence.interlace; }
    bool isIFrame() override { return m_lastIFrame; }
    void onSplitEvent() override { m_firstFileFrame = true; }

   private:
    int decodeFrame(uint8_t* buff);
    int decodeEntryPoint(uint8_t* buff);
    int decodeSeqHeader(uint8_t* buff);

   private:
    int64_t m_prevDtsInc;
    bool m_lastIFrame;
    std::vector<uint8_t> m_entryPointBuffer;
    std::vector<uint8_t> m_seqBuffer;
    bool m_isFirstFrame;
    int m_spsFound;
    VC1SequenceHeader m_sequence;
    VC1Frame m_frame;
    int getNextBFrames(uint8_t* buffer, int64_t& bTiming);
    uint8_t* findNextFrame(uint8_t* buffer);
    bool m_firstFileFrame;
    bool m_decodedAfterSeq;
    uint8_t* m_nextFrameAddr;
};

#endif
