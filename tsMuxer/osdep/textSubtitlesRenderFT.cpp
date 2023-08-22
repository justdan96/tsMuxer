// #if !defined(_WIN32) || defined(WIN32_DEBUG_FREETYPE)

#ifdef _WIN32
#pragma comment(lib, "../../freetype/lib/freetype.lib")
#endif

#include "textSubtitlesRenderFT.h"

#include "../convertUTF.h"
#include "../vodCoreException.h"
#include "../vod_common.h"
// #include "../math.h"
#include <fs/directory.h>
#include <cmath>

#include <algorithm>
#include <map>

#if defined(_WIN32)
static constexpr char FONT_ROOT[] = "c:/WINDOWS/Fonts";  // for debug only
#elif __linux__ == 1
const static char FONT_ROOT[] = "/usr/share/fonts/";
#elif defined(__APPLE__) && defined(__MACH__)
const static char FONT_ROOT[] = "/System/Library/Fonts/";
#elif defined(__NetBSD__)
const static char FONT_ROOT[] = "/usr/X11R7/lib/X11/fonts/";
#endif

#include <freetype/ftstroke.h>
#include <fs/systemlog.h>

using namespace std;

namespace tsmuxer
{
std::string realpath(const std::string& path)
{
    const std::unique_ptr<char, decltype(::free)*> resolved{::realpath(path.c_str(), nullptr), ::free};
    return resolved ? resolved.get() : std::string();
}
}  // namespace tsmuxer

