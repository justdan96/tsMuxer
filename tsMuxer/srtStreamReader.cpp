
#include "srtStreamReader.h"

#include <string>

#include "convertUTF.h"
#include "matroskaParser.h"
#include "memory.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;

using namespace text_subtitles;

SRTStreamReader::SRTStreamReader() : m_lastBlock(false)
{
    // in future version here must be case for destination subtitle format (DVB sub, DVD sub e.t.c)
    m_dstSubCodec = new PGSStreamReader();
    m_srtRender = new TextToPGSConverter(true);
    m_processedSize = 0;
    m_state = PARSE_FIRST_LINE;
    m_inTime = m_outTime = 0.0;
    m_srcFormat = UtfConverter::sfUnknown;
    m_charSize = 1;
    m_splitterOfs = 0;
}

SRTStreamReader::~SRTStreamReader()
{
    delete m_dstSubCodec;
    delete m_srtRender;
}

void SRTStreamReader::setAnimation(const text_subtitles::TextAnimation& animation) { m_animation = animation; }

void SRTStreamReader::setBuffer(uint8_t* data, int dataLen, bool lastBlock)
{
    m_lastBlock = lastBlock;
    uint8_t* dataBegin = data + MAX_AV_PACKET_SIZE - m_tmpBuffer.size();
    if (m_tmpBuffer.size() > 0)
        memcpy(dataBegin, &m_tmpBuffer[0], m_tmpBuffer.size());
    int parsedLen = parseText(dataBegin, dataLen + m_tmpBuffer.size());
    int rest = dataLen + m_tmpBuffer.size() - parsedLen;
    if (rest > MAX_AV_PACKET_SIZE)
        THROW(ERR_COMMON, "Invalid SRT file or too large text message (>" << MAX_AV_PACKET_SIZE << " bytes)");
    m_tmpBuffer.resize(rest);
    if (rest > 0)
        memcpy(&m_tmpBuffer[0], dataBegin + dataLen + m_tmpBuffer.size() - rest, rest);
}

bool SRTStreamReader::detectSrcFormat(uint8_t* dataStart, int len, int& prefixLen)
{
    prefixLen = 0;
    if (len < 4)
        return false;
    // detect UTF-8/UTF-16/UTF-32 format
    if ((dataStart[0] == 0xEF && dataStart[1] == 0xBB && dataStart[2] == 0xBF) ||
        convertUTF::isLegalUTF8String(dataStart, len))
    {
        m_charSize = 1;
        m_srcFormat = UtfConverter::sfUTF8;
        prefixLen = 3;
    }
    else if (dataStart[0] == 0 && dataStart[1] == 0 && dataStart[2] == 0xFE && dataStart[3] == 0xFF)
    {
        m_srcFormat = UtfConverter::sfUTF32be;
        m_charSize = 4;
        prefixLen = 4;
        m_splitterOfs = 3;
        m_long_N = my_htonl(0x0000000a);
        m_long_R = my_htonl(0x0000000d);
    }
    else if (dataStart[0] == 0xFF && dataStart[1] == 0xFE && dataStart[2] == 0 && dataStart[3] == 0)
    {
        m_srcFormat = UtfConverter::sfUTF32le;
        m_charSize = 4;
        prefixLen = 4;
        m_long_N = my_htonl(0x0a000000);
        m_long_R = my_htonl(0x0d000000);
    }
    else if (dataStart[0] == 0xFE && dataStart[1] == 0xFF)
    {
        m_srcFormat = UtfConverter::sfUTF16be;
        m_charSize = 2;
        prefixLen = 2;
        m_splitterOfs = 1;
        m_short_N = my_htons(0x000a);
        m_short_R = my_htons(0x000d);
    }
    else if (dataStart[0] == 0xFF && dataStart[1] == 0xFE)
    {
        m_srcFormat = UtfConverter::sfUTF16le;
        m_charSize = 2;
        prefixLen = 2;
        m_short_N = my_htons(0x0a00);
        m_short_R = my_htons(0x0d00);
    }
    else
    {
#ifdef _WIN32
        LTRACE(LT_INFO, 2, "Failed to auto-detect SRT encoding : falling back to the active code page");
        m_srcFormat = UtfConverter::sfANSI;  // default value for win32
#else
        LTRACE(LT_INFO, 2, "Failed to auto-detect SRT encoding : falling back to UTF-8");
        m_srcFormat = UtfConverter::sfUTF8;
#endif
    }
    return true;
}

int SRTStreamReader::parseText(uint8_t* dataStart, int len)
{
    int prefixLen = 0;
    if (m_srcFormat == UtfConverter::sfUnknown)
    {
        if (!detectSrcFormat(dataStart, len, prefixLen))
            return false;
    }
    uint8_t* cur = dataStart + prefixLen;
    int roundLen = len & (~(m_charSize - 1));
    uint8_t* end = cur + roundLen;
    uint8_t* lastProcessedLine = cur;
    vector<string> rez;
    for (; cur < end; cur += m_charSize)
    {
        // if (cur[m_splitterOfs] == '\n')
        if (m_charSize == 1 && *cur == '\n' || m_charSize == 2 && *((uint16_t*)cur) == m_short_N ||
            m_charSize == 4 && *((uint32_t*)cur) == m_long_N)
        {
            long x = 0;
            if (cur - lastProcessedLine >= m_charSize)
                if (m_charSize == 1 && cur[-1] == '\r' || m_charSize == 2 && ((uint16_t*)cur)[-1] == m_short_R ||
                    m_charSize == 4 && ((uint32_t*)cur)[-1] == m_long_R)
                    x = m_charSize;

            m_sourceText.emplace(UtfConverter::toUtf8(lastProcessedLine, cur - lastProcessedLine - x, m_srcFormat));
            std::string& tmp = m_sourceText.back();
            if (strOnlySpace(tmp))
                tmp.clear();

            m_origSize.push(cur + m_charSize - lastProcessedLine + prefixLen);
            prefixLen = 0;
            lastProcessedLine = cur + m_charSize;
        }
    }
    return lastProcessedLine - dataStart;
}

