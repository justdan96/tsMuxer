#ifndef SIMPLE_PACKETIZER_READER_H_
#define SIMPLE_PACKETIZER_READER_H_

#include "abstractStreamReader.h"
#include "avCodecs.h"
#include "avPacket.h"
#include "tsPacket.h"

class SimplePacketizerReader : public AbstractStreamReader
{
   public:
    // static const int NOT_ENOUGH_BUFFER = -10;
    SimplePacketizerReader();
    ~SimplePacketizerReader() override
    {
        // delete [] m_tmpBuffer;
    }
    int readPacket(AVPacket& avPacket) override;
    int flushPacket(AVPacket& avPacket) override;
    void setBuffer(uint8_t* data, int dataLen, bool lastBlock = false) override;
    uint64_t getProcessedSize() override;
    virtual CheckStreamRez checkStream(uint8_t* buffer, int len, ContainerType containerType, int containerDataType,
                                       int containerStreamIndex);
    virtual int getFreq() = 0;
    virtual int getAltFreq() { return getFreq(); }
    virtual int getChannels() = 0;
    void setStretch(const double value) { m_stretch = value; }
    void setMPLSInfo(const std::vector<MPLSPlayItem>& mplsInfo)
    {
        m_mplsInfo = mplsInfo;
        if (m_mplsInfo.size() > 0)
        {
            m_curMplsIndex = 0;
            m_lastMplsTime = (m_mplsInfo[0].OUT_time - m_mplsInfo[0].IN_time) * (INTERNAL_PTS_FREQ / 45000.0);
        }
        else
            m_curMplsIndex = -1;
    }

    // split point can be on any frame
    virtual bool isIFrame(AVPacket* packet) { return true; }

   protected:
    virtual int getHeaderLen() = 0;  // return fixed frame header size at bytes
    virtual int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes,
                            int& skipBeforeBytes) = 0;  // decode frame parameters. bitrate, channels for audio e.t.c
    virtual uint8_t* findFrame(uint8_t* buff, uint8_t* end) = 0;  // find forawrd nearest frame
    virtual double getFrameDuration() = 0;                        // frame duration at nano seconds
    virtual const std::string getStreamInfo() = 0;
    virtual void setTestMode(bool value) {}
    // virtual bool isSubFrame() {return false;} // used for DTS-HD, Dolby-TRUEHD. returns true for MLP data. Data can't
    // be splitted in point where subFrame=true
    virtual bool needMPLSCorrection() const { return true; }
    virtual bool needSkipFrame(const AVPacket& packet) { return false; }

   protected:
    // uint8_t* m_tmpBuffer;
    unsigned m_curMplsIndex;
    double m_stretch;
    std::vector<uint8_t> m_tmpBuffer;
    uint64_t m_processedBytes;
    uint64_t m_frameNum;
    bool m_needSync;
    double m_curPts;
    double m_lastMplsTime;
    double m_mplsOffset;
    double m_halfFrameLen;

    int m_containerDataType;
    int m_containerStreamIndex;
    std::vector<MPLSPlayItem> m_mplsInfo;
    void doMplsCorrection();
};

#endif