namespace text_subtitles
{
FT_Library TextSubtitlesRenderFT::library;
std::map<std::string, std::string> TextSubtitlesRenderFT::m_fontNameToFile;

constexpr double PI = 3.1415926f;
constexpr double angle = -PI / 10.0f;

uint32_t convertColor(uint32_t color)
{
    const auto arr = reinterpret_cast<uint8_t*>(&color);
    const uint8_t tmp = arr[0];
    arr[0] = arr[2];
    arr[2] = tmp;
    return color;
}

TextSubtitlesRenderFT::TextSubtitlesRenderFT() : TextSubtitlesRender()
{
    static bool initialized = false;
    if (!initialized)
    {
        int error = FT_Init_FreeType(&library);
        if (error)
            THROW(ERR_COMMON, "Can't initialize freeType font library");
        loadFontMap();
        initialized = true;
    }
    m_pData = nullptr;
    italic_matrix.xx = static_cast<FT_Fixed>(cos(angle) * 0x10000L);
    italic_matrix.xy = static_cast<FT_Fixed>(-sin(angle) * 0x10000L);
    italic_matrix.yx = 0 * 0x10000L;
    italic_matrix.yy = 1 * 0x10000L;

    bold_matrix.xx = static_cast<FT_Fixed>(1.5 * 0x10000L);
    bold_matrix.xy = static_cast<FT_Fixed>(0.0 * 0x10000L);
    bold_matrix.yx = static_cast<FT_Fixed>(0.0 * 0x10000L);
    bold_matrix.yy = static_cast<FT_Fixed>(1.0 * 0x10000L);

    italic_bold_matrix.xx = static_cast<FT_Fixed>(cos(angle) * 1.5 * 0x10000L);
    italic_bold_matrix.xy = static_cast<FT_Fixed>(-sin(angle) * 1.5 * 0x10000L);
    italic_bold_matrix.yx = 0 * 0x10000L;
    italic_bold_matrix.yy = 1 * 0x10000L;
}

void TextSubtitlesRenderFT::loadFontMap()
{
    vector<string> fileList;
    // sort(fileList.begin(), fileList.end());
    findFilesRecursive(FONT_ROOT, "*.ttf", &fileList);
#if defined(__APPLE__) && defined(__MACH__)
    vector<string> fileList1;
    findFilesRecursive("/Library/Fonts/", "*.ttf", &fileList1);
    fileList.insert(fileList.end(), fileList1.begin(), fileList1.end());
    findFilesRecursive("~/Library/Fonts/", "*.ttf", &fileList1);
    fileList.insert(fileList.end(), fileList1.begin(), fileList1.end());
#endif

    for (auto& fontFile : fileList)
    {
        // LTRACE(LT_INFO, 2, "before loading font " << fileList[i].c_str());
        if (strEndWith(fontFile, "AppleMyungjo.ttf"))
            continue;

        FT_Face font;
        const int error = FT_New_Face(library, fontFile.c_str(), 0, &font);
        if (error == 0)
        {
            string fontFamily = strToLowerCase(font->family_name);

            auto itr = m_fontNameToFile.find(fontFamily);

            if (itr == m_fontNameToFile.end() || fontFile.length() < itr->second.length())
            {
                m_fontNameToFile[fontFamily] = tsmuxer::realpath(fontFile);
            }
            FT_Done_Face(font);
        }
        // LTRACE(LT_INFO, 2, "after loading font " << fileList[i].c_str());
    }
}

TextSubtitlesRenderFT::~TextSubtitlesRenderFT() { delete m_pData; }

void TextSubtitlesRenderFT::setRenderSize(int width, int height)
{
    m_width = width;
    m_height = height;
    delete m_pData;
    const int size = width * height * 4;
    m_pData = new uint8_t[size];  // 32bpp ARGB buffer
}

string TextSubtitlesRenderFT::findAdditionFontFile(const string& fontName, const string& fontExt, bool isBold,
                                                   bool isItalic)
{
    m_emulateItalic = false;
    m_emulateBold = false;
    if (isBold && isItalic)
    {
        if (fileExists(fontName + string("BI") + fontExt))
            return fontName + string("BI") + fontExt;
        if (fileExists(fontName + string("Bi") + fontExt))
            return fontName + string("Bi") + fontExt;
        if (fileExists(fontName + string("Bd") + fontExt))
        {
            m_emulateItalic = true;
            return fontName + string("Bd") + fontExt;
        }
        if (fileExists(fontName + string("B") + fontExt))
        {
            m_emulateItalic = true;
            return fontName + string("B") + fontExt;
        }
        if (fileExists(fontName + string("It") + fontExt))
        {
            m_emulateBold = true;
            return fontName + string("It") + fontExt;
        }
        if (fileExists(fontName + string("I") + fontExt))
        {
            m_emulateBold = true;
            return fontName + string("I") + fontExt;
        }
        m_emulateBold = true;
        m_emulateItalic = true;
        return fontName + fontExt;
    }
    if (isBold)
    {
        if (fileExists(fontName + string("Bd") + fontExt))
        {
            return fontName + string("Bd") + fontExt;
        }
        if (fileExists(fontName + string("B") + fontExt))
        {
            return fontName + string("B") + fontExt;
        }
        m_emulateBold = true;
        return fontName + fontExt;
    }
    if (isItalic)
    {
        if (fileExists(fontName + string("It") + fontExt))
        {
            return fontName + string("It") + fontExt;
        }
        if (fileExists(fontName + string("I") + fontExt))
        {
            return fontName + string("I") + fontExt;
        }
        m_emulateItalic = true;
        return fontName + fontExt;
    }

    return fontName + fontExt;
}

int TextSubtitlesRenderFT::loadFont(const string& fontName, FT_Face& face)
{
    const auto itr = m_fontMap.find(fontName);
    if (itr == m_fontMap.end())
    {
        const int error = FT_New_Face(library, fontName.c_str(), 0, &face);
        if (error)
            return error;
        // m_fontMap.insert(make_pair<string, FT_Face>(fontName, face));
        m_fontMap.insert(make_pair(fontName, face));
    }
    else
        face = itr->second;
    return 0;
}

void TextSubtitlesRenderFT::setFont(const Font& font)
{
    if (m_font != font)
    {
        m_font = font;
        string fontName = font.m_name;
        if (!strEndWith(fontName, string(".ttf")))
        {
            std::string fontLower = strToLowerCase(fontName);
            auto itr = m_fontNameToFile.find(fontLower);
            if (itr != m_fontNameToFile.end())
                fontName = itr->second;
            else
                THROW(ERR_COMMON, "Can't find ttf file for font " << fontName)
        }
        string fileExt = extractFileExt(fontName);
        if (fileExt.length() > 0)
            fileExt = string(".") + fileExt;
        fontName = fontName.substr(0, fontName.length() - fileExt.length());
        string postfix;
        string modFontName =
            findAdditionFontFile(fontName, fileExt, font.m_opts & Font::BOLD, font.m_opts & Font::ITALIC);
        int error = loadFont(modFontName, m_face);
        if (error == FT_Err_Unknown_File_Format)
        {
            THROW(ERR_COMMON, "Unknown font format for file " << modFontName.c_str())
        }
        else if (error)
        {
            THROW(ERR_COMMON, "Can't load font file " << modFontName.c_str())
        }

        error = FT_Set_Char_Size(m_face,           /* handle to face object */
                                 0,                /* char_width in 1/64th of points */
                                 font.m_size * 64, /* char_height in 1/64th of points */
                                 64,               /* horizontal device resolution */
                                 64);              /* vertical device resolution */
        if (error)
            THROW(ERR_COMMON, "Invalid font size " << font.m_size)

        m_line_thickness = FT_MulFix(m_face->underline_thickness, m_face->size->metrics.y_scale) >> 6;
        if (m_line_thickness < 1)
            m_line_thickness = 1;
        m_underline_position = -FT_MulFix(m_face->underline_position, m_face->size->metrics.y_scale);
        m_underline_position >>= 6;

        if (m_font.m_charset)
        {
            auto encoding = reinterpret_cast<FT_Encoding*>(&m_font.m_charset);
            error = FT_Select_Charmap(m_face, *encoding);
            if (error)
                THROW(ERR_COMMON, "Can't load charset " << m_font.m_charset << " for font " << modFontName.c_str())
        }
    }
}

uint32_t grayColorToARGB(unsigned char gray, uint32_t color)
{
    uint32_t rez = color;
    const auto bPtr = reinterpret_cast<uint8_t*>(&rez);
    gray &= 0xF0;
    for (int i = 0; i < 4; ++i)
        bPtr[i] = static_cast<uint8_t>(lround(static_cast<float>(bPtr[i]) * static_cast<float>(gray) / 240.0f));
    return rez;
}

uint32_t clrMax(uint32_t first, uint32_t second)
{
    const auto rgba = reinterpret_cast<RGBQUAD*>(&first);
    const auto rgba2 = reinterpret_cast<RGBQUAD*>(&second);
    const int first_yuv = static_cast<int>(0.257 * rgba->rgbRed + 0.504 * rgba->rgbGreen + 0.098 * rgba->rgbBlue) + 16;
    const int second_yuv =
        static_cast<int>(0.257 * rgba2->rgbRed + 0.504 * rgba2->rgbGreen + 0.098 * rgba2->rgbBlue) + 16;
    return first_yuv > second_yuv ? first : second;
}

void TextSubtitlesRenderFT::drawHorLine(int left, int right, int top, const RECT* rect) const
{
    if (top < rect->top || top >= rect->bottom)
        return;
    if (left < rect->left)
        left = rect->left;
    if (right > rect->right)
        right = rect->right;
    const int offset = m_width * top + left;
    auto dst = reinterpret_cast<uint32_t*>(m_pData) + offset;
    for (int x = left; x <= right; ++x) *dst++ = convertColor(m_font.m_color);
}

struct Pixel32
{
    Pixel32(const uint32_t val = 0)
        : b(static_cast<uint8_t>(val)),
          g(static_cast<uint8_t>(val >> 8)),
          r(static_cast<uint8_t>(val >> 16)),
          a(static_cast<uint8_t>(val >> 24))
    {
    }
    Pixel32(const uint8_t bi, const uint8_t gi, const uint8_t ri, const uint8_t ai = 255)
    {
        b = bi;
        g = gi;
        r = ri;
        a = ai;
    }

