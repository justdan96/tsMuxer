#include "types.h"

#include <cstring>
#include <iomanip>
#include <ostream>
#include <regex>
#include <sstream>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#endif

#include <algorithm>
#include <random>

#include "fs/directory.h"

namespace
{
const std::regex& invalidChars()
{
    static const std::regex invalid(
#ifndef _WIN32
        // / and ASCII 0 to 31
        "[/\\x00-\\x1F]"
#else
        // <>:"/|?\*, ASCII 0 to 31 and all reserved names such as CON or LPT1
        // see here: https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file#naming-conventions
        R"([:<>"/|?\*\x00-\x1F]|^CON$|^PRN$|^AUX$|^NUL$|^COM\d$|^LPT\d$)"
#endif
        ,
        std::regex_constants::ECMAScript | std::regex_constants::optimize);
    return invalid;
}
}  // namespace

using namespace std;

uint64_t my_ntohll(const uint64_t& original)
{
#ifdef SPARC_V9  // big endian
    return original;
#else  // little endian
    return (static_cast<uint64_t>(my_ntohl(static_cast<uint32_t>(original))) << 32) |
           static_cast<uint64_t>(my_ntohl(static_cast<uint32_t>(original >> 32)));
#endif
}

uint64_t my_htonll(const uint64_t& original)
{
#ifdef SPARC_V9  // big endian
    return original;
#else  // little endian
    return (static_cast<uint64_t>(my_ntohl(static_cast<uint32_t>(original))) << 32) |
           static_cast<uint64_t>(my_ntohl(static_cast<uint32_t>(original >> 32)));
#endif
}

// Simple types conversion
int64_t strToInt64(const char* const str)
{
#ifdef _WIN32
    return _atoi64(str);
#else
    return strtoll(str, 0, 10);
#endif
}

uint64_t strToInt64u(const char* const str)
{
#ifdef _WIN32
    return _atoi64(str);
#else
    return strtoull(str, 0, 10);
#endif
}

int32_t strToInt32(const char* const str) { return strtol(str, nullptr, 10); }

int32_t strToInt32(const std::string& str) { return strToInt32(str.c_str()); }

int32_t strToInt32(const char* const str, const int radix) { return strtol(str, nullptr, radix); }

uint32_t strToInt32u(const char* const str, const int radix)
{
    return static_cast<uint32_t>(strtoul(str, nullptr, radix));
}

int16_t strToInt16(const char* const str) { return static_cast<int16_t>(strtol(str, nullptr, 10)); }

uint16_t strToInt16u(const char* const str) { return static_cast<uint16_t>(strtol(str, nullptr, 10)); }

int8_t strToInt8(const char* const str) { return static_cast<int8_t>(strtol(str, nullptr, 10)); }

uint8_t strToInt8u(const char* const str) { return static_cast<uint8_t>(strtol(str, nullptr, 10)); }

double strToDouble(const char* const str) { return strtod(str, nullptr); }

float strToFloat(const char* const str) { return strtof(str, nullptr); }

bool strToBool(const char* const str)
{
    if (!strcmp(str, "true"))
        return true;

    return false;
}

string int64ToStr(const int64_t& x)
{
    std::ostringstream str;
    str << x;
    return str.str();
}

string int64uToStr(const uint64_t& x)
{
    std::ostringstream str;
    str << x;
    return str.str();
}

string int32ToStr(const int32_t& x)
{
    std::ostringstream str;
    str << x;
    return str.str();
}

string int32ToHex(const int32_t& x)
{
    std::ostringstream str;
    str << std::hex << x;
    return str.str();
}

string int32uToStr(const uint32_t& x)
{
    std::ostringstream str;
    str << x;
    return str.str();
}

string int32uToHex(const uint32_t& x)
{
    std::ostringstream str;
    str << std::hex << x;
    return str.str();
}

string doubleToStr(const double& x, const int precision)
{
    std::ostringstream str;
    if (precision > 0)
        str << fixed << std::setprecision(precision);
    str << x;
    return str.str();
}

string int16ToStr(const int16_t& x)
{
    std::ostringstream str;
    str << x;
    return str.str();
}

string int16uToStr(const uint16_t& x)
{
    std::ostringstream str;
    str << x;
    return str.str();
}

string int8ToStr(const int8_t& x)
{
    std::ostringstream str;
    str << x;
    return str.str();
}

string int8uToStr(const uint8_t& x)
{
    std::ostringstream str;
    str << x;
    return str.str();
}

string boolToStr(const bool& x) { return x ? "true" : "false"; }

uint32_t roundDown(const uint32_t& value, const uint32_t& roundVal)
{
    return roundVal ? (value / roundVal) * roundVal : 0;
}

