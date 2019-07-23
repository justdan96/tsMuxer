#if !defined(WIN32) || defined(WIN32_DEBUG_FREETYPE)

#ifdef WIN32
#pragma comment(lib, "../../freetype/lib/freetype.lib")
#endif

#include "textSubtitlesRenderFT.h"

#include "vod_common.h"
#include "vodCoreException.h"
#include "utf8Converter.h"
#include "math.h"
#include <fs/directory.h>
#include <map>
#include <algorithm>
#include "vod_common.h"

#ifdef WIN32
const static char FONT_ROOT[] = "c:/WINDOWS/Fonts"; // for debug only
#endif
#ifdef LINUX
const static char FONT_ROOT[] = "/usr/share/fonts/";
#endif
#ifdef MAC
const static char FONT_ROOT[] = "/Library/Fonts/";
#endif

#include <fs/systemlog.h>
#include <freetype/ftstroke.h>


using namespace std;

namespace text_subtitles 
{

FT_Library TextSubtitlesRenderFT::library; 
std::map<std::string, std::string> TextSubtitlesRenderFT::m_fontNameToFile;

const double PI = 3.1415926f;
const double angle = -PI / 10.0f;

uint32_t convertColor(uint32_t color)
{
	uint8_t* arr = (uint8_t*) &color;
	uint8_t tmp = arr[0];
	arr[0] = arr[2];
	arr[2] = tmp;
	return color;
}

TextSubtitlesRenderFT::TextSubtitlesRenderFT(): TextSubtitlesRender()
{
	static bool initialized = false;
	if (!initialized) {
		int error = FT_Init_FreeType( &library ); 
		if ( error ) 
			THROW(ERR_COMMON, "Can't initialize freeType font library");
		loadFontMap();
		initialized = true;
	}
    m_pData = 0;
	italic_matrix.xx = (FT_Fixed)( cos( angle ) * 0x10000L ); 
    italic_matrix.xy = (FT_Fixed)(-sin( angle ) * 0x10000L ); 
    italic_matrix.yx = (FT_Fixed)(0 * 0x10000L);
    italic_matrix.yy = (FT_Fixed)(1 * 0x10000L);

    bold_matrix.xx = (FT_Fixed)( 1.5 * 0x10000L ); 
    bold_matrix.xy = (FT_Fixed)( 0.0 * 0x10000L ); 
    bold_matrix.yx = (FT_Fixed)( 0.0 * 0x10000L);
    bold_matrix.yy = (FT_Fixed)( 1.0 * 0x10000L);

    italic_bold_matrix.xx = (FT_Fixed)( cos( angle ) * 1.5 * 0x10000L ); 
    italic_bold_matrix.xy = (FT_Fixed)(-sin( angle ) * 1.5 * 0x10000L ); 
    italic_bold_matrix.yx = (FT_Fixed)(0 * 0x10000L);
    italic_bold_matrix.yy = (FT_Fixed)(1 * 0x10000L);
}

void TextSubtitlesRenderFT::loadFontMap()
{
	vector<string> fileList;
	//sort(fileList.begin(), fileList.end());
	findFilesRecursive(FONT_ROOT, "*.ttf", &fileList);
	FT_Face font;
	for (int i = 0; i < fileList.size(); ++i)
	{
		//LTRACE(LT_INFO, 2, "before loading font " << fileList[i].c_str());
		if (strEndWith(fileList[i], "AppleMyungjo.ttf"))
		    continue;
		int error = FT_New_Face( library, fileList[i].c_str(), 0, &font); 
		if (error == 0)
		{
			string fontFamily = strToLowerCase ( font->family_name );
			
			std::map<std::string, std::string>::iterator itr = m_fontNameToFile.find(fontFamily);
			
			if (itr == m_fontNameToFile.end())
				m_fontNameToFile[fontFamily] = fileList[i];
			else if (fileList[i].length() < itr->second.length())
				m_fontNameToFile[fontFamily] = fileList[i];
		}
		FT_Done_Face(font);
		//LTRACE(LT_INFO, 2, "after loading font " << fileList[i].c_str());
	}
}

TextSubtitlesRenderFT::~TextSubtitlesRenderFT()
{
    delete m_pData;
}

void TextSubtitlesRenderFT::setRenderSize(int width, int height)
{
    m_width = width;
    m_height = height;
    delete m_pData;
    m_pData = new uint8_t[width * height * 4]; // 32bpp ARGB buffer
}

string TextSubtitlesRenderFT::findAdditionFontFile(const string& fontName, const string& fontExt,
						  bool isBold, bool isItalic)
{
	m_emulateItalic = false;
	m_emulateBold = false;
	if (isBold && isItalic) {
		if (fileExists(fontName + string("BI") + fontExt))
			return fontName + string("BI") + fontExt;
		else if (fileExists(fontName + string("Bi") + fontExt))
			return fontName + string("Bi") + fontExt;
		else if (fileExists(fontName + string("Bd") + fontExt)) {
			m_emulateItalic = true;
			return fontName + string("Bd") + fontExt;
		}
		else if (fileExists(fontName + string("B") + fontExt)) {
			m_emulateItalic = true;
			return fontName + string("B") + fontExt;
		}
		else if (fileExists(fontName + string("It") + fontExt)) {
			m_emulateBold = true;
			return fontName + string("It") + fontExt;
		}
		else if (fileExists(fontName + string("I") + fontExt)) {
			m_emulateBold = true;
			return fontName + string("I") + fontExt;
		}
		else {
			m_emulateBold = true;
			m_emulateItalic = true;
			return fontName + fontExt;
		}
	}
	else if (isBold) {
		if (fileExists(fontName + string("Bd") + fontExt)) {
			return fontName + string("Bd") + fontExt;
		}
		else if (fileExists(fontName + string("B") + fontExt)) {
			return fontName + string("B") + fontExt;
		}
		else {
			m_emulateBold = true;
			return fontName + fontExt;
		}
	}
	else if (isItalic) {
		if (fileExists(fontName + string("It") + fontExt)) {
			return fontName + string("It") + fontExt;
		}
		else if (fileExists(fontName + string("I") + fontExt)) {
			return fontName + string("I") + fontExt;
		}
		else {
			m_emulateItalic = true;
			return fontName + fontExt;
		}
	}
	else 
		return fontName + fontExt;
}

int TextSubtitlesRenderFT::loadFont(const string& fontName, FT_Face& face)
{
	map<string, FT_Face>::iterator itr = m_fontMap.find(fontName);
	if (itr == m_fontMap.end())
	{
		int error = FT_New_Face( library, fontName.c_str(), 0, &face ); 
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
		string fontName = UtfConverter::ToUtf8(font.m_name);
		if (!strEndWith(fontName, string(".ttf"))) 
		{
			std::string fontLower = strToLowerCase ( fontName );
			map<string, string>::iterator itr = m_fontNameToFile.find(fontLower);
			if (itr != m_fontNameToFile.end())
				fontName = itr->second;
			else 
				THROW(ERR_COMMON, "Can't find ttf file for font " << fontName);
		}
		string fileExt = extractFileExt(fontName);
		if (fileExt.length() > 0)
			fileExt = string(".") + fileExt;
		fontName = fontName.substr(0, fontName.length() - fileExt.length());
		string postfix;
		string modFontName = findAdditionFontFile(fontName, fileExt, font.m_opts & Font::BOLD, font.m_opts & Font::ITALIC);
   	    int error = loadFont(modFontName, face);
		if ( error == FT_Err_Unknown_File_Format ) {
			THROW(ERR_COMMON, "Unknown font format for file " << modFontName.c_str());
		}
		else if (error) {
			THROW(ERR_COMMON, "Can't load font file " << modFontName.c_str());
        }
        
		error = FT_Set_Char_Size ( 
	    face, /* handle to face object */  
	    0, /* char_width in 1/64th of points */  
	    font.m_size*64, /* char_height in 1/64th of points */  
	    64, /* horizontal device resolution */  
	    64 ); /* vertical device resolution */         
		if (error)
			THROW(ERR_COMMON, "Invalid font size " << font.m_size);

		m_line_thickness =  FT_MulFix(face->underline_thickness, face->size->metrics.y_scale) >> 6;
		if (m_line_thickness < 1)
			m_line_thickness = 1;
    	m_underline_position = -FT_MulFix(face->underline_position, face->size->metrics.y_scale);
		m_underline_position >>= 6;
	    	
    	if (m_font.m_charset) 
    	{
	    	
			FT_Encoding* encoding = (FT_Encoding*) &m_font.m_charset;
			error = FT_Select_Charmap( face, *encoding); 
			if (error)
			THROW(ERR_COMMON, "Can't load charset " << m_font.m_charset << " for font " << modFontName.c_str());
		}
	}
}

uint32_t grayColorToARGB(unsigned char gray, uint32_t color)
{
    uint32_t rez = color;
    uint8_t* bPtr = (uint8_t*) &rez;
	gray &= 0xF0;
	for (int i = 0; i < 4; ++i)
		bPtr[i] = (float) bPtr[i] * (float) gray / 240.0f + 0.5;
	return rez;
}

uint32_t clrMax(uint32_t first, uint32_t second)
{
	RGBQUAD* rgba = (RGBQUAD*) &first;
	RGBQUAD* rgba2 = (RGBQUAD*) &second;
	int first_yuv  = (0.257 * rgba->rgbRed) + (0.504 * rgba->rgbGreen) + (0.098 * rgba->rgbBlue) + 16;
	int second_yuv  = (0.257 * rgba2->rgbRed) + (0.504 * rgba2->rgbGreen) + (0.098 * rgba2->rgbBlue) + 16;
	return first_yuv > second_yuv ? first : second;
}

void TextSubtitlesRenderFT::drawHorLine(int left, int right, int top, RECT* rect)
{
	if (top < rect->top || top >= rect->bottom)
		return;
	if (left < rect->left)
		left = rect->left;
	if (right > rect->right)
		right = rect->right;
	uint32_t* dst = (uint32_t*) m_pData + m_width*top + left;
	for (int x = left; x <= right; ++x)
		*dst++ =  convertColor(m_font.m_color);
}

union Pixel32
{
    Pixel32(uint32_t val = 0) : integer(val) { }
    Pixel32(uint8_t bi, uint8_t gi, uint8_t ri, uint8_t ai = 255)
    {
        b = bi;
        g = gi;
        r = ri;
        a = ai;
    }

