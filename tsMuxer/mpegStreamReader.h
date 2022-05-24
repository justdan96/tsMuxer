#ifndef __MPEG_STREAM_READER_H
#define __MPEG_STREAM_READER_H

#include <algorithm>

#include "abstractStreamReader.h"
#include "limits.h"
#include "vod_common.h"

const static int TMP_BUFFER_SIZE = 1024 * 1024 * 8;

class MPEGStreamReader : public AbstractStreamReader
{
   public:
    MPEGStreamReader() : AbstractStreamReader()
    {
        m_tmpBufferLen = 0;
        setFPS(0);
        m_eof = false;
        m_lastDecodeOffset = LONG_MAX;
        m_tmpBuffer = new uint8_t[TMP_BUFFER_SIZE];
        m_lastDecodedPos = 0;
        m_curPts = m_curDts = PTS_CONST_OFFSET;
        m_processedBytes = 0;
        m_totalFrameNum = 0;
        m_syncToStream = false;
        m_isFirstFpsWarn = true;
        m_shortStartCodes = true;
        m_pcrIncPerFrame = m_pcrIncPerField = 0;
        m_longCodesAllowed = true;
        m_removePulldown = false;
        // m_firstPulldownWarn = true;
        m_pulldownWarnCnt = 1;
        m_testPulldownDts = 0;
        m_streamAR = m_ar = VideoAspectRatio::AR_KEEP_DEFAULT;
        m_spsPpsFound = false;
    }
    ~MPEGStreamReader() override { delete[] m_tmpBuffer; }
    void setFPS(double fps)
    {
        m_fps = fps;
        if (fps > 0)
            m_pcrIncPerFrame = (int64_t)((double)INTERNAL_PTS_FREQ / fps);
        else
            m_pcrIncPerFrame = 0;
        m_pcrIncPerField = m_pcrIncPerFrame / 2;
    }
    double getFPS() const { return m_fps; }
    VideoAspectRatio getStreamAR() { return m_streamAR; }
    void setAspectRatio(VideoAspectRatio ar) { m_ar = ar; }
    uint64_t getProcessedSize() override;
    void setBuffer(uint8_t* data, int dataLen, bool lastBlock = false) override;
    int readPacket(AVPacket& avPacket) override;
    int flushPacket(AVPacket& avPacket) override;
    virtual int getStreamWidth() const = 0;
    virtual int getStreamHeight() const = 0;
    virtual bool getInterlaced() = 0;
    void setRemovePulldown(bool value) { m_removePulldown = value; }
    virtual int getFrameDepth() { return 1; }
    virtual void onShiftBuffer(int offset);

   protected:
    VideoAspectRatio m_ar;
    VideoAspectRatio m_streamAR;
    bool m_shortStartCodes;
    int64_t m_curPts;
    int64_t m_curDts;
    uint64_t m_processedBytes;
    bool m_eof;
    double m_fps;
    int64_t m_pcrIncPerFrame;
    int64_t m_pcrIncPerField;
    const uint8_t* m_lastDecodedPos;
    int m_totalFrameNum;
    virtual double getStreamFPS(void* curNalUnit) = 0;
    void updateFPS(void* curNALUnit, uint8_t* buff, uint8_t* nextNal, int oldSPSLen);
    virtual void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen) = 0;
    virtual void updateStreamAR(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen){};
    void fillAspectBySAR(double sar);
    virtual bool isIFrame() = 0;

    virtual bool skipNal(uint8_t* nal) { return false; }

   protected:
    // virtual CodecInfo getCodecInfo() = 0;
    virtual int intDecodeNAL(uint8_t* buff) = 0;
    // static int bufFromNAL(const uint8_t* buff, const uint8_t* bufEnd, bool longCodesAllowed);
    bool m_longCodesAllowed;
    bool m_removePulldown;
    int64_t m_testPulldownDts;
    void checkPulldownSync();
    uint8_t* m_tmpBuffer;
    bool m_spsPpsFound;
    // std::vector<uint8_t*> m_skippedNal;
   private:
    // bool m_firstPulldownWarn;
    int64_t m_pulldownWarnCnt;
    long m_lastDecodeOffset;
    bool m_syncToStream;
    bool m_isFirstFpsWarn;
    int bufFromNAL();
    virtual int decodeNal(uint8_t* buff);
    void storeBufferRest();
};

#endif
