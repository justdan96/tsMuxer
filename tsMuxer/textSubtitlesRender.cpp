#include "textSubtitlesRender.h"
#include "vodCoreException.h"
#include <assert.h>
#include <string>
#include "vod_common.h"

#include "utf8Converter.h"
#include "stdio.h"
#include "memory.h"
#include "math.h"

using namespace std;


namespace text_subtitles 
{

const static pair<const wchar_t*, uint32_t> defaultPallette[] = {
	make_pair(L"black",   0x000000),
	make_pair(L"aqua",    0x00ffff),
	make_pair(L"red",     0xff0000),
	make_pair(L"green",   0x008000),
	make_pair(L"blue",    0x0000ff),
	make_pair(L"fuchsia", 0xff00ff),
	make_pair(L"gray",    0x808080),
	make_pair(L"lime",    0x00ff00),
	make_pair(L"maroon",  0x800000),
	make_pair(L"navy",    0x000080),
	make_pair(L"olive",   0x808000),
	make_pair(L"purple",  0x800080),
	make_pair(L"silver",  0xc0c0c0),
	make_pair(L"teal",    0x008080),
	make_pair(L"white",   0xffffff),
	make_pair(L"yellow",  0xffff00),
	make_pair(L"orange",  0xffa500),
	make_pair(L"violet",  0xEE82EE),

	make_pair(L"aliceblue", 0xf0f8ff),
	make_pair(L"antiquewhite", 0xfaebd7),
	make_pair(L"aquamarine", 0x7fffd4),
	make_pair(L"azure", 0xf0ffff),
	make_pair(L"beige", 0xf5f5dc),
	make_pair(L"bisque", 0xffe4c4),
	make_pair(L"blanchedalmond", 0xffebcd),
	make_pair(L"blueviolet", 0x8a2be2),
	make_pair(L"brown", 0xa52a2a),
	make_pair(L"burlywood", 0xdeb887),
	make_pair(L"cadetblue", 0x5f9ea0),
	make_pair(L"chartreuse", 0x7fff00),
	make_pair(L"chocolate", 0xd2691e),
	make_pair(L"coral", 0xff7f50),
	make_pair(L"cornflowerblue", 0x6495ed),
	make_pair(L"cornsilk", 0xfff8dc),
	make_pair(L"crimson", 0xdc143c),
	make_pair(L"darkblue", 0x00008b),
	make_pair(L"darkcyan", 0x008b8b),
	make_pair(L"darkgoldenrod", 0xb8860b),
	make_pair(L"darkgray", 0xa9a9a9),
	make_pair(L"darkgreen", 0x006400),
	make_pair(L"darkkhaki", 0xbdb76b),
	make_pair(L"darkmagenta", 0x8b008b),
	make_pair(L"darkolivegreen", 0x556b2f),
	make_pair(L"darkorange", 0xff8c00),
	make_pair(L"darkorchid", 0x9932cc),
	make_pair(L"darkred", 0x8b0000),
	make_pair(L"darksalmon", 0xe9967a),
	make_pair(L"darkseagreen", 0x8fbc8f),
	make_pair(L"darkslateblue", 0x483d8b),
	make_pair(L"darkslategray", 0x2f4f4f),
	make_pair(L"darkturquoise", 0x00ced1),
	make_pair(L"darkviolet", 0x9400d3),
	make_pair(L"deeppink", 0xff1493),
	make_pair(L"deepskyblue", 0x00bfff),
	make_pair(L"dimgray", 0x696969),
	make_pair(L"dodgerblue", 0x1e90ff),
	make_pair(L"firebrick", 0xb22222),
	make_pair(L"floralwhite", 0xfffaf0),
	make_pair(L"forestgreen", 0x228b22),
	make_pair(L"gainsboro", 0xdcdcdc),
	make_pair(L"ghostwhite", 0xf8f8ff),
	make_pair(L"gold", 0xffd700),
	make_pair(L"goldenrod", 0xdaa520),
	make_pair(L"greenyellow", 0xadff2f),
	make_pair(L"honeydew", 0xf0fff0),
	make_pair(L"hotpink",0xff69b4),
	make_pair(L"indianred",0xcd5c5c),
	make_pair(L"indigo", 0x4b0082),
	make_pair(L"ivory", 0xfffff0),
	make_pair(L"khaki", 0xf0e68c),
	make_pair(L"lavender", 0xe6e6fa),
	make_pair(L"lavenderblush", 0xfff0f5),
	make_pair(L"lawngreen",0x7cfc00),
	make_pair(L"lemonchiffon", 0xfffacd),
	make_pair(L"lightblue",0xadd8e6),
	make_pair(L"lightcoral",0xf08080),
	make_pair(L"lightcyan",0xe0ffff),
	make_pair(L"lightgoldenrodyellow",0xfafad2),
	make_pair(L"lightgreen",0x90ee90),
	make_pair(L"lightgrey",0xd3d3d3),
	make_pair(L"lightpink",0xffb6c1),
	make_pair(L"lightsalmon",0xffa07a),
	make_pair(L"lightseagreen",0x20b2aa),
	make_pair(L"lightskyblue",0x87cefa),
	make_pair(L"lightslategray",0x778899),
	make_pair(L"lightsteelblue",0xb0c4de),
	make_pair(L"lightyellow",0xffffe0),
	make_pair(L"limegreen",0x32cd32),
	make_pair(L"linen",0xfaf0e6),
	make_pair(L"magenta",0xff00ff),
	make_pair(L"mediumauqamarine",0x66cdaa),
	make_pair(L"mediumblue",0x0000cd),
	make_pair(L"mediumorchid",0xba55d3),
	make_pair(L"mediumpurple",0x9370d8),
	make_pair(L"mediumseagreen",0x3cb371),
	make_pair(L"mediumslateblue",0x7b68ee),
	make_pair(L"mediumspringgreen",0x00fa9a),
	make_pair(L"mediumturquoise",0x48d1cc),
	make_pair(L"mediumvioletred",0xc71585),
	make_pair(L"midnightblue",0x191970),
	make_pair(L"mintcream",0xf5fffa),
	make_pair(L"mistyrose",0xffe4e1),
	make_pair(L"moccasin",0xffe4b5),
	make_pair(L"navajowhite",0xffdead),
	make_pair(L"oldlace",0xfdf5e6),
	make_pair(L"olivedrab",0x688e23),
	make_pair(L"orangered",0xff4500),
	make_pair(L"orchid",0xda70d6),
	make_pair(L"palegoldenrod",0xeee8aa),
	make_pair(L"palegreen",0x98fb98),
	make_pair(L"paleturquoise",0xafeeee),
	make_pair(L"palevioletred",0xd87093),
	make_pair(L"papayawhip",0xffefd5),
	make_pair(L"peachpuff",0xffdab9),
	make_pair(L"peru",0xcd853f),
	make_pair(L"pink",0xffc0cb),
	make_pair(L"plum",0xdda0dd),
	make_pair(L"powderblue",0xb0e0e6),
	make_pair(L"rosybrown",0xbc8f8f),
	make_pair(L"royalblue",0x4169e1),
	make_pair(L"saddlebrown",0x8b4513),
	make_pair(L"salmon",0xfa8072),
	make_pair(L"sandybrown",0xf4a460),
	make_pair(L"seagreen",0x2e8b57),
	make_pair(L"seashell",0xfff5ee),
	make_pair(L"sienna",0xa0522d),
	make_pair(L"skyblue",0x87ceeb),
	make_pair(L"slateblue",0x6a5acd),
	make_pair(L"slategray",0x708090),
	make_pair(L"snow",0xfffafa),
	make_pair(L"springgreen",0x00ff7f),
	make_pair(L"steelblue",0x4682b4),
	make_pair(L"tan",0xd2b48c),
	make_pair(L"thistle",0xd8bfd8),
	make_pair(L"tomato",0xff6347),
	make_pair(L"turquoise",0x40e0d0),
	make_pair(L"wheat",0xf5deb3),
	make_pair(L"whitesmoke",0xf5f5f5),
	make_pair(L"yellowgreen",0x9acd32)
};

TextSubtitlesRender::TextSubtitlesRender(): m_width(0), m_height(0)
{
	m_pData = 0;
	//m_renderedData = 0;
}

TextSubtitlesRender::~TextSubtitlesRender() {
	//delete [] m_renderedData;
}

wstring findFontArg(const wstring& text, int pos)
{
	bool delFound = false;
	int firstPos = -1;
	for (int i = pos; i < text.size(); i++)
	{
		if (text[i] == L'=')
			delFound = true;
		else if (delFound) {
			if (text[i] == ' ') {
				if (firstPos != -1)
					return text.substr(firstPos, i - firstPos);
			}
			else if (firstPos == -1)
				firstPos = i;
		}
	}
	if (firstPos != -1)
		return text.substr(firstPos, text.size() - firstPos);
	else
		return L"";
}

size_t TextSubtitlesRender::findUnquotedStrW(const wstring& str, const wstring& substr)
{
	if (substr.size() == 0)
		return string::npos;
	bool quote = false;
	for (int i = 0; i < str.size(); i++)
	{
		if (str[i] == L'\"' || str[i] == L'\'')
			quote = !quote;
		else if(!quote && str[i] == substr[0]) 
		{
			bool found = true;
			for (int j = 1; j < substr.size(); j++) 
			{
				if (i + j >= str.size() || str[i+j] != substr[j]) {
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

uint32_t rgbSwap(uint32_t color) {
	uint8_t* rgb = (uint8_t*)&color;
	uint8_t tmp = rgb[0];
	rgb[0] = rgb[2];
	rgb[2] = tmp;
	return color;
}

int TextSubtitlesRender::browserSizeToRealSize(int bSize, double rSize)
{
	if (bSize > DEFAULT_BROWSER_STYLE_FS)
		for (int i = DEFAULT_BROWSER_STYLE_FS; i < bSize; i++)
			rSize *= BROWSER_FONT_STYLE_INC_COEFF;
	else if (bSize < DEFAULT_BROWSER_STYLE_FS)
		for (int i = bSize; i < DEFAULT_BROWSER_STYLE_FS; i++)
			rSize /= BROWSER_FONT_STYLE_INC_COEFF;
	return rSize;
}

vector<pair<Font,wstring> > TextSubtitlesRender::processTxtLine(const std::wstring& line, vector<Font>& fontStack)
{
	if (fontStack.size() == 0) {
		fontStack.push_back(m_font);
		fontStack[0].m_size = DEFAULT_BROWSER_STYLE_FS;
	}
	Font curFont = fontStack[fontStack.size()-1];

	vector<pair<Font,wstring> > rez;
	int prevTextPos = 0;
	int bStartPos = -1;
	for (int i = 0; i < line.size(); i++) 
	{
		if (line[i] == '<')
			bStartPos = i;
		else if (line[i] == '>' && bStartPos != -1) 
		{
			bool isTag = false;
			bool endTag = false;
			wstring tagStr = trimStrW(line.substr(bStartPos+1, i-bStartPos-1));
			wstring ltagStr = tagStr;
			for (int j = 0; j < ltagStr.size(); j++)
				ltagStr[j] = towlower(ltagStr[j]);
			if (ltagStr == L"i" || ltagStr == L"italic") {
				curFont.m_opts |= Font::ITALIC;
				isTag = true;
			}
			else if (ltagStr == L"/i"  || ltagStr == L"/italic") {
				endTag = true;
			}
			else if (ltagStr == L"b" || ltagStr == L"bold") {
				curFont.m_opts |= Font::BOLD;
				isTag = true;
			}
			else if (ltagStr == L"/b" || ltagStr == L"/bold") {
				endTag = true;
			}
			else if (ltagStr == L"u" || ltagStr == L"underline") {
				curFont.m_opts |= Font::UNDERLINE;
				isTag = true;
			}
			else if (ltagStr == L"/u" || ltagStr == L"underline") {
				endTag = true;
			}
            else if (ltagStr == L"f" || ltagStr == L"force") {
                curFont.m_opts |= Font::FORCED;
                isTag = true;
            }
            else if (ltagStr == L"/f" || ltagStr == L"/force") {
                endTag = true;
            }
			else if (ltagStr == L"strike") {
				curFont.m_opts |= Font::STRIKE_OUT;
				isTag = true;
			}
			else if (ltagStr == L"/strike") {
				endTag = true;
			}
			else if (strStartWithW(ltagStr, L"font ")) 
			{
				size_t fontNamePos = findUnquotedStrW(ltagStr, L"name"); //ltagStr.find(L"name");
				if (fontNamePos == string::npos)
					fontNamePos = findUnquotedStrW(ltagStr, L"face");
				if (fontNamePos != string::npos) 
					curFont.m_name = unquoteStrW(findFontArg(tagStr, fontNamePos/*, lastIndexPos*/));

				size_t colorPos = findUnquotedStrW(ltagStr, L"color");
				if (colorPos != string::npos) 
				{
					wstring arg = unquoteStrW(findFontArg(ltagStr, colorPos));
					bool defClrFound = false;
					for (int j = 0; j < sizeof(defaultPallette) / sizeof(pair<const wchar_t*, uint32_t>); j++) {
						if (defaultPallette[j].first == arg) {
							curFont.m_color = defaultPallette[j].second;
							defClrFound = true;
							break;
						}
					}
					if (!defClrFound) {
						if (arg.size() > 0 && (arg[0] == L'#' || arg[0] == L'x'))
							curFont.m_color = strWToInt32u(arg.substr(1,16384).c_str(), 16);
                        else if (arg.size() > 1 && (arg[0] == L'0' || arg[1] == L'x'))
                            curFont.m_color = strWToInt32u(arg.substr(2,16384).c_str(), 16);
						else
							curFont.m_color = strWToInt32u(arg.c_str(), 10);
					}
                    if ((curFont.m_color & 0xff000000u) == 0)
                        curFont.m_color |= 0xff000000u;
				}
				size_t fontSizePos = findUnquotedStrW(ltagStr, L"size");
				if (fontSizePos != string::npos) {
					wstring arg = unquoteStrW(findFontArg(tagStr, fontSizePos));
					if (arg.size() > 0) {
						if (arg[0] == '+' || arg[0] == '-')
							curFont.m_size += strWToInt32(arg.c_str(), 10);
						else
							curFont.m_size = strWToInt32u(arg.c_str(), 10);
					}
				}
				isTag = true;
			}
			else if (strStartWithW(tagStr, L"/font")) 
			{
				endTag = true;
			}
			if (isTag || endTag) {
				if (bStartPos > prevTextPos) {
					wstring msg = line.substr(prevTextPos, bStartPos - prevTextPos);
					rez.push_back(make_pair(fontStack[fontStack.size()-1], msg));
				}
				if (isTag)
					fontStack.push_back(curFont);
				else if (fontStack.size() > 1) {
					fontStack.resize(fontStack.size()-1);
					curFont = fontStack[fontStack.size()-1];
				}
				prevTextPos = i + 1;
			}
			bStartPos = -1;
		}
	}
	if (line.size() > prevTextPos)
		rez.push_back(make_pair(curFont,line.substr(prevTextPos, line.size() - prevTextPos)));
	double rSize = m_initFont.m_size;
	for (int i = 0; i < rez.size(); i++) 
		rez[i].first.m_size = browserSizeToRealSize(rez[i].first.m_size, rSize);
	return rez;
}

bool TextSubtitlesRender::rasterText(const std::wstring& text)
{
    bool forced = false;
	memset(m_pData, 0, m_width * m_height * 4);
	vector<Font> fontStack;
	vector<wstring> lines = splitStrW(text.c_str(),'\n');
	int curY = 0;
	m_initFont = m_font;
	for (int i = 0; i < lines.size(); ++i)
	{
		vector<pair<Font,wstring> > txtParts = processTxtLine(lines[i], fontStack);
        for (int i = 0; i < txtParts.size(); ++i) {
            if (txtParts[i].first.m_opts & Font::FORCED)
                forced = true;
        }

		int ySize = 0;
		int tWidth = 0;
		int maxHeight = 0;
        int maxBaseLine = 0;
		vector<int> xSize;
		for (int j = 0; j < txtParts.size(); j++) 
		{
			setFont(txtParts[j].first);
			SIZE mSize;
			getTextSize(txtParts[j].second, &mSize);
			ySize = FFMAX(ySize, mSize.cy);
			maxHeight = FFMAX(maxHeight, getLineSpacing());
            maxBaseLine = FFMAX(maxBaseLine, getBaseline());
			xSize.push_back(mSize.cx);
			tWidth += mSize.cx;
		}
		int xOffs = (m_width - tWidth) / 2;
		int curX = 0;
		for (int j = 0; j < txtParts.size(); j++) 
		{
			Font font(txtParts[j].first);
			setFont(font);

			RECT rect = {curX + xOffs, curY + (maxHeight - getLineSpacing()) - (maxBaseLine - getBaseline()), m_width, m_height};
			drawText(txtParts[j].second, &rect);

			curX += xSize[j];
		}
		curY += ySize * m_font.m_lineSpacing;
	}
	flushRasterBuffer();
    //if (!isOutlineSupported())
	//    addBorder(m_font.m_borderWidth, m_pData, m_width, m_height);
	m_font = m_initFont;

    return forced;
}

const static uint32_t BORDER_COLOR = 0xff020202;
const static uint32_t BORDER_COLOR_TMP = RGB(0x1,0x1,0x1);

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
		uint32_t* dst = (uint32_t*) data;
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				if (*dst != 0 && *dst != BORDER_COLOR_TMP) {
					if (x > 0) {
						setBPoint(dst - 1);
						if (y > 0)
							setBPoint(dst - 1 - width);
						if (y < height-1)
							setBPoint(dst - 1 + width);
					}
					if (y > 0)
						setBPoint(dst - width);
					if (y < height-1)
						setBPoint(dst + width);
					if (x < width - 1) {
						setBPoint(dst + 1);
						if (y > 0)
							setBPoint(dst + 1 - width);
						if (y < height-1)
							setBPoint(dst + 1 + width);
					}

				}
				dst++;	
			}
		}
		dst = (uint32_t*) data;
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

}