    uint8_t b, g, r, a;
};

struct Vec2
{
    Vec2() = default;
    Vec2(const int a, const int b) : x(a), y(b) {}

    int x, y;
};

struct Rect
{
    Rect() : xmin(0), xmax(0), ymin(0), ymax(0) {}

    Rect(const int left, const int top, const int right, const int bottom)
        : xmin(left), xmax(right), ymin(top), ymax(bottom)
    {
    }

    void Include(const Vec2& r)
    {
        xmin = FFMIN(xmin, r.x);
        ymin = FFMIN(ymin, r.y);
        xmax = FFMAX(xmax, r.x);
        ymax = FFMAX(ymax, r.y);
    }

    int Width() const { return xmax - xmin + 1; }
    int Height() const { return ymax - ymin + 1; }

    int xmin, xmax, ymin, ymax;
};

struct Span
{
    Span() : x(0), y(0), width(0), coverage(0) {}

    Span(int _x, int _y, int _width, uint8_t _coverage) : x(_x), y(_y), width(_width), coverage(_coverage) {}

    int x, y, width;
    uint8_t coverage;
};

typedef std::vector<Span> Spans;

void RasterCallback(const int y, const int count, const FT_Span* const spans, void* const user)
{
    const auto sptr = static_cast<Spans*>(user);
    for (int i = 0; i < count; ++i) sptr->push_back(Span(spans[i].x, y, spans[i].len, spans[i].coverage));
}

void RenderSpans(const FT_Library& library, FT_Outline* const outline, Spans* spans)
{
    FT_Raster_Params params = {};
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
    params.gray_spans = RasterCallback;
    params.user = spans;

    FT_Outline_Render(library, outline, &params);
}

void RenderGlyph(const FT_Library& library, const uint32_t ch, const FT_Face& face, const Pixel32& fontCol,
                 const Pixel32 outlineColOut, const Pixel32 outlineColIner, const float outlineWidth, int left, int top,
                 const int width, const int height, uint32_t* dstData)
{
    // Load the glyph we are looking for.
    const FT_UInt gindex = FT_Get_Char_Index(face, ch);
    // Need an outline for this to work.
    if (FT_Load_Glyph(face, gindex, FT_LOAD_NO_BITMAP) != 0 || face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
        return;

    // Render the basic glyph to a span list.
    Spans spans;
    RenderSpans(library, &face->glyph->outline, &spans);

    // Next we need the spans for the outline.
    Spans outlineSpansOut;
    Spans outlineSpansIner;

    FT_Glyph glyph;
    if (FT_Get_Glyph(face->glyph, &glyph) == 0)
    {
        FT_Stroker strokerOut;
        FT_Stroker_New(library, &strokerOut);
        FT_Stroker_Set(strokerOut, static_cast<int>(outlineWidth * 64), FT_STROKER_LINECAP_ROUND,
                       FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Glyph_StrokeBorder(&glyph, strokerOut, 0, 1);
        // Again, this needs to be an outline to work.
        if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            // Render the outline spans to the span list
            FT_Outline* o = &reinterpret_cast<FT_OutlineGlyph>(glyph)->outline;
            RenderSpans(library, o, &outlineSpansOut);
        }
        FT_Stroker_Done(strokerOut);  // Clean up afterwards.
        FT_Done_Glyph(glyph);

        FT_Get_Glyph(face->glyph, &glyph);
        FT_Stroker strokerIn;
        FT_Stroker_New(library, &strokerIn);
        FT_Stroker_Set(strokerIn, static_cast<int>(outlineWidth * 32), FT_STROKER_LINECAP_ROUND,
                       FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Glyph_StrokeBorder(&glyph, strokerIn, 0, 1);
        if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            FT_Outline* o = &reinterpret_cast<FT_OutlineGlyph>(glyph)->outline;
            RenderSpans(library, o, &outlineSpansIner);
        }
        FT_Stroker_Done(strokerIn);

        // Now we need to put it all together.
        if (!spans.empty())
        {
            // Figure out what the bounding rect is for both the span lists.
            Rect rect(spans[0].x, spans[0].y, spans[0].x, spans[0].y);
            for (const auto& span : spans)
            {
                rect.Include(Vec2(span.x, span.y));
                rect.Include(Vec2(span.x + span.width - 1, span.y));
            }
            for (const auto& span : outlineSpansOut)
            {
                rect.Include(Vec2(span.x, span.y));
                rect.Include(Vec2(span.x + span.width - 1, span.y));
            }
            for (const auto& span : outlineSpansIner)
            {
                rect.Include(Vec2(span.x, span.y));
                rect.Include(Vec2(span.x + span.width - 1, span.y));
            }

            // This is unused in this test but you would need this to draw
            // more than one glyph.
            const int bearingX = face->glyph->metrics.horiBearingX >> 6;
            const int bearingY = face->glyph->metrics.horiBearingY >> 6;

            // Get some metrics of our image.
            const int imgHeight = rect.Height();

            top += (face->size->metrics.ascender >> 6) - bearingY;
            left += bearingX;

            // Loop over the outline spans and just draw them into the image.
            for (const auto& span : outlineSpansOut)
            {
                for (int w = 0; w < span.width; ++w)
                {
                    const int y = imgHeight - 1 - (span.y - rect.ymin) + top;
                    const int x = span.x + w + left;
                    if (y >= 0 && y < height && x >= 0 && x < width)
                    {
                        const int offset = y * width + x;
                        const auto dst = reinterpret_cast<Pixel32*>(dstData) + offset;
                        if (*reinterpret_cast<uint32_t*>(dst) == 0)
                            *dst = Pixel32(
                                outlineColOut.r, outlineColOut.g, outlineColOut.b,
                                static_cast<uint8_t>(static_cast<float>(span.coverage * outlineColOut.a) / 255.0F));
                    }
                }
            }

            // Loop over the outline spans and just draw them into the image.
            for (const auto& span : outlineSpansIner)
            {
                for (int w = 0; w < span.width; ++w)
                {
                    const int y = imgHeight - 1 - (span.y - rect.ymin) + top;
                    const int x = span.x + w + left;
                    if (y >= 0 && y < height && x >= 0 && x < width)
                    {
                        const int offset = y * width + x;
                        Pixel32* dst = reinterpret_cast<Pixel32*>(dstData) + offset;
                        *dst = Pixel32(outlineColIner.r, outlineColIner.g, outlineColIner.b, span.coverage);
                    }
                }
            }

            // Then loop over the regular glyph spans and blend them into the image.
            for (const auto& span : spans)
            {
                for (int w = 0; w < span.width; ++w)
                {
                    const int y = imgHeight - 1 - (span.y - rect.ymin) + top;
                    const int x = span.x + w + left;

                    if (y >= 0 && y < height && x >= 0 && x < width)
                    {
                        const int offset = y * width + x;
                        const auto dst = reinterpret_cast<Pixel32*>(dstData) + offset;

                        const auto src = Pixel32(fontCol.r, fontCol.g, fontCol.b, span.coverage);
                        dst->r = static_cast<uint8_t>(static_cast<float>(dst->r + (src.r - dst->r) * src.a) / 255.0f);
                        dst->g = static_cast<uint8_t>(static_cast<float>(dst->g + (src.g - dst->g) * src.a) / 255.0f);
                        dst->b = static_cast<uint8_t>(static_cast<float>(dst->b + (src.b - dst->b) * src.a) / 255.0f);
                        dst->a = FFMIN(255, dst->a + src.a);
                    }
                }
            }
        }
    }
}

void TextSubtitlesRenderFT::drawText(const string& text, RECT* rect)
{
    FT_Vector pen;
    pen.x = rect->left;
    pen.y = rect->top;
    if (rect->left < 0)
        rect->left = 0;
    if (rect->top < 0)
        rect->top = 0;

    int maxX = 0;

    if (m_emulateItalic && m_emulateBold)
        FT_Set_Transform(m_face, &italic_bold_matrix, &pen);
    else if (m_emulateItalic)
        FT_Set_Transform(m_face, &italic_matrix, &pen);
    else if (m_emulateBold)
        FT_Set_Transform(m_face, &bold_matrix, &pen);
    else
        FT_Set_Transform(m_face, nullptr, &pen);

    const uint8_t alpha = m_font.m_color >> 24;
    const auto outColor = static_cast<uint8_t>(lround(static_cast<float>(alpha) / 255.0 * 48.0));
    convertUTF::IterateUTF8Chars(text,
                                 [&](auto c)
                                 {
                                     RenderGlyph(library, c, m_face, m_font.m_color, Pixel32(0, 0, 0, outColor),
                                                 Pixel32(0, 0, 0, alpha), m_font.m_borderWidth, pen.x, pen.y,
                                                 rect->right, rect->bottom, reinterpret_cast<uint32_t*>(m_pData));
                                     pen.x += m_face->glyph->advance.x >> 6;
                                     pen.x += lround(m_font.m_borderWidth / 2.0F);
                                     if (m_emulateBold || m_emulateItalic)
                                         pen.x += m_line_thickness - 1;
                                     maxX = pen.x + m_face->glyph->bitmap_left;
                                     return true;
                                 });
    if (m_font.m_opts & Font::UNDERLINE || m_font.m_opts & Font::STRIKE_OUT)
    {
        if (m_font.m_opts & Font::UNDERLINE)
        {
            const int y = pen.y + (m_face->size->metrics.ascender >> 6) + m_underline_position - m_line_thickness / 2;
            for (int i = 0; i < m_line_thickness; ++i)
                drawHorLine(rect->left, /*rect->left + size.cx*/ maxX, y + i, rect);
        }
        if (m_font.m_opts & Font::STRIKE_OUT)
        {
            const int y =
                pen.y + static_cast<int>((m_face->size->metrics.ascender >> 6) * 2.0 / 3.0) - m_line_thickness / 2;
            for (int i = 0; i < m_line_thickness; ++i)
                drawHorLine(rect->left, /*rect->left + size.cx*/ pen.x, y + i, rect);
        }
    }
}

void TextSubtitlesRenderFT::getTextSize(const string& text, SIZE* mSize)
{
    FT_Vector pen;
    pen.x = 0;
    pen.y = 0;
    mSize->cy = mSize->cx = 0;

    if (m_emulateItalic && m_emulateBold)
        FT_Set_Transform(m_face, &italic_bold_matrix, &pen);
    else if (m_emulateItalic)
        FT_Set_Transform(m_face, &italic_matrix, &pen);
    else if (m_emulateBold)
        FT_Set_Transform(m_face, &bold_matrix, &pen);
    else
        FT_Set_Transform(m_face, nullptr, &pen);

    convertUTF::IterateUTF8Chars(text,
                                 [&](auto c)
                                 {
                                     const int glyph_index = FT_Get_Char_Index(m_face, c);
                                     int error = FT_Load_Glyph(m_face, glyph_index, 0);
                                     if (error)
                                         THROW(ERR_COMMON, "Can't load symbol code '" << c << "' from font")

                                     error = FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);
                                     if (error)
                                         THROW(ERR_COMMON, "Can't render symbol code '" << c << "' from font")
                                     pen.x += m_face->glyph->advance.x >> 6;
                                     if (m_emulateBold || m_emulateItalic)
                                         pen.x += m_line_thickness - 1;
                                     pen.x += lround(m_font.m_borderWidth / 2.0F);
                                     mSize->cy = m_face->size->metrics.height >> 6;
                                     mSize->cx = pen.x + m_face->glyph->bitmap_left;
                                     return true;
                                 });
}

int TextSubtitlesRenderFT::getLineSpacing() { return m_face->size->metrics.height >> 6; }

int TextSubtitlesRenderFT::getBaseline() { return abs(m_face->size->metrics.descender) >> 6; }

void TextSubtitlesRenderFT::flushRasterBuffer() { return; }

}  // namespace text_subtitles

#endif
