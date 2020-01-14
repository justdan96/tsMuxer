#ifndef __TEXT_SUBTITLES_H
#define __TEXT_SUBTITLES_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include "windows.h"
#endif

#include <types/types.h>

#include <map>
#include <string>

#include "textSubtitlesRender.h"

namespace text_subtitles
{
enum CompositionMode
{
    CM_Start,
    CM_Update,
    CM_Finish
};

struct TextAnimation
{
    TextAnimation() : fadeInDuration(0.0), fadeOutDuration(0.0) {}

    float fadeInDuration;
    float fadeOutDuration;
};

const static uint8_t PCS_DEF_SEGMENT = 0x16;
const static uint8_t WINDOWS_DEF_SEGMENT = 0x17;
const static uint8_t PALETTE_DEF_SEGMENT = 0x14;
const static uint8_t OBJECT_DEF_SEGMENT = 0x15;
const static uint8_t END_DEF_SEGMENT = 0x80;

const static uint8_t EPOTH_NORMAL = 0;
const static uint8_t EPOTH_START = 2;

static const double PIXEL_DECODING_RATE = 128 * 1000000 / 8;  // in bytes
static const double PIXEL_COMPOSITION_RATE = 256 * 1000000 / 8;

class TextToPGSConverter  //: public TextSubtitlesRenderWin32
{
   public:
    typedef std::map<uint8_t, YUVQuad> Palette;

    TextToPGSConverter(bool sourceIsText);
    ~TextToPGSConverter();
    void setVideoInfo(int width, int height, double fps);
    void enlargeCrop(int width, int height, int* newWidth, int* newHeight);
    void setBottomOffset(int offset) { m_bottomOffset = offset; }
    uint8_t* doConvert(std::wstring& text, const TextAnimation& animation, double inTime, double outTime,
                       uint32_t& dstBufSize);
    TextSubtitlesRender* m_textRender;
    static YUVQuad RGBAToYUVA(uint32_t rgba);
    static RGBQUAD YUVAToRGBA(const YUVQuad& yuv);
    void setImageBuffer(uint8_t* value) { m_imageBuffer = value; }

   public:
    uint32_t m_rleLen;
    int m_bottomOffset;
    int m_composition_number;
    int m_videoWidth;
    int m_videoHeight;
    double m_videoFps;
    uint8_t* m_pgsBuffer;
    uint8_t* m_imageBuffer;
    long composePresentationSegment(uint8_t* buff, CompositionMode mode, int64_t pts, int64_t dts, int top,
                                    bool needPgHeader, bool forced);
    long composeWindowDefinition(uint8_t* buff, int64_t pts, int64_t dts, int top, int height,
                                 bool needPgHeader = true);
    long composeWindow(uint8_t* buff, int top, int height);
    long composePaletteDefinition(const Palette& palette, uint8_t* buff, int64_t pts, int64_t dts,
                                  bool needPgHeader = true);
    long composeObjectDefinition(uint8_t* buff, int64_t pts, int64_t dts, int firstLine, int lastLine,
                                 bool needPgHeader);
    long composeVideoDescriptor(uint8_t* buff);
    long composeCompositionDescriptor(uint8_t* buff, uint16_t number, uint8_t state);
    long composeEnd(uint8_t* buff, int64_t pts, int64_t dts, bool needPgHeader = true);
    long writePGHeader(uint8_t* buff, int64_t pts, int64_t dts);
    double alignToGrid(double value);
    bool rlePack(uint32_t colorMask);
    void reduceColors(uint8_t mask);
    int getRepeatCnt(const uint32_t* pos, const uint32_t* end, uint32_t colorMask);
    uint8_t color32To8(uint32_t* buff, uint32_t colorMask);
    Palette buildPalette(float opacity);
    int renderedHeight() const;
    int minLine() const;
    int maxLine() const;

    // std::vector<uint32_t> m_rleLineLen;
    std::map<YUVQuad, uint8_t> m_paletteYUV;
    uint8_t* m_renderedData;

    Palette m_paletteByColor;
    uint8_t palette_update_flag;
    uint8_t m_paletteID;
    uint8_t m_paletteVersion;
    int m_minLine;
    int m_maxLine;
};
};  // namespace text_subtitles

#endif
