#ifndef __SRT_STREAM_READER
#define __SRT_STREAM_READER

#include <queue>

#include "abstractStreamReader.h"
#include "avCodecs.h"
#include "avPacket.h"
#include "psgStreamReader.h"
#include "textSubtitles.h"
#include "utf8Converter.h"

class SRTStreamReader : public AbstractStreamReader
{
   public:
    SRTStreamReader();
    ~SRTStreamReader() override;
    int readPacket(AVPacket& avPacket) override;
    int flushPacket(AVPacket& avPacket) override { return m_dstSubCodec->flushPacket(avPacket); }
    void setBuffer(uint8_t* data, int dataLen, bool lastBlock = false) override;
    uint64_t getProcessedSize() override { return m_processedSize; }
    CheckStreamRez checkStream(uint8_t* buffer, int len, ContainerType containerType, int containerDataType,
                               int containerStreamIndex);
    const CodecInfo& getCodecInfo() override { return pgsCodecInfo; }
    void setStreamIndex(const int index) override
    {
        m_streamIndex = index;
        m_dstSubCodec->setStreamIndex(index);
    }
    void setDemuxMode(const bool value) override
    {
        m_demuxMode = value;
        const auto pgsReader = dynamic_cast<PGSStreamReader*>(m_dstSubCodec);
        if (pgsReader)
            pgsReader->setDemuxMode(value);
    }
    void setVideoInfo(const int width, const int height, const double fps) const
    {
        m_srtRender->setVideoInfo(width, height, fps);
        const auto pgsReader = dynamic_cast<PGSStreamReader*>(m_dstSubCodec);
        if (pgsReader)
            pgsReader->setVideoInfo(0, 0, fps);
    }
    void setFont(const text_subtitles::Font& font) const { m_srtRender->m_textRender->setFont(font); }
    void setAnimation(const text_subtitles::TextAnimation& animation);
    void setBottomOffset(const int offset) const { m_srtRender->setBottomOffset(offset); }

   protected:
    int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                          PriorityDataInfo* priorityData) override
    {
        return m_dstSubCodec->writeAdditionData(dstBuffer, dstEnd, avPacket, priorityData);
    }

   private:
    UtfConverter::SourceFormat m_srcFormat;
    double m_inTime;
    double m_outTime;
    int m_processedSize;
    int32_t m_charSize;
    AbstractStreamReader* m_dstSubCodec;
    text_subtitles::TextToPGSConverter* m_srtRender;
    bool m_lastBlock;
    int parseText(uint8_t* dataStart, int len);
    std::vector<uint8_t> m_tmpBuffer;
    std::queue<std::string> m_sourceText;
    std::queue<uint32_t> m_origSize;
    std::string m_renderedText;
    long m_splitterOfs;
    uint16_t m_short_R;
    uint16_t m_short_N;
    uint32_t m_long_R;
    uint32_t m_long_N;
    text_subtitles::TextAnimation m_animation;

    enum class ParseState
    {
        PARSE_FIRST_LINE,
        PARSE_TIME,
        PARSE_TEXT
    };
    ParseState m_state;
    uint8_t* renderNextMessage(uint32_t& renderedLen);
    bool parseTime(const std::string& text);
    static std::string detectUTF8Lang(uint8_t* buffer, int len);
    bool detectSrcFormat(uint8_t* dataStart, int len, int& prefixLen);
    static bool strOnlySpace(std::string& str);
};

#endif
