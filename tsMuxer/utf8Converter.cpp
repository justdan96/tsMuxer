#include "utf8Converter.h"

#include <fs/systemlog.h>
#include <types/types.h>

#include <type_traits>

#include "convertUTF.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace convertUTF;

namespace
{
uint16_t read_be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
uint16_t read_le16(const uint8_t *p) { return p[0] | (p[1] << 8); }
uint32_t read_be32(const uint8_t *p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
uint32_t read_le32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

template <typename R, typename... A>
R get_fn_ret_type(R (*)(A...));

template <typename F>
auto make_vector(const uint8_t *start, size_t numBytes, F func) -> std::vector<decltype(get_fn_ret_type(func))>
{
    std::vector<decltype(get_fn_ret_type(func))> rv;
    const auto elem_size = sizeof(typename decltype(rv)::value_type);
    if (numBytes % elem_size)
    {
        THROW(ERR_COMMON, "Cannot convert string : size " << numBytes << " should be divisible by " << elem_size);
    }
    rv.reserve(numBytes);
    const auto end = (start + numBytes);
    while (start != end)
    {
        rv.emplace_back(func(start));
        start += elem_size;
    }
    return rv;
}

template <typename InputType, typename F>
std::string from_utf_nn(std::vector<InputType> &&vec, F conversionFn)
{
    std::string rv;
    rv.resize(vec.size() * 4);
    const InputType *sourceStart = vec.data();
    auto sourceEnd = sourceStart + vec.size();
    auto targetStart = reinterpret_cast<UTF8 *>(&rv[0]);
    auto targetStart_out = targetStart;
    auto targetEnd = targetStart + rv.size();
    auto result = conversionFn(&sourceStart, sourceEnd, &targetStart_out, targetEnd, strictConversion);
    if (result != conversionOK)
    {
        THROW(ERR_COMMON, "Cannot convert string : invalid source text");
    }
    rv.resize(targetStart_out - targetStart);
    return rv;
}
}  // namespace

namespace UtfConverter
{
std::string toUtf8(const uint8_t *start, size_t numBytes, SourceFormat srcFormat)
{
    switch (srcFormat)
    {
    case sfUTF8:
        return std::string(start, start + numBytes);
    case sfUTF16be:
        return from_utf_nn(make_vector(start, numBytes, read_be16), ConvertUTF16toUTF8);
    case sfUTF16le:
        return from_utf_nn(make_vector(start, numBytes, read_le16), ConvertUTF16toUTF8);
    case sfUTF32be:
        return from_utf_nn(make_vector(start, numBytes, read_be32), ConvertUTF32toUTF8);
    case sfUTF32le:
        return from_utf_nn(make_vector(start, numBytes, read_le32), ConvertUTF32toUTF8);
#ifdef _WIN32
    case sfANSI:
    {
        return ::toUtf8(fromAcp(reinterpret_cast<const char *>(start), numBytes).data());
    }
#endif
    default:
        THROW(ERR_COMMON, "Unknown parameter to UtfConverter::toUtf8");
    }
}
}  // namespace UtfConverter
