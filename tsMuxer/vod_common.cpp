#include "vod_common.h"

#include <climits>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include <fs/directory.h>

using namespace std;

bool sLastMsg = false;

std::string toNativeSeparators(const std::string& dirName)
{
    std::string rez = dirName;
    for (char& c : rez)
    {
        if (c == '\\' || c == '/')
            c = getDirSeparator();
    }
    return rez;
}

bool isFillerNullPacket(uint8_t* curBuf)
{
    const auto endPos = reinterpret_cast<uint32_t*>(curBuf + TS_FRAME_SIZE);
    for (auto curBuf32 = reinterpret_cast<uint32_t*>(curBuf + 4); curBuf32 < endPos; curBuf32++)
        if (*curBuf32 != UINT_MAX && *curBuf32 != 0)
            return false;
    return true;
}

std::string unquoteStr(const std::string& val)
{
    std::string tmp = val;
    if (!val.empty() && val[0] == '\"')
        tmp = tmp.substr(1, tmp.size());
    if (!val.empty() && val[val.size() - 1] == '\"')
        tmp = tmp.substr(0, tmp.size() - 1);
    return tmp;
}

std::string quoteStr(const std::string& val) { return "\"" + val + "\""; }

std::vector<std::string> extractFileList(const std::string& val)
{
    std::vector<std::string> rez;
    bool quoted = false;
    size_t lastStartPos = 0;
    for (size_t i = 0; i < val.size(); i++)
    {
        if (val[i] == '"')
            quoted = !quoted;
        else if (val[i] == '+' && !quoted)
        {
            if (i > lastStartPos)
            {
                std::string tmp = val.substr(lastStartPos, i - lastStartPos);
                tmp = trimStr(tmp);
                tmp = unquoteStr(tmp);
                if (!tmp.empty())
                    rez.push_back(tmp);
            }
            lastStartPos = i + 1;
        }
    }
    if (rez.empty())
        rez.push_back(unquoteStr(trimStr(val)));
    else
    {
        std::string tmp = val.substr(lastStartPos, val.size());
        tmp = trimStr(tmp);
        tmp = unquoteStr(tmp);
        if (!tmp.empty())
            rez.push_back(tmp);
    }
    return rez;
}

uint16_t AV_RB16(const uint8_t* buffer) { return static_cast<uint16_t>(*buffer << 8 | buffer[1]); }
uint32_t AV_RB32(uint8_t* buffer) { return my_ntohl(*reinterpret_cast<uint32_t*>(buffer)); }
uint32_t AV_RB24(const uint8_t* buffer) { return buffer[0] << 16 | buffer[1] << 8 | buffer[2]; }

void AV_WB16(uint8_t* buffer, const uint16_t value) { *reinterpret_cast<uint16_t*>(buffer) = my_htons(value); }

void AV_WB24(uint8_t* buffer, const uint32_t value)
{
    buffer[0] = static_cast<uint8_t>(value >> 16);
    buffer[1] = static_cast<uint8_t>(value >> 8);
    buffer[2] = static_cast<uint8_t>(value);
}
void AV_WB32(uint8_t* buffer, const uint32_t value) { *reinterpret_cast<uint32_t*>(buffer) = my_htonl(value); }

std::string floatToTime(const double time, const char msSeparator)
{
    int iTime = static_cast<int>(time);
    const int hour = iTime / 3600;
    iTime -= hour * 3600;
    const int min = iTime / 60;
    iTime -= min * 60;
    const int sec = iTime;
    const int msec = static_cast<int>((time - static_cast<int>(time)) * 1000.0);
    std::ostringstream str;
    str << strPadLeft(int32ToStr(hour), 2, '0') << ':';
    str << strPadLeft(int32ToStr(min), 2, '0') << ':';
    str << strPadLeft(int32ToStr(sec), 2, '0') << msSeparator << strPadLeft(int32ToStr(msec), 3, '0');
    return str.str();
}

double timeToFloat(const std::string& chapterStr)
{
    if (chapterStr.empty())
        return 0;
    const std::vector<std::string> timeParts = splitStr(chapterStr.c_str(), ':');
    double sec = 0;
    if (!timeParts.empty())
        sec = strToDouble(timeParts[timeParts.size() - 1].c_str());
    int min = 0;
    if (timeParts.size() > 1)
        min = strToInt32(timeParts[timeParts.size() - 2].c_str());
    int hour = 0;
    if (timeParts.size() > 2)
        hour = strToInt32(timeParts[timeParts.size() - 3].c_str());
    return hour * 3600 + min * 60 + sec;
}

double correctFps(const double fps)
{
    struct FPSCorrect
    {
        double from;
        double to;
    };

    FPSCorrect fpsCorrectList[] = {
        {5.994, 5.99400599400599},   {11.988, 11.98801198801198}, {23.976, 23.97602397602397},
        {47.952, 47.95204795204795}, {7.4925, 7.49250749250749},  {14.985, 14.98501498501498},
        {29.97, 29.97002997002997},  {59.94, 59.94005994005994},
    };

    for (const auto& i : fpsCorrectList)
    {
        if (fabs(fps - i.from) < 1e-4)
        {
            return i.to;
        }
    }
    return fps;
}
