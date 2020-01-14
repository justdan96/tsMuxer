#ifndef __MPEG2_STREAM_READER_H
#define __MPEG2_STREAM_READER_H

#include "avCodecs.h"
#include "mpegStreamReader.h"
#include "mpegVideo.h"

class MPEG2StreamReader : public MPEGStreamReader
{
   public:
    MPEG2StreamReader() : MPEGStreamReader(), m_sequence(0), m_frame(0)
    {
        m_isFirstFrame = true;
        spsFound = false;
        m_framesAtGop = 0;
        // m_lastFullFrame = true;
        m_lastRef = -1;
        m_seqExtFound = false;
        m_streamMsgPrinted = false;
        m_lastIFrame = false;
        m_longCodesAllowed = false;
        m_prevFrameDelay = 0;
    }
    int getTSDescriptor(uint8_t* dstBuff) override;
    virtual CheckStreamRez checkStream(uint8_t* buffer, int len);

    int getStreamWidth() const override { return m_sequence.width; }
    int getStreamHeight() const override { return m_sequence.height; }
    int getStreamHDR() const override { return 0; }
    bool getInterlaced() override { return !m_sequence.progressive_sequence; }

   protected:
    const CodecInfo& getCodecInfo() override { return mpeg2CodecInfo; };
    int intDecodeNAL(uint8_t* buff) override;
    void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen) override;
    void updateStreamAR(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen) override;
    double getStreamFPS(void* curNalUnit) override { return m_sequence.getFrameRate(); };
    bool isIFrame() override { return m_lastIFrame; }

   private:
    bool m_streamMsgPrinted;
    int m_lastRef;
    // bool m_lastFullFrame;
    int m_framesAtGop;
    bool m_isFirstFrame;
    bool spsFound;
    bool m_seqExtFound;
    bool m_lastIFrame;
    int64_t m_prevFrameDelay;
    MPEGSequenceHeader m_sequence;
    MPEGPictureHeader m_frame;
    int getNextBFrames(uint8_t* buffer);
    int findFrameExt(uint8_t* buffer);
    int decodePicture(uint8_t* buff);
    int processExtStartCode(uint8_t* buff);
    int processSeqStartCode(uint8_t* buff);
    uint8_t* m_nextFrameAddr;
};

#endif
