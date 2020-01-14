#ifndef __UTFCONVERTER__H__
#define __UTFCONVERTER__H__

#include <types/types.h>

#include <string>

namespace UtfConverter
{
enum SourceFormat
{
    sfUnknown,
    sfANSI,
    sfDefault,
    sfUTF8,
    sfUTF16le,
    sfUTF16be,
    sfUTF32le,
    sfUTF32be
};

std::wstring toWideString(uint8_t* start, size_t widesize, SourceFormat srcFormat);
std::wstring FromUtf8(const std::string& utf8string);
std::string ToUtf8(const std::wstring& widestring);
}  // namespace UtfConverter

#endif