    uint32_t integer;

    struct
    {
#ifdef BIG_ENDIAN
        uint8_t a, r, g, b;
#else // BIG_ENDIAN
        uint8_t b, g, r, a;
#endif // BIG_ENDIAN
    };
};

struct Vec2
{
    Vec2() { }
    Vec2(float a, float b)
        : x(a), y(b) { }

    float x, y;
};


struct Rect
{
    Rect() { }
    Rect(float left, float top, float right, float bottom)
        : xmin(left), xmax(right), ymin(top), ymax(bottom) { }

    void Include(const Vec2 &r)
    {
        xmin = FFMIN(xmin, r.x);
        ymin = FFMIN(ymin, r.y);
        xmax = FFMAX(xmax, r.x);
        ymax = FFMAX(ymax, r.y);
    }

    float Width() const { return xmax - xmin + 1; }
    float Height() const { return ymax - ymin + 1; }

    float xmin, xmax, ymin, ymax;
};

struct Span
{
    Span() { }
    Span(int _x, int _y, int _width, int _coverage)
        : x(_x), y(_y), width(_width), coverage(_coverage) { }

    int x, y, width, coverage;
};

typedef std::vector<Span> Spans;


void
RasterCallback(const int y,
               const int count,
               const FT_Span * const spans,
               void * const user) 
{
    Spans *sptr = (Spans *)user;
    for (int i = 0; i < count; ++i) 
        sptr->push_back(Span(spans[i].x, y, spans[i].len, spans[i].coverage));
}

void
RenderSpans(FT_Library &library,
            FT_Outline * const outline,
            Spans *spans) 
{
    FT_Raster_Params params;
    memset(&params, 0, sizeof(params));
    params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
    params.gray_spans = RasterCallback;
    params.user = spans;

    FT_Outline_Render(library, outline, &params);
}

void
RenderGlyph(FT_Library &library,
                wchar_t ch,
                FT_Face &face,
                int size,
                const Pixel32 &fontCol,
                const Pixel32 outlineColOut,
                const Pixel32 outlineColIner,
                float outlineWidth,
                int left, int top, 
                int width, int height,
                uint32_t* dstData)
{
    // Load the glyph we are looking for.
    FT_UInt gindex = FT_Get_Char_Index(face, ch);
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
        FT_Stroker_Set(strokerOut, (int)(outlineWidth * 64), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Glyph_StrokeBorder(&glyph, strokerOut, 0, 1);
        // Again, this needs to be an outline to work.
        if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            // Render the outline spans to the span list
            FT_Outline *o = &reinterpret_cast<FT_OutlineGlyph>(glyph)->outline;
            RenderSpans(library, o, &outlineSpansOut);
        }
        FT_Stroker_Done(strokerOut); // Clean up afterwards.
        FT_Done_Glyph(glyph);

        FT_Get_Glyph(face->glyph, &glyph);
        FT_Stroker strokerIn;
        FT_Stroker_New(library, &strokerIn);
        FT_Stroker_Set(strokerIn, (int)(outlineWidth * 32), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Glyph_StrokeBorder(&glyph, strokerIn, 0, 1);
        if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            FT_Outline *o = &reinterpret_cast<FT_OutlineGlyph>(glyph)->outline;
            RenderSpans(library, o, &outlineSpansIner);
        }
        FT_Stroker_Done(strokerIn);

        // Now we need to put it all together.
        if (!spans.empty())
        {
            // Figure out what the bounding rect is for both the span lists.
            Rect rect(spans.front().x, spans.front().y, spans.front().x, spans.front().y);
            for (Spans::iterator s = spans.begin(); s != spans.end(); ++s)
            {
                rect.Include(Vec2(s->x, s->y));
                rect.Include(Vec2(s->x + s->width - 1, s->y));
            }
            for (Spans::iterator s = outlineSpansOut.begin(); s != outlineSpansOut.end(); ++s)
            {
                rect.Include(Vec2(s->x, s->y));
                rect.Include(Vec2(s->x + s->width - 1, s->y));
            }
            for (Spans::iterator s = outlineSpansIner.begin(); s != outlineSpansIner.end(); ++s)
            {
                rect.Include(Vec2(s->x, s->y));
                rect.Include(Vec2(s->x + s->width - 1, s->y));
            }

            // This is unused in this test but you would need this to draw
            // more than one glyph.
            float bearingX = face->glyph->metrics.horiBearingX >> 6;
            float bearingY = face->glyph->metrics.horiBearingY >> 6;
            float advance = face->glyph->advance.x >> 6;

            // Get some metrics of our image.
            int imgWidth = rect.Width(), imgHeight = rect.Height(), imgSize = imgWidth * imgHeight;

            top += (face->size->metrics.ascender >> 6) - bearingY;
            left += bearingX;

            // Loop over the outline spans and just draw them into the image.
            for (Spans::iterator s = outlineSpansOut.begin(); s != outlineSpansOut.end(); ++s)
            {
                for (int w = 0; w < s->width; ++w)
                {
                    int y = imgHeight - 1 - (s->y - rect.ymin) + top;
                    int x = s->x - rect.xmin + w + left;
                    if (y >= 0 && y < height && x >= 0 && x < width) 
                    {
                        Pixel32* dst = (Pixel32*) dstData + y * width + x;
                        if (*(uint32_t*)dst == 0)
                            *dst = Pixel32(outlineColOut.r, outlineColOut.g, outlineColOut.b, s->coverage * outlineColOut.a / 255.0);
                    }
                }
            }

            // Loop over the outline spans and just draw them into the image.
            for (Spans::iterator s = outlineSpansIner.begin(); s != outlineSpansIner.end(); ++s)
            {
                for (int w = 0; w < s->width; ++w)
                {
                    int y = imgHeight - 1 - (s->y - rect.ymin) + top;
                    int x = s->x - rect.xmin + w + left;
                    if (y >= 0 && y < height && x >= 0 && x < width) 
                    {
                        Pixel32* dst = (Pixel32*) dstData + y * width + x;
                        *dst = Pixel32(outlineColIner.r, outlineColIner.g, outlineColIner.b, s->coverage);
                    }
                }
            }

            // Then loop over the regular glyph spans and blend them into the image.
            for (Spans::iterator s = spans.begin(); s != spans.end(); ++s)
            {
                for (int w = 0; w < s->width; ++w)
                {
                    int y = imgHeight - 1 - (s->y - rect.ymin) + top;
                    int x = s->x - rect.xmin + w + left;

                    if (y >= 0 && y < height && x >= 0 && x < width)
                    {
                        Pixel32* dst = (Pixel32*) dstData + y * width + x;

                        Pixel32 src = Pixel32(fontCol.r, fontCol.g, fontCol.b, s->coverage);
                        dst->r = (int)(dst->r + ((src.r - dst->r) * src.a) / 255.0f);
                        dst->g = (int)(dst->g + ((src.g - dst->g) * src.a) / 255.0f);
                        dst->b = (int)(dst->b + ((src.b - dst->b) * src.a) / 255.0f);
                        dst->a = FFMIN(255, dst->a + src.a);
                    }
                }
            }
        }
    }
}

