#ifndef __TEXT_SUBTITLES_RENDER_FT_H
#define __TEXT_SUBTITLES_RENDER_FT_H

#include "textSubtitlesRender.h"
#include <map>
#include <ft2build.h>  

#include FT_FREETYPE_H 
#include FT_GLYPH_H


namespace text_subtitles 
{

class TextSubtitlesRenderFT : public TextSubtitlesRender
{
public:
	TextSubtitlesRenderFT();
	virtual ~TextSubtitlesRenderFT();
	virtual void setRenderSize(int width, int height);
	virtual void drawText(const std::wstring& text, RECT* rect);
	virtual void setFont(const Font& font);
	virtual void getTextSize(const std::wstring& text, SIZE* mSize);
	virtual int getLineSpacing() override;
    virtual int getBaseline() override;
	virtual void flushRasterBuffer();
private:
	static FT_Library library; 
	static std::map<std::string, std::string> m_fontNameToFile;
	FT_Face face;
	bool m_emulateItalic;
	bool m_emulateBold;
	int m_underline_position;
	int m_line_thickness;
    FT_Matrix italic_matrix;
    FT_Matrix bold_matrix;
    FT_Matrix italic_bold_matrix;
	void drawHorLine(int left, int right, int top, RECT* rect);
	std::string findAdditionFontFile(const std::string& fontName, const std::string& fontExt, bool isBold, bool isItalic);
	int loadFont(const std::string& fontName, FT_Face& face);
	void loadFontMap();

	std::map<std::string, FT_Face> m_fontMap;
};

}

#endif