uint32_t roundUp(const uint32_t& value, const uint32_t& roundVal)
{
    return roundVal ? ((value + roundVal - 1) / roundVal) * roundVal : 0;
}

uint64_t roundDown64(const uint64_t& value, const uint64_t& roundVal)
{
    return roundVal ? (value / roundVal) * roundVal : 0;
}

uint64_t roundUp64(const uint64_t& value, const uint64_t& roundVal)
{
    return roundVal ? ((value + roundVal - 1) / roundVal) * roundVal : 0;
}

string strPadLeft(const string& str, const size_t newSize, const char filler)
{
    const int cnt = static_cast<int>(newSize - str.size());
    string prefix;
    for (int i = 0; i < cnt; i++) prefix += filler;
    return prefix + str;
}

string strPadRight(const string& str, const size_t newSize, const char filler)
{
    const int cnt = static_cast<int>(newSize - str.size());
    string postfix;
    for (int i = 0; i < cnt; i++) postfix += filler;
    return str + postfix;
}

bool strEndWith(const string& str, const string& substr)
{
    if (str.empty())
        return false;
    size_t idx = str.size();
    for (size_t i = substr.size(); i-- > 0;)
        if (substr[i] != str[--idx])
            return false;
    return true;
}

bool strStartWith(const string& str, const string& substr)
{
    if (str.size() < substr.size())
        return false;
    for (size_t i = 0; i < substr.size(); i++)
        if (substr[i] != str[i])
            return false;
    return true;
}

vector<string> splitStr(const char* str, const char splitter)
{
    vector<string> rez;
    const char* prevPos = str;
    const char* buf = str;
    for (; *buf; buf++)
    {
        if (*buf == splitter)
        {
            rez.push_back(string(prevPos, buf - prevPos));
            prevPos = buf + 1;
        }
    }
    if (buf > prevPos)
        rez.push_back(string(prevPos, buf - prevPos));
    return rez;
}

vector<string> splitStr(const string& str, const string& splitter)
{
    vector<string> res;

    const size_t splitterSize = splitter.size();
    size_t posBegin = 0;
    size_t posEnd;

    if (splitterSize > 0 && !str.empty())
    {
        while ((posEnd = str.find(splitter, posBegin)) != string::npos)
        {
            res.push_back(str.substr(posBegin, posEnd - posBegin));
            posBegin = posEnd + splitterSize;
        }

        // if ( res.size() == 0 )
        res.push_back(str.substr(posBegin, str.size()));
    }

    return res;
}

void splitStr(vector<string>& rez, const char* str, const char splitter)
{
    rez.clear();
    const char* prevPos = str;
    const char* buf = str;
    for (; *buf; buf++)
    {
        if (*buf == splitter)
        {
            rez.push_back(string(prevPos, buf - prevPos));
            prevPos = buf + 1;
        }
    }
    if (buf > prevPos)
        rez.push_back(string(prevPos, buf - prevPos));
}

string extractFileExt(const string& src)
{
    for (size_t i = src.size() - 1; i > 0; i--)
        if (src[i] == '.')
        {
            string rez = src.substr(i + 1);
            if (!rez.empty() && rez[rez.size() - 1] == '\"')
                return rez.substr(0, rez.size() - 1);
            return rez;
        }
    return "";
}

string extractFileName(const string& src)
{
    size_t endPos = src.size();
    for (size_t i = src.size(); i-- > 0;)
        if (src[i] == '.')
        {
            if (endPos == src.size())
                endPos = i;
        }
        else if (src[i] == '/' || src[i] == '\\')
        {
            string rez = src.substr(i + 1, endPos - i - 1);
            if (!rez.empty() && rez[rez.size() - 1] == '\"')
                return rez.substr(0, rez.size() - 1);
            return rez;
        }
    return "";
}

string extractFileName2(const string& src, const bool withExt)
{
    string fileName = src;

    const size_t extSep = fileName.find_last_of('.');
    size_t dirSep = fileName.find_last_of('/');

    if (dirSep == string::npos)
        dirSep = fileName.find_last_of('\\');

    if (extSep != string::npos && !withExt)
        fileName = fileName.substr(0, extSep);

    if (dirSep != string::npos)
        fileName = fileName.substr(dirSep + 1, fileName.size());

    return fileName;
}

string extractFilePath(const string& src)
{
    for (size_t i = src.size(); i-- > 0;)
        if (src[i] == '/' || src[i] == '\\')
        {
            string rez = src.substr(0, i);
            return rez;
        }
    return "";
}

string closeDirPath(const string& src, char delimiter)
{
    if (delimiter == ' ')
        delimiter = getDirSeparator();
    if (src.length() == 0)
        return src;
    if (src[src.length() - 1] == '/' || src[src.length() - 1] == '\\')
        return src;
    return src + delimiter;
}