void TextSubtitlesRenderFT::drawText(const wstring& text, RECT* rect)
{
    FT_Vector pen;     
    pen.x = rect->left;
    pen.y = rect->top;
	if (rect->left < 0)
		rect->left = 0;
	if (rect->top < 0)
		rect->top = 0;

    int use_kerning = FT_HAS_KERNING( face ); 
    int maxX = 0;
    int firstOffs = 0;

    if (m_emulateItalic && m_emulateBold)
        FT_Set_Transform( face, &italic_bold_matrix, &pen ); 
    else if (m_emulateItalic)
        FT_Set_Transform( face, &italic_matrix, &pen ); 
    else if (m_emulateBold)
        FT_Set_Transform( face, &bold_matrix, &pen ); 
    else
        FT_Set_Transform( face, 0, &pen ); 

    uint8_t alpha = m_font.m_color >> 24;
    uint8_t outColor = (float)alpha / 255.0 * 48.0 + 0.5;
    for(int i = 0; i < text.length(); ++i)
    {
        RenderGlyph(library,
            text.at(i),
            face,
            m_font.m_size,
            m_font.m_color,
            Pixel32(0, 0, 0, outColor),
            Pixel32(0, 0, 0, alpha),
            m_font.m_borderWidth,
            pen.x, 
            pen.y,
            rect->right, rect->bottom, (uint32_t*) m_pData);

		pen.x += face->glyph->advance.x >> 6; 
        pen.x += m_font.m_borderWidth/2;
        if (m_emulateBold || m_emulateItalic)
            pen.x += m_line_thickness-1;
		maxX = pen.x + face->glyph->bitmap_left;
    }
	if ((m_font.m_opts & m_font.UNDERLINE) || (m_font.m_opts & m_font.STRIKE_OUT))
	{
		//SIZE size;
		//getTextSize(text, &size);
		if (m_font.m_opts & m_font.UNDERLINE)
		{
			int y = pen.y + (face->size->metrics.ascender >> 6) + m_underline_position - m_line_thickness/2;
			for (int i = 0; i < m_line_thickness; ++i)
				drawHorLine(rect->left, /*rect->left + size.cx*/ maxX, y + i, rect);
		}
		if (m_font.m_opts & m_font.STRIKE_OUT) {
			int y = pen.y + (face->size->metrics.ascender >> 6) * 2.0/3.0 - m_line_thickness/2;
			for (int i = 0; i < m_line_thickness; ++i)
				drawHorLine(rect->left, /*rect->left + size.cx*/pen.x, y + i, rect);
		}
	}
}