bool SRTStreamReader::strOnlySpace(std::string& str)
{
    for (std::string::iterator itr = str.begin(); itr != str.end(); ++itr)
        if (*itr != ' ')
            return false;
    return true;
}

int SRTStreamReader::readPacket(AVPacket& avPacket)
{
    int rez = m_dstSubCodec->readPacket(avPacket);
    if (rez == NEED_MORE_DATA)
    {
        uint32_t renderedLen;
        uint8_t* renderedBuffer = renderNextMessage(renderedLen);
        if (renderedBuffer)
        {
            m_dstSubCodec->setBuffer(renderedBuffer - MAX_AV_PACKET_SIZE, renderedLen,
                                     m_lastBlock && m_sourceText.size() == 0);
            return m_dstSubCodec->readPacket(avPacket);
        }
        else
            return NEED_MORE_DATA;
    }
    return rez;
}

uint8_t* SRTStreamReader::renderNextMessage(uint32_t& renderedLen)
{
    uint8_t* rez = 0;
    if (m_sourceText.size() == 0)
        return 0;
    if (m_state == PARSE_FIRST_LINE)
    {
        while (m_sourceText.size() > 0 && m_sourceText.front().size() == 0)
        {
            m_sourceText.pop();  // delete empty lines before message
            m_processedSize += m_origSize.front();
            m_origSize.pop();
        }
        if (m_sourceText.size() == 0)
            return 0;
        m_state = PARSE_TIME;
        bool isNUmber = true;
        {
            for (auto c : m_sourceText.front())
                if (!(c >= '0' && c <= '9') && c != ' ')
                {
                    isNUmber = false;
                    break;
                }
        }
        if (isNUmber)
        {
            m_sourceText.pop();
            m_processedSize += m_origSize.front();
            m_origSize.pop();
            if (m_sourceText.size() == 0)
                return 0;
        }
    }
    if (m_state == PARSE_TIME)
    {
        if (!parseTime(m_sourceText.front()))
            THROW(ERR_COMMON, "Invalid SRT format. \"" << m_sourceText.front().c_str() << "\" is invalid timing info");
        m_state = PARSE_TEXT;
        m_sourceText.pop();
        m_processedSize += m_origSize.front();
        m_origSize.pop();
        if (m_sourceText.size() == 0)
            return 0;
    }

    while (m_sourceText.size() > 0 && m_sourceText.front().size() > 0)
    {
        if (m_renderedText.size() > 0)
            m_renderedText += '\n';
        m_renderedText += m_sourceText.front();
        m_sourceText.pop();
        m_processedSize += m_origSize.front();
        m_origSize.pop();
    }

    if (m_sourceText.size() == 0)
    {
        if (m_lastBlock && m_renderedText.size() > 0)
        {
            m_state = PARSE_FIRST_LINE;
            m_renderedText.clear();
            rez = m_srtRender->doConvert(m_renderedText, m_animation, m_inTime, m_outTime, renderedLen);
        }
        return rez;
    }
    else
    {
        m_sourceText.pop();  // delete empty line (messages separator)
        m_processedSize += m_origSize.front();
        m_origSize.pop();
        rez = m_srtRender->doConvert(m_renderedText, m_animation, m_inTime, m_outTime, renderedLen);
        m_state = PARSE_FIRST_LINE;
        m_renderedText.clear();
    }
    return rez;
}

bool SRTStreamReader::parseTime(const string& text)
{
    for (int i = 0; i < text.length() - 2; i++)
    {
        if (text[i] == '-' && text[i + 1] == '-' && text[i + 2] == '>')
        {
            string first = trimStr(text.substr(0, i));
            string second = trimStr(text.substr(i + 3, text.length() - i - 3));
            for (int j = 0; j < first.length(); j++)
                if (first[j] == ',')
                    first[j] = '.';
            for (int j = 0; j < second.length(); j++)
                if (second[j] == ',')
                    second[j] = '.';
            m_inTime = timeToFloat(first);
            m_outTime = timeToFloat(second);
            return true;
        }
    }
    return false;
}

CheckStreamRez SRTStreamReader::checkStream(uint8_t* buffer, int len, ContainerType containerType,
                                            int containerDataType, int containerStreamIndex)
{
    CheckStreamRez rez;
    if ((containerType == ctMKV || containerType == ctMOV) && containerDataType == TRACKTYPE_SRT ||
        containerType == ctSRT)
    {
        rez.codecInfo = srtCodecInfo;
        rez.streamDescr = "SRT text subtitles";
        if (containerType == ctSRT)
            rez.lang = detectUTF8Lang(buffer, len);
    }
    return rez;
}

std::string SRTStreamReader::detectUTF8Lang(uint8_t* buffer, int len) { return ""; }
