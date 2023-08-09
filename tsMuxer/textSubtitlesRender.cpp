#include "textSubtitlesRender.h"

#include <cassert>
#include <string>
#include <unordered_map>

#include "math.h"
#include "memory.h"
#include "stdio.h"
#include "utf8Converter.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;

namespace
{
const std::unordered_map<std::string, uint32_t> defaultPallette = {make_pair("black", 0x000000),
                                                                   make_pair("aqua", 0x00ffff),
                                                                   make_pair("red", 0xff0000),
                                                                   make_pair("green", 0x008000),
                                                                   make_pair("blue", 0x0000ff),
                                                                   make_pair("fuchsia", 0xff00ff),
                                                                   make_pair("gray", 0x808080),
                                                                   make_pair("lime", 0x00ff00),
                                                                   make_pair("maroon", 0x800000),
                                                                   make_pair("navy", 0x000080),
                                                                   make_pair("olive", 0x808000),
                                                                   make_pair("purple", 0x800080),
                                                                   make_pair("silver", 0xc0c0c0),
                                                                   make_pair("tea", 0x008080),
                                                                   make_pair("white", 0xffffff),
                                                                   make_pair("yellow", 0xffff00),
                                                                   make_pair("orange", 0xffa500),
                                                                   make_pair("violet", 0xEE82EE),

                                                                   make_pair("aliceblue", 0xf0f8ff),
                                                                   make_pair("antiquewhite", 0xfaebd7),
                                                                   make_pair("aquamarine", 0x7fffd4),
                                                                   make_pair("azure", 0xf0ffff),
                                                                   make_pair("beige", 0xf5f5dc),
                                                                   make_pair("bisque", 0xffe4c4),
                                                                   make_pair("blanchedalmond", 0xffebcd),
                                                                   make_pair("blueviolet", 0x8a2be2),
                                                                   make_pair("brown", 0xa52a2a),
                                                                   make_pair("burlywood", 0xdeb887),
                                                                   make_pair("cadetblue", 0x5f9ea0),
                                                                   make_pair("chartreuse", 0x7fff00),
                                                                   make_pair("chocolate", 0xd2691e),
                                                                   make_pair("cora", 0xff7f50),
                                                                   make_pair("cornflowerblue", 0x6495ed),
                                                                   make_pair("cornsilk", 0xfff8dc),
                                                                   make_pair("crimson", 0xdc143c),
                                                                   make_pair("darkblue", 0x00008b),
                                                                   make_pair("darkcyan", 0x008b8b),
                                                                   make_pair("darkgoldenrod", 0xb8860b),
                                                                   make_pair("darkgray", 0xa9a9a9),
                                                                   make_pair("darkgreen", 0x006400),
                                                                   make_pair("darkkhaki", 0xbdb76b),
                                                                   make_pair("darkmagenta", 0x8b008b),
                                                                   make_pair("darkolivegreen", 0x556b2f),
                                                                   make_pair("darkorange", 0xff8c00),
                                                                   make_pair("darkorchid", 0x9932cc),
                                                                   make_pair("darkred", 0x8b0000),
                                                                   make_pair("darksalmon", 0xe9967a),
                                                                   make_pair("darkseagreen", 0x8fbc8f),
                                                                   make_pair("darkslateblue", 0x483d8b),
                                                                   make_pair("darkslategray", 0x2f4f4f),
                                                                   make_pair("darkturquoise", 0x00ced1),
                                                                   make_pair("darkviolet", 0x9400d3),
                                                                   make_pair("deeppink", 0xff1493),
                                                                   make_pair("deepskyblue", 0x00bfff),
                                                                   make_pair("dimgray", 0x696969),
                                                                   make_pair("dodgerblue", 0x1e90ff),
                                                                   make_pair("firebrick", 0xb22222),
                                                                   make_pair("floralwhite", 0xfffaf0),
                                                                   make_pair("forestgreen", 0x228b22),
                                                                   make_pair("gainsboro", 0xdcdcdc),
                                                                   make_pair("ghostwhite", 0xf8f8ff),
                                                                   make_pair("gold", 0xffd700),
                                                                   make_pair("goldenrod", 0xdaa520),
                                                                   make_pair("greenyellow", 0xadff2f),
                                                                   make_pair("honeydew", 0xf0fff0),
                                                                   make_pair("hotpink", 0xff69b4),
                                                                   make_pair("indianred", 0xcd5c5c),
                                                                   make_pair("indigo", 0x4b0082),
                                                                   make_pair("ivory", 0xfffff0),
                                                                   make_pair("khaki", 0xf0e68c),
                                                                   make_pair("lavender", 0xe6e6fa),
                                                                   make_pair("lavenderblush", 0xfff0f5),
                                                                   make_pair("lawngreen", 0x7cfc00),
                                                                   make_pair("lemonchiffon", 0xfffacd),
                                                                   make_pair("lightblue", 0xadd8e6),
                                                                   make_pair("lightcora", 0xf08080),
                                                                   make_pair("lightcyan", 0xe0ffff),
                                                                   make_pair("lightgoldenrodyellow", 0xfafad2),
                                                                   make_pair("lightgreen", 0x90ee90),
                                                                   make_pair("lightgrey", 0xd3d3d3),
                                                                   make_pair("lightpink", 0xffb6c1),
                                                                   make_pair("lightsalmon", 0xffa07a),
                                                                   make_pair("lightseagreen", 0x20b2aa),
                                                                   make_pair("lightskyblue", 0x87cefa),
                                                                   make_pair("lightslategray", 0x778899),
                                                                   make_pair("lightsteelblue", 0xb0c4de),
                                                                   make_pair("lightyellow", 0xffffe0),
                                                                   make_pair("limegreen", 0x32cd32),
                                                                   make_pair("linen", 0xfaf0e6),
                                                                   make_pair("magenta", 0xff00ff),
                                                                   make_pair("mediumauqamarine", 0x66cdaa),
                                                                   make_pair("mediumblue", 0x0000cd),
                                                                   make_pair("mediumorchid", 0xba55d3),
                                                                   make_pair("mediumpurple", 0x9370d8),
                                                                   make_pair("mediumseagreen", 0x3cb371),
                                                                   make_pair("mediumslateblue", 0x7b68ee),
                                                                   make_pair("mediumspringgreen", 0x00fa9a),
                                                                   make_pair("mediumturquoise", 0x48d1cc),
                                                                   make_pair("mediumvioletred", 0xc71585),
                                                                   make_pair("midnightblue", 0x191970),
                                                                   make_pair("mintcream", 0xf5fffa),
                                                                   make_pair("mistyrose", 0xffe4e1),
                                                                   make_pair("moccasin", 0xffe4b5),
                                                                   make_pair("navajowhite", 0xffdead),
                                                                   make_pair("oldlace", 0xfdf5e6),
                                                                   make_pair("olivedrab", 0x688e23),
                                                                   make_pair("orangered", 0xff4500),
                                                                   make_pair("orchid", 0xda70d6),
                                                                   make_pair("palegoldenrod", 0xeee8aa),
                                                                   make_pair("palegreen", 0x98fb98),
                                                                   make_pair("paleturquoise", 0xafeeee),
                                                                   make_pair("palevioletred", 0xd87093),
                                                                   make_pair("papayawhip", 0xffefd5),
                                                                   make_pair("peachpuff", 0xffdab9),
                                                                   make_pair("peru", 0xcd853f),
                                                                   make_pair("pink", 0xffc0cb),
                                                                   make_pair("plum", 0xdda0dd),
                                                                   make_pair("powderblue", 0xb0e0e6),
                                                                   make_pair("rosybrown", 0xbc8f8f),
                                                                   make_pair("royalblue", 0x4169e1),
                                                                   make_pair("saddlebrown", 0x8b4513),
                                                                   make_pair("salmon", 0xfa8072),
                                                                   make_pair("sandybrown", 0xf4a460),
                                                                   make_pair("seagreen", 0x2e8b57),
                                                                   make_pair("seashel", 0xfff5ee),
                                                                   make_pair("sienna", 0xa0522d),
                                                                   make_pair("skyblue", 0x87ceeb),
                                                                   make_pair("slateblue", 0x6a5acd),
                                                                   make_pair("slategray", 0x708090),
                                                                   make_pair("snow", 0xfffafa),
                                                                   make_pair("springgreen", 0x00ff7f),
                                                                   make_pair("steelblue", 0x4682b4),
                                                                   make_pair("tan", 0xd2b48c),
                                                                   make_pair("thistle", 0xd8bfd8),
                                                                   make_pair("tomato", 0xff6347),
                                                                   make_pair("turquoise", 0x40e0d0),
                                                                   make_pair("wheat", 0xf5deb3),
                                                                   make_pair("whitesmoke", 0xf5f5f5),
                                                                   make_pair("yellowgreen", 0x9acd32)};

size_t findUnquotedStr(const string& str, const string& substr)
{
    if (substr.size() == 0)
        return string::npos;
    bool quote = false;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] == '\"' || str[i] == '\'')
            quote = !quote;
        else if (!quote && str[i] == substr[0])
        {
            bool found = true;
            for (size_t j = 1; j < substr.size(); j++)
            {
                if (i + j >= str.size() || str[i + j] != substr[j])
                {
                    found = false;
                    break;
                }
            }
            if (found)
                return i;
        }
    }
    return string::npos;
}
}  // namespace