// extract the filename from a path, check for invalid characters
bool isValidFileName(const string& src)
{
    const string filename = extractFileName(src);

    // invalidChars() returns a different regex pattern for Windows or Unix
    const bool isvalid = !(std::regex_search(filename, invalidChars()));
    return isvalid;
}

string trimStr(const string& value)
{
    const char* bufStart = value.c_str();
    const char* bufEnd = bufStart + value.length() - 1;
    const char* chBeg = bufStart;
    const char* chEnd = bufEnd;
    for (; chBeg < bufEnd && (*chBeg == '\n' || *chBeg == '\r' || *chBeg == ' '); chBeg++)
        ;
    for (; chEnd >= chBeg && (*chEnd == '\n' || *chEnd == '\r' || *chEnd == ' '); chEnd--)
        ;
    return value.substr(chBeg - bufStart, chEnd - chBeg + 1);
}

vector<string> splitQuotedStr(const char* str, const char splitter)
{
    vector<string> rez;
    const char* prevPos = str;
    const char* buf = str;
    bool quoted = false;
    for (; *buf; buf++)
    {
        if (*buf == '"')
            quoted = !quoted;
        if (*buf == splitter && !quoted)
        {
            rez.push_back(string(prevPos, buf - prevPos));
            prevPos = buf + 1;
        }
    }
    if (buf > prevPos)
        rez.push_back(string(prevPos, buf - prevPos));
    return rez;
}

string strToUpperCase(const string& src)
{
    string res = src;

    transform(res.begin(), res.end(), res.begin(), CaseChanger<string>(CaseType::ctUpper));

    return res;
}

string strToLowerCase(const string& src)
{
    string res = src;

    transform(res.begin(), res.end(), res.begin(), CaseChanger<string>(CaseType::ctLower));

    return res;
}

uint32_t my_ntohl(uint32_t val)
{
    const auto* tmp = reinterpret_cast<uint8_t*>(&val);
    return tmp[3] + (tmp[2] << 8) + (tmp[1] << 16) + (tmp[0] << 24);
}

uint16_t my_ntohs(uint16_t val)
{
    const auto* tmp = reinterpret_cast<uint8_t*>(&val);
    return static_cast<uint16_t>(tmp[1] | tmp[0] << 8);
}

char* strnstr(char* s1, const char* s2, const size_t len)
{
    size_t l1 = len;
    const size_t l2 = strlen(s2);
    if (!l2)
        return s1;
    while (l1 >= l2)
    {
        l1--;
        if (!memcmp(s1, s2, l2))
            return s1;
        s1++;
    }
    return nullptr;
}

uint32_t random32()
{
    static std::random_device dev;
    static std::minstd_rand raand(dev());
    return raand();
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace
{
std::vector<wchar_t> mbtwc_wrapper(const int codePage, const char* inputStr, const int inputSize, const int outputSize)
{
    std::vector<wchar_t> multiByteBuf(static_cast<std::size_t>(outputSize));
    MultiByteToWideChar(codePage, 0, inputStr, inputSize, multiByteBuf.data(), outputSize);
    if (multiByteBuf.empty() || multiByteBuf.back() != 0)
    {
        multiByteBuf.push_back(0);
    }
    return multiByteBuf;
}
}  // namespace

std::vector<wchar_t> fromAcp(const char* acpStr, const int sz)
{
    const auto requiredSiz = MultiByteToWideChar(CP_ACP, 0, acpStr, sz, nullptr, 0);
    return mbtwc_wrapper(CP_ACP, acpStr, sz, requiredSiz);
}

std::vector<wchar_t> toWide(const std::string& utf8Str)
{
    return toWide(utf8Str.c_str(), static_cast<int>(utf8Str.size()));
}

std::vector<wchar_t> toWide(const char* utf8Str, const int sz)
{
    const auto requiredSiz = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Str, sz, nullptr, 0);
    if (requiredSiz != 0)
    {
        return mbtwc_wrapper(CP_UTF8, utf8Str, sz, static_cast<std::size_t>(requiredSiz));
    }
    /* utf8Str is not a valid UTF-8 string. try converting it according to the currently active code page in order
     * to keep compatibility with meta files saved by older versions of the GUI which put the file name through
     * QString::toLocal8Bit, which uses the ACP on Windows. */
    return fromAcp(utf8Str, sz);
}

std::string toUtf8(const wchar_t* wideStr)
{
    auto needed = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    needed--;  // includes terminating null byte, needless when returning a std::string.
    std::string s(static_cast<std::size_t>(needed), 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, s.data(), needed, nullptr, nullptr);
    return s;
}
#endif
