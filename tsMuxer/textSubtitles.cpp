#include "textSubtitles.h"

#ifdef _WIN32
// #include <winerror.h>

#include "osdep/textSubtitlesRenderWin32.h"
#ifdef WIN32_DEBUG_FREETYPE
#include "osdep/textSubtitlesRenderFT.h"
#endif
#else
#include "osdep/textSubtitlesRenderFT.h"
#endif

#include <cassert>
#include <cmath>

#include "psgStreamReader.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;
using namespace text_subtitles;

static constexpr float FLOAT_EPS = 1e-6f;
static constexpr int PG_BUFFER_SIZE = 1024 * 1024 * 2;

// const static int BORDER_WIDTH = 4;
// const RGBQUAD RESERVED_BACKGROUND_COLOR = {0x00, 0x00, 0x00, 0x00};

///////////////////////////////////////////
TextToPGSConverter::TextToPGSConverter(const bool sourceIsText)
    : /* TextSubtitlesRenderWin32(), */
      m_rleLen(0),
      m_composition_number(0),
      m_videoWidth(0),
      m_videoHeight(0),
      m_videoFps(0),
      m_imageBuffer()
{
    m_bottomOffset = 0;
    m_renderedData = nullptr;
    m_textRender = nullptr;
    palette_update_flag = false;
    m_paletteID = 0;
    m_paletteVersion = 0;
    m_pgsBuffer = new uint8_t[PG_BUFFER_SIZE];
    m_minLine = 0;
    m_maxLine = 0;
    if (sourceIsText)
    {
#ifdef _WIN32
#ifdef WIN32_DEBUG_FREETYPE
        m_textRender = new TextSubtitlesRenderFT;
#else
        m_textRender = new TextSubtitlesRenderWin32;
#endif
#else
        m_textRender = new TextSubtitlesRenderFT;
#endif
    }
}

TextToPGSConverter::~TextToPGSConverter()
{
    delete[] m_pgsBuffer;
    delete[] m_renderedData;
    delete m_textRender;
}

void TextToPGSConverter::enlargeCrop(const uint16_t width, const uint16_t height, uint16_t* newWidth,
                                     uint16_t* newHeight) const
{
    *newWidth = width;
    *newHeight = height;
    if (width > 1280)
    {
        *newWidth = 1920;
        *newHeight = 1080;
    }
    else if (width > 720)
    {
        *newWidth = 1280;
        *newHeight = 720;
    }
    else if (fabs(m_videoFps - 25.0) < 0.1 || fabs(m_videoFps - 50.0) < 0.1)
    {
        *newWidth = 720;
        *newHeight = 576;
    }
    else
    {
        *newWidth = 720;
        *newHeight = 480;
    }
}

void TextToPGSConverter::setVideoInfo(const uint16_t width, const uint16_t height, const double fps)
{
    enlargeCrop(width, height, &m_videoWidth, &m_videoHeight);
    m_videoFps = fps;
    if (m_textRender)
        m_textRender->setRenderSize(m_videoWidth, m_videoHeight);
    delete[] m_renderedData;
    const int len = (m_videoWidth + 16) * m_videoHeight;
    m_renderedData = new uint8_t[len];
}

double TextToPGSConverter::alignToGrid(const double value) const
{
    // how many frames have passed until this moment in time (rounded)
    const double frameCnt = round(value * m_videoFps);
    return frameCnt / m_videoFps;
}

uint8_t TextToPGSConverter::color32To8(const uint32_t* buff, const uint32_t colorMask)
{
    YUVQuad yuv = RGBAToYUVA(*buff | colorMask);
    const auto itr = m_paletteYUV.find(yuv);
    if (itr == m_paletteYUV.end())
    {
        if (m_paletteYUV.size() < 255)
        {
            const pair<map<YUVQuad, uint8_t>::iterator, bool> rez =
                m_paletteYUV.insert(std::make_pair(yuv, static_cast<uint8_t>(m_paletteYUV.size())));
            return rez.first->second;
        }
        // find nearest color
        // if (m_paletteYUV.size() >= 256)
        THROW(ERR_COMMON, "Can't transform image to YUV: too many colors are used.")
        // return m_paletteYUV[yuv];
    }
    return itr->second;
}

