#include "types.h"

#include <memory.h>

#include <cstring>
#include <iomanip>
#include <ostream>
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

using namespace std;

uint64_t my_ntohll(const uint64_t& original)
{
#ifdef SPARC_V9  // big endian
    return original;
#else  // little endian
    return (((uint64_t)my_ntohl((uint32_t)original)) << 32) | ((uint64_t)my_ntohl((uint32_t)(original >> 32)));
#endif
}

uint64_t my_htonll(const uint64_t& original)
{
#ifdef SPARC_V9  // big endian
    return original;
#else  // little endian
    return (((uint64_t)my_ntohl((uint32_t)original)) << 32) | ((uint64_t)my_ntohl((uint32_t)(original >> 32)));
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

int32_t strToInt32(const char* const str) { return strtol(str, 0, 10); }

int32_t strToInt32(const std::string& str) { return strToInt32(str.c_str()); }

int32_t strToInt32(const char* const str, int radix) { return strtol(str, 0, radix); }

int32_t strWToInt32(const wchar_t* const str) { return wcstol(str, 0, 10); }

int32_t strWToInt32(const wchar_t* const str, int radix) { return wcstol(str, 0, radix); }

uint32_t strWToInt32u(const wchar_t* const str, int radix) { return static_cast<uint32_t>(wcstoul(str, 0, radix)); }

uint32_t strToInt32u(const char* const str, int radix) { return static_cast<uint32_t>(strtoul(str, 0, radix)); }

int16_t strToInt16(const char* const str) { return (int16_t)strtol(str, 0, 10); }

uint16_t strToInt16u(const char* const str) { return (uint16_t)strtol(str, 0, 10); }

int8_t strToInt8(const char* const str) { return (int8_t)strtol(str, 0, 10); }

uint8_t strToInt8u(const char* const str) { return (uint8_t)strtol(str, 0, 10); }

double strToDouble(const char* const str) { return strtod(str, 0); }

double strWToDouble(const wchar_t* const str) { return wcstod(str, 0); }

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

string doubleToStr(const double& x, int precision)
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

string strPadLeft(const string& str, size_t newSize, char filler)
{
    int cnt = newSize - str.size();
    string prefix = "";
    for (int i = 0; i < cnt; i++) prefix += filler;
    return prefix + str;
}

string strPadRight(const string& str, size_t newSize, char filler)
{
    int cnt = newSize - str.size();
    string postfix = "";
    for (int i = 0; i < cnt; i++) postfix += filler;
    return str + postfix;
}

bool strEndWith(const string& str, const string& substr)
{
    if (str.size() == 0)
        return false;
    size_t idx = str.size();
    for (int i = substr.size() - 1; i >= 0; i--)
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

bool strStartWithW(const wstring& str, const wstring& substr)
{
    if (str.size() < substr.size())
        return false;
    for (size_t i = 0; i < substr.size(); i++)
        if (substr[i] != str[i])
            return false;
    return true;
}

vector<string> splitStr(const char* str, char splitter)
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

    size_t splitterSize = splitter.size();
    size_t posBegin = 0;
    size_t posEnd = string::npos;

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

void splitStr(vector<string>& rez, const char* str, char splitter)
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

vector<wstring> splitStrW(const wchar_t* str, wchar_t splitter)
{
    vector<wstring> rez;
    const wchar_t* prevPos = str;
    const wchar_t* buf = str;
    for (; *buf; buf++)
    {
        if (*buf == splitter)
        {
            rez.push_back(wstring(prevPos, buf - prevPos));
            prevPos = buf + 1;
        }
    }
    if (buf > prevPos)
        rez.push_back(wstring(prevPos, buf - prevPos));
    return rez;
}

string extractFileExt(const string& src)
{
    for (int i = src.size() - 1; i >= 0; i--)
        if (src[i] == '.')
        {
            string rez = src.substr(i + 1);
            if (rez.size() > 0 && rez[rez.size() - 1] == '\"')
                return rez.substr(0, rez.size() - 1);
            else
                return rez;
        }
    return "";
}

string extractFileName(const string& src)
{
    int endPos = src.size();
    for (int i = src.size() - 1; i >= 0; i--)
        if (src[i] == '.')
        {
            endPos = i;
        }
        else if (src[i] == '/' || src[i] == '\\')
        {
            string rez = src.substr(i + 1, endPos - i - 1);
            if (rez.size() > 0 && rez[rez.size() - 1] == '\"')
                return rez.substr(0, rez.size() - 1);
            else
                return rez;
        }
    return "";
}

string extractFileName2(const string& src, bool withExt)
{
    string fileName = src;

    size_t extSep = fileName.find_last_of('.');
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
    for (int i = src.size() - 1; i >= 0; i--)
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

wstring trimStrW(const wstring& value)
{
    int chBeg = 0;
    int chEnd = value.length() - 1;
    for (; chBeg < value.length() && (value[chBeg] == '\n' || value[chBeg] == '\r' || value[chBeg] == ' '); chBeg++)
        ;
    for (; chEnd >= chBeg && (value[chEnd] == '\n' || value[chEnd] == '\r' || value[chEnd] == ' '); chEnd--)
        ;
    return value.substr(chBeg, chEnd - chBeg + 1);
}

vector<string> splitQuotedStr(const char* str, char splitter)
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

    transform(res.begin(), res.end(), res.begin(), CaseChanger<string>(ctUpper));

    return res;
}

string strToLowerCase(const string& src)
{
    string res = src;

    transform(res.begin(), res.end(), res.begin(), CaseChanger<string>(ctLower));

    return res;
}

uint32_t my_ntohl(const uint32_t val)
{
    uint8_t* tmp = (uint8_t*)&val;
    return tmp[3] + (tmp[2] << 8) + (tmp[1] << 16) + (tmp[0] << 24);
}

uint16_t my_ntohs(const uint16_t val)
{
    uint8_t* tmp = (uint8_t*)&val;
    return tmp[1] + (tmp[0] << 8);
}

char* strnstr(const char* s1, const char* s2, size_t len)
{
    size_t l1 = len, l2;

    l2 = strlen(s2);
    if (!l2)
        return (char*)s1;
    while (l1 >= l2)
    {
        l1--;
        if (!memcmp(s1, s2, l2))
            return (char*)s1;
        s1++;
    }
    return NULL;
}

uint32_t random32()
{
    static std::random_device dev;
    static std::minstd_rand raand(dev());
    return static_cast<std::uint32_t>(raand());
}

#ifdef _WIN32
#include <windows.h>

std::vector<wchar_t> toWide(const std::string& utf8Str) { return toWide(utf8Str.c_str(), utf8Str.size()); }

std::vector<wchar_t> toWide(const char* utf8Str, int sz)
{
    auto requiredSiz = MultiByteToWideChar(CP_UTF8, 0, utf8Str, sz, nullptr, 0);
    std::vector<wchar_t> multiByteBuf(static_cast<std::size_t>(requiredSiz));
    MultiByteToWideChar(CP_UTF8, 0, utf8Str, sz, multiByteBuf.data(), requiredSiz);
    if (multiByteBuf.empty() || multiByteBuf.back() != 0)
    {
        multiByteBuf.push_back(0);
    }
    return multiByteBuf;
}

std::string toUtf8(const wchar_t* wideStr)
{
    auto needed = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    needed--;  // includes terminating null byte, needless when returning a std::string.
    std::string s(static_cast<std::size_t>(needed), 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, &s[0], needed, nullptr, nullptr);
    return s;
}
#endif
