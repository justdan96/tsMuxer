#ifndef __UTFCONVERTER__H__
#define __UTFCONVERTER__H__

#include <types/types.h>

#include <string>

namespace UtfConverter
{
enum SourceFormat
{
    sfUnknown,
#ifdef _WIN32
    sfANSI,  // currently active code page (CP_ACP)
#endif
    sfUTF8,
    sfUTF16le,
    sfUTF16be,
    sfUTF32le,
    sfUTF32be
};

std::string toUtf8(const uint8_t* start, size_t widesize, SourceFormat srcFormat);
}  // namespace UtfConverter

#endif