int TextToPGSConverter::getRepeatCnt(const uint32_t* pos, const uint32_t* end, const uint32_t colorMask)
{
    int rez = 1;
    if (*pos == 0)
    {
        for (const uint32_t* cur = pos + 1; cur < end; cur++)
        {
            if (*cur == *pos)
                rez++;
            else
                break;
        }
    }
    else
    {
        const uint32_t rgbColor = *pos | colorMask;
        const YUVQuad color = RGBAToYUVA(rgbColor);
        for (const uint32_t* cur = pos + 1; cur < end && *cur != 0; cur++)
        {
            const uint32_t newRGBColor = *cur | colorMask;
            if (newRGBColor == rgbColor || RGBAToYUVA(newRGBColor) == color)
                rez++;
            else
                break;
        }
    }
    return rez;
}

YUVQuad TextToPGSConverter::RGBAToYUVA(uint32_t data)
{
    const auto rgba = reinterpret_cast<RGBQUAD*>(&data);
    YUVQuad rez;
    rez.Y = static_cast<uint8_t>(
        lround(65.738 * rgba->rgbRed + 129.057 * rgba->rgbGreen + 25.064 * rgba->rgbBlue + 4224.0) >> 8);
    rez.Cr = static_cast<uint8_t>(
        lround(112.439 * rgba->rgbRed - 94.154 * rgba->rgbGreen - 18.285 * rgba->rgbBlue + 32896.0) >> 8);
    rez.Cb = static_cast<uint8_t>(
        lround(-37.945 * rgba->rgbRed - 74.494 * rgba->rgbGreen + 112.439 * rgba->rgbBlue + 32896.0) >> 8);
    rez.alpha = rgba->rgbReserved;
    return rez;
}

RGBQUAD TextToPGSConverter::YUVAToRGBA(const YUVQuad& yuv)
{
    RGBQUAD rez;
    int tmp = lround(298.082 * yuv.Y + 516.412 * yuv.Cb - 70742.016) >> 8;
    rez.rgbBlue = static_cast<uint8_t>(FFMAX(FFMIN(tmp, 255L), 0));
    tmp = lround(298.082 * yuv.Y - 208.120 * yuv.Cr - 100.291 * yuv.Cb + 34835.456) >> 8;
    rez.rgbGreen = static_cast<uint8_t>(FFMAX(FFMIN(tmp, 255L), 0));
    tmp = lround(298.082 * yuv.Y + 516.412 * yuv.Cr - 56939.776) >> 8;
    rez.rgbRed = static_cast<uint8_t>(FFMAX(FFMIN(tmp, 255L), 0));
    rez.rgbReserved = yuv.alpha;
    return rez;
}

void TextToPGSConverter::reduceColors(uint8_t mask) const
{
    mask = ~mask;
    const uint32_t val = (mask << 24) + (mask << 16) + (mask << 8) + mask;
    auto dst = reinterpret_cast<uint32_t*>(m_textRender ? m_textRender->m_pData : m_imageBuffer);
    const int len = m_videoWidth * m_videoHeight;
    const uint32_t* end = dst + len;
    for (; dst < end; ++dst) *dst &= val;
}

