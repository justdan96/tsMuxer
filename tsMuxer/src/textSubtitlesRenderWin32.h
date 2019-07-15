#ifndef __TEXT_SUBTITLES_RENDER_WIN_H
#define __TEXT_SUBTITLES_RENDER_WIN_H

#include "textsubtitlesrender.h"
#include "windows.h"

namespace text_subtitles 
{

class GdiPlusPriv;

class TextSubtitlesRenderWin32 : public TextSubtitlesRender
{
public:
	TextSubtitlesRenderWin32();
	virtual ~TextSubtitlesRenderWin32();
	virtual void setRenderSize(int width, int height);
	virtual void drawText(const std::wstring& text, RECT* rect);
	virtual void setFont(const Font& font);
	virtual void getTextSize(const std::wstring& text, SIZE* mSize);
	virtual int getLineSpacing() override;
    virtual int getBaseline() override;
	virtual void flushRasterBuffer();
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

}

#endif