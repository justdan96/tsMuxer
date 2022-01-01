#include "textSubtitlesRenderWin32.h"

#include <gdiplus.h>

#include "../vodCoreException.h"
#include "../vod_common.h"
#include "types/types.h"

using namespace std;
using namespace Gdiplus;

namespace text_subtitles
{
#ifndef OLD_WIN32_RENDERER
class GdiPlusPriv
{
   public:
    GdiPlusPriv() { GdiplusStartup(&m_gdiplusToken, &m_gdiplusStartupInput, NULL); }

    ~GdiPlusPriv() { GdiplusShutdown(m_gdiplusToken); }

    Gdiplus::GdiplusStartupInput m_gdiplusStartupInput;
    ULONG_PTR m_gdiplusToken;
};
#endif

TextSubtitlesRenderWin32::TextSubtitlesRenderWin32() : TextSubtitlesRender(), m_hbmp(0)
{
    m_hfont = 0;
#ifndef OLD_WIN32_RENDERER
    m_gdiPriv = new GdiPlusPriv();
#endif

    m_pbmpInfo = 0;
    m_hdcScreen = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
    m_dc = ::CreateCompatibleDC(m_hdcScreen);
}

TextSubtitlesRenderWin32::~TextSubtitlesRenderWin32()
{
    delete[] m_pbmpInfo;
    if (m_hbmp)
        DeleteObject(m_hbmp);
    ::DeleteDC(m_dc);
    ::DeleteDC(m_hdcScreen);

#ifndef OLD_WIN32_RENDERER
    delete m_gdiPriv;
#endif
}

void TextSubtitlesRenderWin32::setRenderSize(int width, int height)
{
    delete[] m_pbmpInfo;
    m_width = width;
    m_height = height;
    m_pbmpInfo = (BITMAPINFO*)new uint8_t[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];
    m_pbmpInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_pbmpInfo->bmiHeader.biWidth = m_width;
    m_pbmpInfo->bmiHeader.biHeight = -m_height;
    m_pbmpInfo->bmiHeader.biPlanes = 1;
    m_pbmpInfo->bmiHeader.biBitCount = 32;
    m_pbmpInfo->bmiHeader.biCompression = BI_RGB;
    m_pbmpInfo->bmiHeader.biClrUsed = 256 * 256 * 256;
    m_hbmp = ::CreateDIBSection(m_dc, m_pbmpInfo, DIB_RGB_COLORS, (void**)&m_pData, 0, 0);
    if (m_hbmp == 0)
        THROW(ERR_COMMON, "Can't initialize graphic subsystem for render text subtitles");
    SelectObject(m_dc, m_hbmp);
    ::SetBkColor(m_dc, RGB(0, 0, 0));
    ::SetBkMode(m_dc, TRANSPARENT);
}

void TextSubtitlesRenderWin32::setFont(const Font& font)
{
    m_font = font;
#ifdef OLD_WIN32_RENDERER
    if (m_hfont)
        ::DeleteObject(m_hfont);
    m_hfont = CreateFont(font.m_size,                                        // height of font
                         0,                                                  // average character width
                         0,                                                  // angle of escapement
                         0,                                                  // base-line orientation angle
                         (font.m_opts & Font::BOLD) ? FW_BOLD : FW_REGULAR,  // font weight
                         font.m_opts & Font::ITALIC,                         // italic attribute option
                         font.m_opts & Font::UNDERLINE,                      // underline attribute option
                         font.m_opts & Font::STRIKE_OUT,                     // strikeout attribute option
                         font.m_charset,                                     // character set identifier
                         OUT_DEFAULT_PRECIS,                                 // output precision
                         CLIP_DEFAULT_PRECIS,                                // clipping precision
                         ANTIALIASED_QUALITY,  // DEFAULT_QUALITY,          // output quality
                         DEFAULT_PITCH,        // pitch and family
                         font.m_name.c_str()   // typeface name
    );
    if (m_hfont == 0)
        THROW(ERR_COMMON, "Can't create font " << font.m_name.c_str());
    ::SelectObject(m_dc, m_hfont);
    ::SetTextColor(m_dc, font.m_color);
#endif
}

void TextSubtitlesRenderWin32::drawText(const std::string& text, RECT* rect)
{
#ifdef OLD_WIN32_RENDERER
    ::DrawText(m_dc, text.c_str(), text.length(), rect, DT_NOPREFIX);
#else
    Gdiplus::Graphics graphics(m_dc);
    graphics.SetSmoothingMode(SmoothingModeHighQuality);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    FontFamily fontFamily(toWide(m_font.m_name).data());
    StringFormat strformat;
    Gdiplus::GraphicsPath path;

    auto text_wide = toWide(text);
    path.AddString(text_wide.data(), (int)text_wide.size(), &fontFamily, m_font.m_opts & 0xf, (float)m_font.m_size,
                   Gdiplus::Point(rect->left, rect->top), &strformat);

    uint8_t alpha = m_font.m_color >> 24;
    uint8_t outColor = (alpha * 48 + 128) / 255;
    Pen pen(Color(outColor, 0, 0, 0), m_font.m_borderWidth * 2.0f);
    pen.SetLineJoin(LineJoinRound);
    graphics.DrawPath(&pen, &path);

    Pen penInner(Color(alpha, 0, 0, 0), (float)m_font.m_borderWidth);
    penInner.SetLineJoin(LineJoinRound);
    graphics.DrawPath(&penInner, &path);

    SolidBrush brush(Color(m_font.m_color));
    graphics.FillPath(&brush, &path);
#endif
}

void TextSubtitlesRenderWin32::getTextSize(const std::string& text, SIZE* mSize)
{
#ifdef OLD_WIN32_RENDERER
    ::GetTextExtentPoint32(m_dc, text.c_str(), text.size(), mSize);
#else
    int opts = m_font.m_opts & 0xf;
    FontFamily fontFamily(toWide(m_font.m_name).data());
    ::Font font(&fontFamily, (float)m_font.m_size, opts, UnitPoint);

    int lineSpacing = fontFamily.GetLineSpacing(FontStyleRegular);
    int lineSpacingPixel = (int)(font.GetSize() * lineSpacing / fontFamily.GetEmHeight(opts));

    StringFormat strformat;
    Gdiplus::GraphicsPath path;
    auto text_wide = toWide(text);
    path.AddString(text_wide.data(), (int)text_wide.size(), &fontFamily, opts, (float)m_font.m_size, Gdiplus::Point(0, 0),
                   &strformat);
    Gdiplus::RectF rect;
    Pen pen(Color(0x30, 0, 0, 0), m_font.m_borderWidth * 2.0f);
    pen.SetLineJoin(LineJoinRound);
    path.GetBounds(&rect, 0, &pen);
    mSize->cx = (int)rect.Width;
    mSize->cy = lineSpacingPixel;
#endif
}

int TextSubtitlesRenderWin32::getLineSpacing()
{
#ifdef OLD_WIN32_RENDERER
    TEXTMETRIC tm;
    ::GetTextMetrics(m_dc, &tm);
    return tm.tmAscent;
#else
    int opts = m_font.m_opts & 0xf;
    Gdiplus::FontFamily fontFamily(toWide(m_font.m_name).data());
    ::Font font(&fontFamily, (float)m_font.m_size, opts, UnitPoint);

    int lineSpacing = fontFamily.GetLineSpacing(opts);
    int lineSpacingPixel = (int)(font.GetSize() * lineSpacing / fontFamily.GetEmHeight(opts));
    return lineSpacingPixel;
#endif
}

int TextSubtitlesRenderWin32::getBaseline()
{
#ifdef OLD_WIN32_RENDERER
    TEXTMETRIC tm;
    ::GetTextMetrics(m_dc, &tm);
    return tm.tmAscent;
#else
    int opts = m_font.m_opts & 0xf;
    Gdiplus::FontFamily fontFamily(toWide(m_font.m_name).data());
    ::Font font(&fontFamily, (float)m_font.m_size, opts, UnitPoint);

    int descentOffset = fontFamily.GetCellDescent(opts);
    int descentPixel = (int)(font.GetSize() * descentOffset / fontFamily.GetEmHeight(opts));
    return descentPixel;
#endif
}

void TextSubtitlesRenderWin32::flushRasterBuffer() { GdiFlush(); }

}  // namespace text_subtitles