bool TextToPGSConverter::rlePack(const uint32_t colorMask)
{
    try
    {
        m_paletteYUV.clear();
        // compress render buffer by RLE

        uint8_t* curPtr = m_renderedData;
        const uint8_t* trimPos = m_renderedData;
        auto srcData = reinterpret_cast<uint32_t*>(m_textRender ? m_textRender->m_pData : m_imageBuffer);
        assert(srcData);
        m_rleLen = 0;
        m_minLine = INT_MAX;
        m_maxLine = 0;
        for (int y = 0; y < m_videoHeight; y++)
        {
            const uint32_t* srcLineEnd = srcData + m_videoWidth;
            const uint8_t* dstLineEnd = curPtr + m_videoWidth + 16;
            bool isEmptyLine = false;
            while (srcData < srcLineEnd)
            {
                const int repCnt = getRepeatCnt(srcData, srcLineEnd, colorMask);

                if (repCnt == m_videoWidth)
                {
                    isEmptyLine = true;
                    if (curPtr == m_renderedData)
                    {
                        srcData += repCnt;
                        continue;
                    }
                }
                else
                {
                    m_minLine = FFMIN(m_minLine, y);
                    m_maxLine = FFMAX(m_maxLine, y);
                }

                const uint8_t srcColor = color32To8(srcData, colorMask);
                assert(repCnt < 16384);
                if (srcColor)  // color exists
                {
                    if (repCnt > 2)
                    {
                        *curPtr++ = 0;
                        if (repCnt <= 63)
                            *curPtr++ = static_cast<uint8_t>(0x80 + repCnt);
                        else
                        {
                            *curPtr++ = static_cast<uint8_t>(0xc0 + (repCnt >> 8));
                            *curPtr++ = repCnt & 0xff;
                        }
                        *curPtr++ = srcColor;
                        srcData += repCnt;
                    }
                    else
                    {  // simple color
                        *curPtr++ = srcColor;
                        srcData++;
                        if (repCnt == 2)
                        {
                            *curPtr++ = srcColor;
                            srcData++;
                        }
                    }
                }
                else  // zero color
                {
                    *curPtr++ = 0;
                    if (repCnt <= 63)
                        *curPtr++ = repCnt & 0xff;
                    else
                    {
                        *curPtr++ = static_cast<uint8_t>(0x40 | repCnt >> 8);
                        *curPtr++ = repCnt & 0xff;
                    }
                    srcData += repCnt;
                }
            }
            if (m_minLine != INT_MAX)
            {
                *curPtr++ = 0;  // end of line signal
                *curPtr++ = 0;  // end of line signal
                if (curPtr >= dstLineEnd)
                {
                    THROW(ERR_COMMON, "Not enough RLE buffer for encoding picture (RLE line length > width + 16)")
                }
            }
            if (!isEmptyLine)
                trimPos = curPtr;
        }
        if (m_minLine == INT_MAX)
            m_minLine = m_maxLine = 0;
        m_rleLen = static_cast<int>(trimPos - m_renderedData);
        // sort by colors indexes
        m_paletteByColor.clear();
        for (auto [fst, snd] : m_paletteYUV) m_paletteByColor.insert(std::make_pair(snd, fst));
        assert(m_paletteByColor.size() == m_paletteYUV.size());
        return true;
    }
    catch (VodCoreException& e)
    {
        (void)e;
        return false;
    }
}

int TextToPGSConverter::renderedHeight() const { return m_maxLine - m_minLine + 1; }

int TextToPGSConverter::maxLine() const { return m_maxLine; }

int TextToPGSConverter::minLine() const { return m_minLine; }

TextToPGSConverter::Palette TextToPGSConverter::buildPalette(const float opacity)
{
    if (opacity == 1.0)
        return m_paletteByColor;
    Palette result = m_paletteByColor;
    for (auto itr = result.begin(); itr != result.end(); ++itr)
        itr->second.alpha = static_cast<uint8_t>(FFMIN(255L, lround(itr->second.alpha * opacity)));
    return result;
}

float toCurve(const float value)
{
    // float result = pow(value, 1.5f);
    const float result = value * sqrt(value);  // same as pow 1.5, reduce binary size
    return result;
}

