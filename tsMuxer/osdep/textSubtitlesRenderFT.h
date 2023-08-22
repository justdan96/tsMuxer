#ifndef TEXT_SUBTITLES_RENDER_FT_H_
#define TEXT_SUBTITLES_RENDER_FT_H_

#include <ft2build.h>

#include <map>

#include "../textSubtitlesRender.h"

#include FT_FREETYPE_H
//#include FT_GLYPH_H

namespace text_subtitles
{
class TextSubtitlesRenderFT final : public TextSubtitlesRender
{
   public:
    TextSubtitlesRenderFT();
    ~TextSubtitlesRenderFT() override;
    void setRenderSize(int width, int height) override;
    void drawText(const std::string& text, RECT* rect) override;
    void setFont(const Font& font) override;
    void getTextSize(const std::string& text, SIZE* mSize) override;
    int getLineSpacing() override;
    int getBaseline() override;
    void flushRasterBuffer() override;

   private:
    static FT_Library library;
    static std::map<std::string, std::string> m_fontNameToFile;
    FT_Face m_face;
    bool m_emulateItalic;
    bool m_emulateBold;
    int m_underline_position;
    int m_line_thickness;
    FT_Matrix italic_matrix;
    FT_Matrix bold_matrix;
    FT_Matrix italic_bold_matrix;
    void drawHorLine(int left, int right, int top, const RECT* rect) const;
    std::string findAdditionFontFile(const std::string& fontName, const std::string& fontExt, bool isBold,
                                     bool isItalic);
    int loadFont(const std::string& fontName, FT_Face& face);
    static void loadFontMap();

    std::map<std::string, FT_Face> m_fontMap;
};

}  // namespace text_subtitles

#endif
