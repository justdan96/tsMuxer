#include "utf8Converter.h"

#include <fs/systemlog.h>
#include <types/types.h>

#include "convertUTF.h"
#include "memory.h"
#include "vodCoreException.h"
#include "vod_common.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <iconv.h>
#include <locale.h>
static iconv_t cd = 0;
static std::string sourceEncoding;
#endif

namespace UtfConverter
{
std::wstring FromUtf8(const std::string& utf8string)
{
    size_t widesize = utf8string.length();
    if (sizeof(wchar_t) == 2)
    {
        wchar_t* widestringnative = new wchar_t[widesize + 1];
        const UTF8* sourcestart = reinterpret_cast<const UTF8*>(utf8string.c_str());
        const UTF8* sourceend = sourcestart + widesize;
        UTF16* targetstart = reinterpret_cast<UTF16*>(widestringnative);
        UTF16* targetend = targetstart + widesize + 1;
        ConversionResult res = ConvertUTF8toUTF16(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
        if (res != conversionOK)
        {
            delete[] widestringnative;
            // throw std::exception("La falla!");
            THROW(ERR_COMMON, "Can't convert UTF-8 to UTF-16. Invalid source text");
        }
        *targetstart = 0;
        std::wstring resultstring(widestringnative);
        delete[] widestringnative;
        return resultstring;
    }
    else if (sizeof(wchar_t) == 4)
    {
        wchar_t* widestringnative = new wchar_t[widesize + 1];
        const UTF8* sourcestart = reinterpret_cast<const UTF8*>(utf8string.c_str());
        const UTF8* sourceend = sourcestart + widesize;
        UTF32* targetstart = reinterpret_cast<UTF32*>(widestringnative);
        UTF32* targetend = targetstart + widesize;
        ConversionResult res = ConvertUTF8toUTF32(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
        if (res != conversionOK)
        {
            delete[] widestringnative;
            // throw std::exception("La falla!");
            THROW(ERR_COMMON, "Can't convert UTF-8 to UTF-32. Invalid source text");
        }
        *targetstart = 0;
        std::wstring resultstring(widestringnative);
        delete[] widestringnative;
        return resultstring;
    }
    else
    {
        THROW(ERR_COMMON, "Unsupported wchar_t size");
    }
    return L"";
}

std::wstring toWideString(uint8_t* start, size_t widesize, SourceFormat srcFormat)
{
    if (widesize == 0)
        return L"";
#ifdef _WIN32
    else if (srcFormat == sfANSI)
    {
        wchar_t* widestringnative = new wchar_t[widesize + 1];
        int wlen = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)start, widesize, widestringnative, widesize);
        widestringnative[wlen] = 0;
        std::wstring resultstring(widestringnative);
        delete[] widestringnative;
        return resultstring;
    }
#endif
    else if (sizeof(wchar_t) == 2)
    {
        wchar_t* widestringnative = new wchar_t[widesize + 1];
        const UTF8* sourcestart = reinterpret_cast<const UTF8*>(start);
        const UTF8* sourceend = sourcestart + widesize;
        UTF16* targetstart = reinterpret_cast<UTF16*>(widestringnative);
        UTF16* targetend = targetstart + widesize + 1;
        ConversionResult res = conversionOK;
        switch (srcFormat)
        {
        case sfUnknown:
        case sfUTF8:
            res = ConvertUTF8toUTF16(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            *targetstart = 0;
            break;
        case sfUTF16be:
            for (uint16_t* ptr = (uint16_t*)start; ptr < (uint16_t*)sourceend; ++ptr) *ptr = my_ntohs(*ptr);
        case sfUTF16le:
            memcpy(widestringnative, start, widesize);
            widestringnative[widesize / 2] = 0;
            break;
        case sfUTF32be:
            for (uint32_t* ptr = (uint32_t*)start; ptr < (uint32_t*)sourceend; ++ptr) *ptr = my_ntohl(*ptr);
        case sfUTF32le:
            res = ConvertUTF32toUTF16((const UTF32**)&sourcestart, (const UTF32*)sourceend, &targetstart, targetend,
                                      strictConversion);
            *targetstart = 0;
            break;
        }

        if (res != conversionOK)
        {
            delete[] widestringnative;
            std::string msg((const char*)sourcestart, widesize);
            THROW(ERR_COMMON, "Can't convert source text to UTF-16. Invalid chars sequence: " << msg);
        }
        std::wstring resultstring(widestringnative);
        delete[] widestringnative;
        return resultstring;
    }
    else if (sizeof(wchar_t) == 4)
    {
        wchar_t* widestringnative = new wchar_t[widesize + 1];
        const UTF8* sourcestart = reinterpret_cast<const UTF8*>(start);
        const UTF8* sourceend = sourcestart + widesize;
        UTF32* targetstart = reinterpret_cast<UTF32*>(widestringnative);
        UTF32* targetend = targetstart + widesize;
        // ConversionResult res;
        //= ConvertUTF8toUTF32(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
        ConversionResult res = conversionOK;
        switch (srcFormat)
        {
        case sfUnknown:
        case sfUTF8:
            res = ConvertUTF8toUTF32(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
            *targetstart = 0;
            break;
        case sfUTF16be:
            for (uint16_t* ptr = (uint16_t*)start; ptr < (uint16_t*)sourceend; ++ptr) *ptr = my_ntohs(*ptr);
        case sfUTF16le:
            res = ConvertUTF16toUTF32((const UTF16**)&sourcestart, (const UTF16*)sourceend, &targetstart, targetend,
                                      strictConversion);
            *targetstart = 0;
            break;
        case sfUTF32be:
            for (uint32_t* ptr = (uint32_t*)start; ptr < (uint32_t*)sourceend; ++ptr) *ptr = my_ntohl(*ptr);
        case sfUTF32le:
            memcpy(widestringnative, start, widesize);
            widestringnative[widesize / 4] = 0ul;
            break;
        }

        if (res != conversionOK)
        {
            delete[] widestringnative;
            THROW(ERR_COMMON, "Can't convert source text to UTF-32. Invalid chars sequence.");
        }
        std::wstring resultstring(widestringnative);
        delete[] widestringnative;
        return resultstring;
    }
    else
    {
        THROW(ERR_COMMON, "Unsupported wchar_t size");
    }
    return L"";
}

std::string ToUtf8(const std::wstring& widestring)
{
    size_t widesize = widestring.length();

    if (sizeof(wchar_t) == 2)
    {
        size_t utf8size = 3 * widesize + 1;
        char* utf8stringnative = new char[utf8size];
        const UTF16* sourcestart = reinterpret_cast<const UTF16*>(widestring.c_str());
        const UTF16* sourceend = sourcestart + widesize;
        UTF8* targetstart = reinterpret_cast<UTF8*>(utf8stringnative);
        UTF8* targetend = targetstart + utf8size;
        ConversionResult res = ConvertUTF16toUTF8(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
        if (res != conversionOK)
        {
            delete[] utf8stringnative;
            THROW(ERR_COMMON, "Can't convert UTF16 to UTF8 string");
        }
        *targetstart = 0;
        std::string resultstring(utf8stringnative);
        delete[] utf8stringnative;
        return resultstring;
    }
    else if (sizeof(wchar_t) == 4)
    {
        size_t utf8size = 4 * widesize + 1;
        char* utf8stringnative = new char[utf8size];
        const UTF32* sourcestart = reinterpret_cast<const UTF32*>(widestring.c_str());
        const UTF32* sourceend = sourcestart + widesize;
        UTF8* targetstart = reinterpret_cast<UTF8*>(utf8stringnative);
        UTF8* targetend = targetstart + utf8size;
        ConversionResult res = ConvertUTF32toUTF8(&sourcestart, sourceend, &targetstart, targetend, strictConversion);
        if (res != conversionOK)
        {
            delete[] utf8stringnative;
            THROW(ERR_COMMON, "Can't convert UTF32 to UTF8 string");
        }
        *targetstart = 0;
        std::string resultstring(utf8stringnative);
        delete[] utf8stringnative;
        return resultstring;
    }
    else
    {
        THROW(ERR_COMMON, "Unsupported wchar_t size " << sizeof(wchar_t));
    }
    return "";
}
}  // namespace UtfConverter
