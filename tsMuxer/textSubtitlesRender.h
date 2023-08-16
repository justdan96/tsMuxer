#ifndef TEXT_SUBTITLES_RENDER_
#define TEXT_SUBTITLES_RENDER_

#include <types/types.h>

#include <map>
#include <string>

#ifdef _WIN32
#include "windows.h"
#endif

namespace text_subtitles
{
static constexpr int DEFAULT_BROWSER_STYLE_FS = 3;
static constexpr double BROWSER_FONT_STYLE_INC_COEFF =
    1.4142135623730950488016887242097;  // example: font 2 > font1 to 20%

#ifndef _WIN32
struct RGBQUAD
{
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
    uint8_t rgbReserved;
};
typedef RGBQUAD* LPRGBQUAD;

struct RECT
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
};

struct SIZE
{
    int32_t cx;
    int32_t cy;
};

typedef uint32_t COLORREF;
#define RGB(r, g, b) ((COLORREF)(((uint8_t)(r) | ((uint16_t)((uint8_t)(g)) << 8)) | (((uint32_t)(uint8_t)(b)) << 16)))
#endif

struct YUVQuad
{
    uint8_t Y;
    uint8_t Cr;
    uint8_t Cb;
    uint8_t alpha;
    YUVQuad() : Y(0), Cr(0), Cb(0), alpha(0) {}
    bool operator==(const YUVQuad& second) const
    {
        return Y == second.Y && Cr == second.Cr && Cb == second.Cb && alpha == second.alpha;
    }
    bool operator<(const YUVQuad& second) const
    {
        if (Y != second.Y)
            return Y < second.Y;
        if (Cr != second.Cr)
            return Cr < second.Cr;
        if (Cb != second.Cb)
            return Cb < second.Cb;

        return alpha < second.alpha;
    }
};

struct Font
{
    static constexpr int BOLD = 1;
    static constexpr int ITALIC = 2;
    static constexpr int UNDERLINE = 4;
    static constexpr int STRIKE_OUT = 8;
    static constexpr int FORCED = 16;
    Font() : m_size(18), m_opts(0), m_borderWidth(0.0), m_charset(0), m_color(0x00ffffff), m_lineSpacing(1.0) {}
    bool operator!=(const Font& other) const
    {
        return m_name != other.m_name || m_size != other.m_size || m_opts != other.m_opts ||
               m_borderWidth != other.m_borderWidth || m_charset != other.m_charset || m_color != other.m_color ||
               m_lineSpacing != other.m_lineSpacing;
    }

    std::string m_name;
    int m_size;
    int m_opts;
    float m_borderWidth;
    uint32_t m_charset;
    uint32_t m_color;
    float m_lineSpacing;
};

class TextSubtitlesRender
{
   public:
    TextSubtitlesRender();
    virtual ~TextSubtitlesRender();
    bool rasterText(const std::string& text);  // return true if text was forced

    virtual void setFont(const Font& font) = 0;
    virtual void setRenderSize(int width, int height) = 0;
    virtual void getTextSize(const std::string& text, SIZE* mSize) = 0;
    virtual int getLineSpacing() = 0;
    virtual int getBaseline() = 0;
    virtual void drawText(const std::string& text, RECT* rect) = 0;
    virtual void flushRasterBuffer() = 0;
    // void rescaleRGB(BitmapInfo* bmpDest, BitmapInfo* bmpRef);
    static void addBorder(int borderWidth, uint8_t* data, int width, int height);

    int m_width;
    int m_height;
    uint8_t* m_pData;

   protected:
    Font m_font;

    float m_borderWidth;
    std::vector<std::pair<Font, std::string>> processTxtLine(const std::string& line,
                                                             std::vector<Font>& fontStack) const;
    static int browserSizeToRealSize(int bSize, double rSize);

   private:
    Font m_initFont;
};
}  // namespace text_subtitles
#endif