void TextSubtitlesRenderFT::getTextSize(const wstring& text, SIZE* mSize)
{

    FT_Vector pen;     
    pen.x = 0;
    pen.y = 0;
    int use_kerning = FT_HAS_KERNING( face ); 
	mSize->cy = mSize->cx = 0;

    if (m_emulateItalic && m_emulateBold)
        FT_Set_Transform( face, &italic_bold_matrix, &pen ); 
    else if (m_emulateItalic)
        FT_Set_Transform( face, &italic_matrix, &pen ); 
    else if (m_emulateBold)
        FT_Set_Transform( face, &bold_matrix, &pen ); 
    else
        FT_Set_Transform( face, 0, &pen ); 

    for(int i = 0; i < text.length(); ++i)
    {
        int glyph_index = FT_Get_Char_Index( face, text.at(i) );
		int error = FT_Load_Glyph( face, glyph_index, 0);
		if (error)
		    THROW(ERR_COMMON, "Can't load symbol code '" << text.at(i) << "' from font");

        error = FT_Render_Glyph( face->glyph, FT_RENDER_MODE_NORMAL);
        if (error)
    	    THROW(ERR_COMMON, "Can't render symbol code '" << text.at(i) << "' from font");
		pen.x += face->glyph->advance.x >> 6; 
		if (m_emulateBold || m_emulateItalic)
			pen.x += m_line_thickness-1;
        pen.x += m_font.m_borderWidth/2;
		mSize->cy = face->size->metrics.height >> 6;
		mSize->cx = pen.x + face->glyph->bitmap_left;
    }
}

int TextSubtitlesRenderFT::getLineSpacing()
{
	return face->size->metrics.height >> 6;
}

int TextSubtitlesRenderFT::getBaseline()
{
    return abs(face->size->metrics.descender) >> 6;
}


void TextSubtitlesRenderFT::flushRasterBuffer()
{
    return;
}

}

#endif
