#ifndef LPCM_STREAM_READER_H_
#define LPCM_STREAM_READER_H_

#include "avPacket.h"
#include "simplePacketizerReader.h"

class LPCMStreamReader final : public SimplePacketizerReader
{
   public:
    enum class LPCMHeaderType
    {
        htNone,
        htM2TS,
        htWAVE,
        htWAVE64
    };

    LPCMStreamReader() : m_useNewStyleAudioPES(false), m_tmpFrameBuffer()
    {
        m_bitsPerSample = m_freq = m_channels = 0;
        m_lfeExists = false;
        m_headerType = LPCMHeaderType::htNone;
        m_testMode = false;
        m_firstFrame = true;
        m_demuxMode = false;
        // m_waveHeaderParsed = false;
        m_curChunkLen = 0;
        m_channels = 0;
        m_frameRest = 0;
        m_needPCMHdr = true;
        m_openSizeWaveFormat = false;  // WAVE data size unknown and zero
        m_lastChannelRemapPos = nullptr;
    }
    void setNewStyleAudioPES(const bool value) { m_useNewStyleAudioPES = value; }
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) override;
    int getFreq() override { return m_freq; }
    uint8_t getChannels() override { return m_channels; }
    // void setDemuxMode(bool value) {m_demuxMode = value;}
    void setFirstFrame(const bool value) { m_firstFrame = value; }
    bool beforeFileCloseEvent(File& file) override;
    void setHeadersType(LPCMHeaderType value);

   protected:
    int getHeaderLen() override { return 4; }
    int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes) override;
    uint8_t* findFrame(uint8_t* buff, uint8_t* end) override;
    double getFrameDuration() override;
    const CodecInfo& getCodecInfo() override;
    const std::string getStreamInfo() override;
    void writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket) override;
    void setTestMode(const bool value) override { m_testMode = value; }
    int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                          PriorityDataInfo* priorityData) override;
    int readPacket(AVPacket& avPacket) override;
    int flushPacket(AVPacket& avPacket) override;
    void onSplitEvent() override { m_firstFrame = true; }
    void setBuffer(uint8_t* data, uint32_t dataLen, bool lastBlock = false) override;

   private:
    LPCMHeaderType m_headerType;
    bool m_useNewStyleAudioPES;
    bool m_testMode;
    bool m_firstFrame;
    // bool m_demuxMode;
    // bool m_waveHeaderParsed;
    uint8_t m_tmpFrameBuffer[17280 + 4];  // max PCM frame size
    int64_t m_frameRest;
    bool m_needPCMHdr;

    uint16_t m_bitsPerSample;
    int32_t m_freq;
    uint8_t m_channels;
    bool m_lfeExists;
    int64_t m_curChunkLen;
    bool m_openSizeWaveFormat;
    uint8_t* m_lastChannelRemapPos;

    bool detectLPCMType(uint8_t* buffer, int64_t len);
    int decodeLPCMHeader(const uint8_t* buff);
    int convertLPCMToWAV(uint8_t* start, uint8_t* end);
    int convertWavToPCM(uint8_t* start, uint8_t* end);

    void storeChannelData(const uint8_t* start, const uint8_t* end, int chNum, uint8_t* tmpData, int mch) const;
    void copyChannelData(uint8_t* start, const uint8_t* end, int chFrom, int chTo, int mch) const;
    void restoreChannelData(uint8_t* start, const uint8_t* end, int chNum, const uint8_t* tmpData, int mch) const;
    void removeChannel(uint8_t* start, const uint8_t* end, int cnNum, int mch) const;
    int decodeWaveHeader(uint8_t* buff, uint8_t* end);
    static uint8_t* findSubstr(const char* pattern, uint8_t* buff, const uint8_t* end);
};

#endif
