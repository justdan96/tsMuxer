#ifndef TEXT_SUBTITLES_RENDER_WIN_H_
#define TEXT_SUBTITLES_RENDER_WIN_H_

#include "../textSubtitlesRender.h"
#include "windows.h"

namespace text_subtitles
{
class GdiPlusPriv;

class TextSubtitlesRenderWin32 final : public TextSubtitlesRender
{
   public:
    TextSubtitlesRenderWin32();
    ~TextSubtitlesRenderWin32() override;
    void setRenderSize(int width, int height) override;
    void drawText(const std::string& text, RECT* rect) override;
    void setFont(const Font& font) override;
    void getTextSize(const std::string& text, SIZE* mSize) override;
    int getLineSpacing() override;
    int getBaseline() override;
    void flushRasterBuffer() override;

   private:
    HBITMAP m_hbmp;
    BITMAPINFO* m_pbmpInfo;
    HDC m_hdcScreen;
    HDC m_dc;
    HFONT m_hfont;
#ifndef OLD_WIN32_RENDERER
    GdiPlusPriv* m_gdiPriv;
#else
#endif
};

}  // namespace text_subtitles

#endif