uint8_t* TextToPGSConverter::doConvert(const std::string& text, const TextAnimation& animation, double inTimeSec,
                                       double outTimeSec, uint32_t& dstBufSize)
{
    const bool forced = m_textRender->rasterText(text);
    inTimeSec = alignToGrid(inTimeSec);
    outTimeSec = alignToGrid(outTimeSec);

    const double inTimePTS = inTimeSec * 90000.0;
    const double outTimePTS = outTimeSec * 90000.0;

    uint32_t mask = 0;
    int step = 0;
    while (!rlePack(mask))
    {
        // reduce colors
        const auto tmp = reinterpret_cast<uint8_t*>(&mask);
        const int idx = step++ % 4;
        tmp[idx] <<= 1;
        tmp[idx]++;
    }

    if (m_rleLen == 0)
        return nullptr;  // empty text

    const auto objectWindowHeight = static_cast<uint16_t>(FFMAX(0, renderedHeight()));
    const auto objectWindowTop = static_cast<uint16_t>(FFMAX(0, m_textRender->m_height - objectWindowHeight - m_bottomOffset));

    int fadeInFrames = lround(animation.fadeInDuration * m_videoFps);
    int fadeOutFrames = lround(animation.fadeOutDuration * m_videoFps);
    fadeInFrames++;
    fadeOutFrames++;

    const float opacityInDelta = 1.0F / static_cast<float>(fadeInFrames);
    const float opacityOutDelta = 1.0F / static_cast<float>(fadeOutFrames);
    float opacity = opacityInDelta;

    const double decodedObjectSize = (m_maxLine - m_minLine + 1) * m_videoWidth;
    const double compositionDecodeTime = 90000.0 * decodedObjectSize / PIXEL_DECODING_RATE + 0.999;
    const double windowsTransferTime = 90000.0 * decodedObjectSize / PIXEL_COMPOSITION_RATE + 0.999;

    const double PLANEINITIALIZATIONTIME = 90000.0 * (m_videoWidth * m_videoHeight) / PIXEL_COMPOSITION_RATE + 0.999;
    const double PRESENTATION_DTS_DELTA = PLANEINITIALIZATIONTIME + windowsTransferTime;

    // 1. show text
    uint8_t* curPos = m_pgsBuffer;
    palette_update_flag = false;
    m_paletteID = 0;
    m_paletteVersion = 0;
    int64_t pts = llround(inTimePTS);
    int64_t dts = llround(inTimePTS - PRESENTATION_DTS_DELTA);

    curPos += composePresentationSegment(curPos, CompositionMode::Start, pts, dts, objectWindowTop, true, forced);
    curPos += composeWindowDefinition(curPos, llround(inTimePTS - windowsTransferTime), dts, objectWindowTop,
                                      objectWindowHeight);
    curPos += composePaletteDefinition(buildPalette(toCurve(opacity)), curPos, dts, dts);
    const int64_t odfPTS = llround(inTimePTS - PRESENTATION_DTS_DELTA + compositionDecodeTime);
    curPos += composeObjectDefinition(curPos, odfPTS, dts, m_minLine, m_maxLine, true);
    curPos += composeEnd(curPos, odfPTS, odfPTS);

    // 2.1 fade in palette
    const double fpsPts = 90000.0 / m_videoFps;
    double updateTime = alignToGrid(inTimePTS + fpsPts);

    const double lastAnimateTime = alignToGrid(outTimePTS - fpsPts);
    opacity += opacityInDelta;
    while (updateTime <= lastAnimateTime + FLOAT_EPS && opacity <= 1.0 + FLOAT_EPS)
    {
        palette_update_flag = true;
        m_paletteVersion++;
        pts = llround(updateTime);
        dts = llround(updateTime - windowsTransferTime);
        curPos += composePresentationSegment(curPos, CompositionMode::Update, pts, dts, objectWindowTop, true, forced);
        curPos += composePaletteDefinition(buildPalette(toCurve(opacity)), curPos, dts, dts);
        curPos += composeEnd(curPos, pts - 90, pts - 90);

        updateTime = alignToGrid(updateTime + fpsPts);
        opacity += opacityInDelta;
    }

    // 2.2 fade out palette
    updateTime = alignToGrid(FFMAX(updateTime, outTimePTS - (fadeOutFrames - 1) * fpsPts));
    opacity = 1.0f - opacityOutDelta;
    while (updateTime < outTimePTS - FLOAT_EPS)
    {
        palette_update_flag = true;
        m_paletteVersion++;
        pts = llround(updateTime);
        dts = llround(updateTime - windowsTransferTime);
        curPos += composePresentationSegment(curPos, CompositionMode::Update, pts, dts, objectWindowTop, true, forced);
        curPos += composePaletteDefinition(buildPalette(toCurve(opacity)), curPos, dts, dts);
        curPos += composeEnd(curPos, pts - 90, pts - 90);

        updateTime = alignToGrid(updateTime + fpsPts);
        opacity -= opacityOutDelta;
    }

    // 3. hide text
    palette_update_flag = false;
    pts = llround(outTimePTS);
    dts = llround(outTimePTS - windowsTransferTime);
    curPos += composePresentationSegment(curPos, CompositionMode::Finish, pts, dts - 90, objectWindowTop, true, false);
    curPos += composeWindowDefinition(curPos, dts, dts - 90, objectWindowTop, objectWindowHeight);
    curPos += composeEnd(curPos, pts - 90, pts - 90);

    assert(curPos - m_pgsBuffer < PG_BUFFER_SIZE);

    dstBufSize = static_cast<uint32_t>(curPos - m_pgsBuffer);
    return m_pgsBuffer;
}