namespace text_subtitles
{
TextSubtitlesRender::TextSubtitlesRender() : m_width(0), m_height(0)
{
    m_pData = nullptr;
    m_borderWidth = 0;
}

TextSubtitlesRender::~TextSubtitlesRender() {}

string findFontArg(const string& text, size_t pos)
{
    bool delFound = false;
    size_t firstPos = 0;
    for (size_t i = pos; i < text.size(); i++)
    {
        if (text[i] == '=')
            delFound = true;
        else if (delFound)
        {
            if (text[i] == ' ')
            {
                if (firstPos != 0)
                    return text.substr(firstPos, i - firstPos);
            }
            else if (firstPos == 0)
                firstPos = i;
        }
    }
    if (firstPos != 0)
        return text.substr(firstPos, text.size() - firstPos);
    else
        return "";
}

uint32_t rgbSwap(uint32_t color)
{
    auto rgb = (uint8_t*)&color;
    uint8_t tmp = rgb[0];
    rgb[0] = rgb[2];
    rgb[2] = tmp;
    return color;
}

int TextSubtitlesRender::browserSizeToRealSize(int bSize, double rSize)
{
    if (bSize > DEFAULT_BROWSER_STYLE_FS)
        for (int i = DEFAULT_BROWSER_STYLE_FS; i < bSize; i++) rSize *= BROWSER_FONT_STYLE_INC_COEFF;
    else if (bSize < DEFAULT_BROWSER_STYLE_FS)
        for (int i = bSize; i < DEFAULT_BROWSER_STYLE_FS; i++) rSize /= BROWSER_FONT_STYLE_INC_COEFF;
    return (int)rSize;
}

vector<pair<Font, string>> TextSubtitlesRender::processTxtLine(const std::string& line, vector<Font>& fontStack)
{
    if (fontStack.size() == 0)
    {
        fontStack.push_back(m_font);
        fontStack[0].m_size = DEFAULT_BROWSER_STYLE_FS;
    }
    Font curFont = fontStack[fontStack.size() - 1];

    vector<pair<Font, string>> rez;
    size_t prevTextPos = 0;
    size_t bStartPos = 0;
    bool bStartFound = false;
    for (size_t i = 0; i < line.size(); i++)
    {
        if (line[i] == '<')
        {
            bStartPos = i;
            bStartFound = true;
        }
        else if (line[i] == '>' && bStartFound)
        {
            bool isTag = false;
            bool endTag = false;
            string tagStr = trimStr(line.substr(bStartPos + 1, i - bStartPos - 1));
            string ltagStr = tagStr;
            for (auto& j : ltagStr) j = (char)towlower(j);
            if (ltagStr == "i" || ltagStr == "italic")
            {
                curFont.m_opts |= Font::ITALIC;
                isTag = true;
            }
            else if (ltagStr == "/i" || ltagStr == "/italic")
            {
                endTag = true;
            }
            else if (ltagStr == "b" || ltagStr == "bold")
            {
                curFont.m_opts |= Font::BOLD;
                isTag = true;
            }
            else if (ltagStr == "/b" || ltagStr == "/bold")
            {
                endTag = true;
            }
            else if (ltagStr == "u" || ltagStr == "underline")
            {
                curFont.m_opts |= Font::UNDERLINE;
                isTag = true;
            }
            else if (ltagStr == "/u" || ltagStr == "/underline")
            {
                endTag = true;
            }
            else if (ltagStr == "f" || ltagStr == "force")
            {
                curFont.m_opts |= Font::FORCED;
                isTag = true;
            }
            else if (ltagStr == "/f" || ltagStr == "/force")
            {
                endTag = true;
            }
            else if (ltagStr == "strike")
            {
                curFont.m_opts |= Font::STRIKE_OUT;
                isTag = true;
            }
            else if (ltagStr == "/strike")
            {
                endTag = true;
            }
            else if (strStartWith(ltagStr, "font "))
            {
                size_t fontNamePos = findUnquotedStr(ltagStr, "name");  // ltagStr.find("name");
                if (fontNamePos == string::npos)
                    fontNamePos = findUnquotedStr(ltagStr, "face");
                if (fontNamePos != string::npos)
                    curFont.m_name = unquoteStr(findFontArg(tagStr, fontNamePos /*, lastIndexPos*/));

                size_t colorPos = findUnquotedStr(ltagStr, "color");
                if (colorPos != string::npos)
                {
                    auto arg = unquoteStr(findFontArg(ltagStr, colorPos));
                    auto it = defaultPallette.find(arg);
                    if (it != std::end(defaultPallette))
                    {
                        curFont.m_color = it->second;
                        if (arg.size() > 0 && (arg[0] == '#' || arg[0] == 'x'))
                            curFont.m_color = strToInt32u(arg.substr(1, 16384).c_str(), 16);
                        else if (arg.size() > 1 && (arg[0] == '0' || arg[1] == 'x'))
                            curFont.m_color = strToInt32u(arg.substr(2, 16384).c_str(), 16);
                        else
                            curFont.m_color = strToInt32u(arg.c_str(), 10);
                    }
                    if ((curFont.m_color & 0xff000000u) == 0)
                        curFont.m_color |= 0xff000000u;
                }
                size_t fontSizePos = findUnquotedStr(ltagStr, "size");
                if (fontSizePos != string::npos)
                {
                    string arg = unquoteStr(findFontArg(tagStr, fontSizePos));
                    if (arg.size() > 0)
                    {
                        if (arg[0] == '+' || arg[0] == '-')
                            curFont.m_size += strToInt32(arg.c_str(), 10);
                        else
                            curFont.m_size = strToInt32u(arg.c_str(), 10);
                    }
                }
                isTag = true;
            }
            else if (strStartWith(tagStr, "/font"))
            {
                endTag = true;
            }
            if (isTag || endTag)
            {
                if (bStartPos > prevTextPos)
                {
                    string msg = line.substr(prevTextPos, bStartPos - prevTextPos);
                    rez.push_back(make_pair(fontStack[fontStack.size() - 1], msg));
                }
                if (isTag)
                    fontStack.push_back(curFont);
                else if (fontStack.size() > 1)
                {
                    fontStack.resize(fontStack.size() - 1);
                    curFont = fontStack[fontStack.size() - 1];
                }
                prevTextPos = i + 1;
            }
            bStartFound = false;
        }
    }
    if (line.size() > (unsigned)prevTextPos)
        rez.push_back(make_pair(curFont, line.substr(prevTextPos, line.size() - prevTextPos)));
    double rSize = m_initFont.m_size;
    for (auto& i : rez) i.first.m_size = browserSizeToRealSize(i.first.m_size, rSize);
    return rez;
}

bool TextSubtitlesRender::rasterText(const std::string& text)
{
    bool forced = false;
    memset(m_pData, 0, (size_t)m_width * m_height * 4);
    vector<Font> fontStack;
    vector<string> lines = splitStr(text.c_str(), '\n');
    int curY = 0;
    m_initFont = m_font;
    for (auto& i : lines)
    {
        vector<pair<Font, string>> txtParts = processTxtLine(i, fontStack);
        for (auto& j : txtParts)
        {
            if (j.first.m_opts & Font::FORCED)
                forced = true;
        }

        int ySize = 0;
        int tWidth = 0;
        int maxHeight = 0;
        int maxBaseLine = 0;
        vector<int> xSize;
        for (auto& j : txtParts)
        {
            setFont(j.first);
            SIZE mSize;
            getTextSize(j.second, &mSize);
            ySize = FFMAX(ySize, mSize.cy);
            maxHeight = FFMAX(maxHeight, getLineSpacing());
            maxBaseLine = FFMAX(maxBaseLine, getBaseline());
            xSize.push_back(mSize.cx);
            tWidth += mSize.cx;
        }
        int xOffs = (m_width - tWidth) / 2;
        int curX = 0;
        for (size_t j = 0; j < txtParts.size(); j++)
        {
            Font font(txtParts[j].first);
            setFont(font);

            RECT rect = {curX + xOffs, curY + (maxHeight - getLineSpacing()) - (maxBaseLine - getBaseline()), m_width,
                         m_height};
            drawText(txtParts[j].second, &rect);

            curX += xSize[j];
        }
        curY += (int)(ySize * m_font.m_lineSpacing);
    }
    flushRasterBuffer();
    m_font = m_initFont;

    return forced;
}

const static uint32_t BORDER_COLOR = 0xff020202;
const static uint32_t BORDER_COLOR_TMP = RGB(0x1, 0x1, 0x1);

inline void setBPoint(uint32_t* addr)
{
    if (*addr == 0)
        *addr = BORDER_COLOR_TMP;
}

void TextSubtitlesRender::addBorder(int borderWidth, uint8_t* data, int width, int height)
{
    // add black border
    for (int i = 0; i < borderWidth; ++i)
    {
        auto dst = (uint32_t*)data;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (*dst != 0 && *dst != BORDER_COLOR_TMP)
                {
                    if (x > 0)
                    {
                        setBPoint(dst - 1);
                        if (y > 0)
                            setBPoint(dst - 1 - width);
                        if (y < height - 1)
                            setBPoint(dst - 1 + width);
                    }
                    if (y > 0)
                        setBPoint(dst - width);
                    if (y < height - 1)
                        setBPoint(dst + width);
                    if (x < width - 1)
                    {
                        setBPoint(dst + 1);
                        if (y > 0)
                            setBPoint(dst + 1 - width);
                        if (y < height - 1)
                            setBPoint(dst + 1 + width);
                    }
                }
                dst++;
            }
        }
        dst = (uint32_t*)data;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                if (*dst == BORDER_COLOR_TMP)
                    *dst = BORDER_COLOR;
                dst++;
            }
    }
    return;
}

}  // namespace text_subtitles
