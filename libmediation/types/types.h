#ifndef __T_TYPES_H
#define __T_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#define strcasecmp stricmp
#endif
char* strnstr(const char* s1, const char* s2, size_t len);

uint64_t my_ntohll(const uint64_t& original);
uint64_t my_htonll(const uint64_t& original);
uint32_t my_ntohl(const uint32_t val);
uint16_t my_ntohs(const uint16_t val);
#define my_htonl(val) my_ntohl(val)
#define my_htons(val) my_ntohs(val)

int64_t strToInt64(const char* const);
uint64_t strToInt64u(const char* const);
int32_t strToInt32(const char* const);
int32_t strToInt32(const std::string&);
int32_t strToInt32(const char* const, const int radix);
int32_t strWToInt32(const wchar_t* const str);
int32_t strWToInt32(const wchar_t* const str, const int radix);
uint32_t strWToInt32u(const wchar_t* const str, const int radix);
uint32_t strToInt32u(const char* const, const int radix = 10);
int16_t strToInt16(const char* const);
uint16_t strToInt16u(const char* const);
int8_t strToInt8(const char* const);
uint8_t strToInt8u(const char* const);
double strToDouble(const char* const);      // The length of the string should not exceed 15 characters
double strWToDouble(const wchar_t* const);  // The length of the string should not exceed 15 characters
bool strToBool(const char* const);
bool strEndWith(const std::string& str, const std::string& substr);
bool strStartWith(const std::string& str, const std::string& substr);
bool strStartWithW(const std::wstring& str, const std::wstring& substr);
std::string strPadLeft(const std::string& str, size_t newSize, char filler);
std::string strPadRight(const std::string& str, size_t newSize, char filler);

std::vector<std::string> splitStr(const char* str, char splitter);
std::vector<std::string> splitStr(const std::string& str, const std::string& splitter);
void splitStr(std::vector<std::string>& rez, const char* str, char splitter);

std::string trimStr(const std::string& value);
std::wstring trimStrW(const std::wstring& value);

std::string extractFileExt(const std::string& src);
std::string extractFileName(const std::string& src);
std::string extractFileName2(const std::string& src, bool withExt = true);
std::string extractFilePath(const std::string& src);
std::string closeDirPath(const std::string& src, char delimiter = ' ');

std::vector<std::wstring> splitStrW(const wchar_t* str, wchar_t splitter);
std::vector<std::string> splitQuotedStr(const char* str, char splitter);

std::string int64ToStr(const int64_t&);
std::string int64uToStr(const uint64_t&);
std::string int32ToStr(const int32_t&);
std::string int32uToStr(const uint32_t&);
std::string int32ToHex(const int32_t&);
std::string int32uToHex(const uint32_t&);
//! Conversion of a floating-point number into a string.
/*!
         When precision = -1, the output contains four digits after the comma.
*/
std::string doubleToStr(const double& x, int precision = -1);

std::string int16ToStr(const int16_t&);
std::string int16uToStr(const uint16_t&);
std::string int8ToStr(const int8_t&);
std::string int8uToStr(const uint8_t&);
void int8uToStr(const uint8_t&, char* const buf);

std::string boolToStr(const bool&);

// Work on lines
std::string strToUpperCase(const std::string& src);
std::string strToLowerCase(const std::string& src);

enum CaseType
{
    ctLower = 0,
    ctUpper
};

template <typename Type>
class CaseChanger
{
   public:
    CaseChanger(CaseType caseType = ctLower) : m_case(caseType) {}

    int operator()(Type& elem) const { return 0; }

    int operator()(char& elem) const { return m_case == ctLower ? tolower(elem) : toupper(elem); }

   private:
    CaseType m_case;
};

typedef uint16_t ip_port_t;

uint32_t roundDown(const uint32_t& value, const uint32_t& roundVal);
uint32_t roundUp(const uint32_t& value, const uint32_t& roundVal);

// 64-bit versions
uint64_t roundDown64(const uint64_t& value, const uint64_t& roundVal);
uint64_t roundUp64(const uint64_t& value, const uint64_t& roundVal);

uint32_t random32();

static uint32_t FOUR_CC(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return my_ntohl((uint32_t(a) << 24) + (uint32_t(b) << 16) + (uint32_t(c) << 8) + uint32_t(d));
}

#ifdef _WIN32
std::vector<wchar_t> toWide(const char*, int sz = -1);
std::vector<wchar_t> toWide(const std::string&);
std::string toUtf8(const wchar_t*);
#endif

#endif  //__T_TYPES_H