long TextToPGSConverter::composePresentationSegment(uint8_t* buff, const CompositionMode mode, const int64_t pts,
                                                    const int64_t dts, const uint16_t top, const bool needPgHeader,
                                                    const bool forced)
{
    uint8_t* curPos = buff;
    if (needPgHeader)
        curPos += writePGHeader(curPos, pts, dts);
    *curPos++ = PCS_DEF_SEGMENT;
    curPos += 2;  // skip length field
    uint8_t* startPos = curPos;

    curPos += composeVideoDescriptor(curPos);
    curPos += composeCompositionDescriptor(curPos, m_composition_number++,
                                           mode == CompositionMode::Start ? EPOTH_START : EPOTH_NORMAL);
    *curPos++ = static_cast<uint8_t>(palette_update_flag << 7);  // palette_update_flag = 0 and 7 reserved bits
    *curPos++ = m_paletteID;                                     // paletteID ref
    *curPos++ = mode != CompositionMode::Finish ? 1 : 0;         // number_of_composition_objects
    // composition object
    if (mode != CompositionMode::Finish)
    {
        *curPos++ = 0;  // objectID ref
        *curPos++ = 0;  // objectID ref
        *curPos++ = 0;  // windowID ref
        if (forced)
            *curPos++ = 0x40;  // object_cropped_flag = false, forced_on_flag = true
        else
            *curPos++ = 0;  // object_cropped_flag = false, forced_on_flag = false
        AV_WB16(curPos, 0);
        curPos += 2;  // object horizontal position = 0
        AV_WB16(curPos, top);
        curPos += 2;  // object vertical position
    }

    AV_WB16(startPos - 2, static_cast<uint16_t>(curPos - startPos));
    return static_cast<long>(curPos - buff);
}

long TextToPGSConverter::composeVideoDescriptor(uint8_t* buff) const
{
    uint8_t* curPos = buff;
    AV_WB16(curPos, m_videoWidth);
    curPos += 2;
    AV_WB16(curPos, m_videoHeight);
    curPos += 2;
    *curPos++ = static_cast<uint8_t>(PGSStreamReader::calcFpsIndex(m_videoFps) << 4);
    return static_cast<long>(curPos - buff);
}

long TextToPGSConverter::composeWindowDefinition(uint8_t* buff, const int64_t pts, const int64_t dts,
                                                 const uint16_t top, const uint16_t height,
                                                 const bool needPgHeader) const
{
    uint8_t* curPos = buff;
    if (needPgHeader)
        curPos += writePGHeader(curPos, pts, dts);
    *curPos++ = WINDOWS_DEF_SEGMENT;
    curPos += 2;  // skip length field
    uint8_t* startPos = curPos;
    *curPos++ = 1;  // number of windows
    curPos += composeWindow(curPos, top, height);
    AV_WB16(startPos - 2, static_cast<uint16_t>(curPos - startPos));
    return static_cast<long>(curPos - buff);
}

long TextToPGSConverter::composeCompositionDescriptor(uint8_t* buff, const uint16_t number, const uint8_t state)
{
    uint8_t* curPos = buff;
    AV_WB16(curPos, number);
    curPos += 2;
    *curPos++ = static_cast<uint8_t>(state << 6);
    return static_cast<long>(curPos - buff);
}

long TextToPGSConverter::composeWindow(uint8_t* buff, const uint16_t top, const uint16_t height) const
{
    uint8_t* curPos = buff;
    *curPos++ = 0;  // window ID
    AV_WB16(curPos, 0);
    curPos += 2;
    AV_WB16(curPos, top);
    curPos += 2;
    AV_WB16(curPos, m_videoWidth);
    curPos += 2;
    AV_WB16(curPos, height);
    curPos += 2;
    return static_cast<long>(curPos - buff);
}

long TextToPGSConverter::composePaletteDefinition(const Palette& palette, uint8_t* buff, const int64_t pts,
                                                  const int64_t dts, const bool needPgHeader) const
{
    uint8_t* curPos = buff;
    if (needPgHeader)
        curPos += writePGHeader(curPos, pts, dts);
    *curPos++ = PALETTE_DEF_SEGMENT;
    curPos += 2;  // skip length field
    uint8_t* startPos = curPos;
    *curPos++ = m_paletteID;       // palette ID
    *curPos++ = m_paletteVersion;  // palette version number
    for (const auto [fst, snd] : palette)
    {
        *curPos++ = fst;
        *curPos++ = snd.Y;
        *curPos++ = snd.Cr;
        *curPos++ = snd.Cb;
        *curPos++ = snd.alpha;
    }
    AV_WB16(startPos - 2, static_cast<uint16_t>(curPos - startPos));  // correct length field
    return static_cast<long>(curPos - buff);
}

long TextToPGSConverter::composeObjectDefinition(uint8_t* buff, const int64_t pts, const int64_t dts,
                                                 const int firstLine, const int lastLine, const bool needPgHeader) const
{
    std::vector<uint8_t*> seqPos;

    const uint8_t* srcData = m_renderedData;
    int srcProcessed = 0;
    int blocks = 0;
    uint8_t* curPos = buff;
    uint8_t* sizePos = buff;
    do
    {
        if (needPgHeader)
            curPos += writePGHeader(curPos, pts, dts);
        *curPos++ = OBJECT_DEF_SEGMENT;
        curPos += 2;  // skip length field
        uint8_t* fragmentStart = curPos;
        *curPos++ = 0;  // objectID
        *curPos++ = 0;  // objectID
        *curPos++ = 0;  // object version number
        seqPos.push_back(curPos);
        *curPos++ = 0;  // 0xc0; // sequence descriptor: first=true, last=true

        if (blocks == 0)
        {
            sizePos = curPos;
            // object data header
            curPos += 3;  // skip total size
            AV_WB16(curPos, m_videoWidth);
            curPos += 2;
            AV_WB16(curPos, static_cast<uint16_t>(lastLine - firstLine + 1));
            curPos += 2;
        }

        int MAX_PG_PACKET = 65515;
        if (blocks == 0)
            MAX_PG_PACKET -= 7;
        const int size = FFMIN(m_rleLen - srcProcessed, MAX_PG_PACKET);
        memcpy(curPos, srcData + srcProcessed, size);
        srcProcessed += size;
        curPos += size;

        AV_WB16(fragmentStart - 2, static_cast<uint16_t>(curPos - fragmentStart));  // correct length field
        blocks++;
    } while (srcProcessed < m_rleLen);
    AV_WB24(sizePos, m_rleLen + 4);  // object len
    if (!seqPos.empty())
    {
        *(seqPos[0]) |= 0x80;
        *(seqPos[seqPos.size() - 1]) |= 0x40;
    }
    return static_cast<long>(curPos - buff);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
long TextToPGSConverter::composeEnd(uint8_t* buff, const int64_t pts, const int64_t dts, const bool needPgHeader)
{
    uint8_t* curPos = buff;
    if (needPgHeader)
        curPos += writePGHeader(curPos, pts, dts);
    *curPos++ = END_DEF_SEGMENT;
    curPos += 2;  // skip length field
    uint8_t* startPos = curPos;
    AV_WB16(startPos - 2, static_cast<uint16_t>(curPos - startPos));
    return static_cast<long>(curPos - buff);
}

long TextToPGSConverter::writePGHeader(uint8_t* buff, const int64_t pts, int64_t dts)
{
    if (dts > pts)
        dts = pts;
    *buff++ = 'P';
    *buff++ = 'G';
    auto data = reinterpret_cast<uint32_t*>(buff);
    *data++ = my_htonl(static_cast<uint32_t>(pts));
    if (dts != pts)
        *data = my_htonl(static_cast<uint32_t>(dts));
    else
        *data = 0;
    return 10;
}
