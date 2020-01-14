
#include "vod_common.h"

#include <fs/directory.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

bool sLastMsg = false;

std::string toNativeSeparators(const std::string& dirName)
{
    std::string rez = dirName;
    for (int i = 0; i < rez.length(); ++i)
        if (rez[i] == '\\' || rez[i] == '/')
            rez[i] = getDirSeparator();
    return rez;
}

bool isFillerNullPacket(uint8_t* curBuf)
{
    uint32_t* endPos = (uint32_t*)(curBuf + TS_FRAME_SIZE);
    for (uint32_t* curBuf32 = (uint32_t*)(curBuf + 4); curBuf32 < endPos; curBuf32++)
        if (*curBuf32 != 0xffffffff && *curBuf32 != 0)
            return false;
    return true;
}

std::string unquoteStr(const std::string& val)
{
    std::string tmp = val;
    if (val.size() > 0 && val[0] == '\"')
        tmp = tmp.substr(1, tmp.size());
    if (val.size() > 0 && val[val.size() - 1] == '\"')
        tmp = tmp.substr(0, tmp.size() - 1);
    return tmp;
}

std::wstring unquoteStrW(const std::wstring& val)
{
    std::wstring tmp = val;
    if (val.size() > 0 && val[0] == '\"')
        tmp = tmp.substr(1, tmp.size());
    if (val.size() > 0 && val[val.size() - 1] == '\"')
        tmp = tmp.substr(0, tmp.size() - 1);
    return tmp;
}

std::string quoteStr(const std::string& val) { return "\"" + val + "\""; }

std::vector<std::string> extractFileList(const std::string& val)
{
    std::vector<std::string> rez;
    bool quoted = false;
    size_t lastStartPos = 0;
    for (int i = 0; i < val.size(); i++)
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
                if (tmp.size() > 0)
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
        if (tmp.size() > 0)
            rez.push_back(tmp);
    }
    return rez;
}

uint16_t AV_RB16(const uint8_t* buffer) { return (*buffer << 8) + buffer[1]; }
uint32_t AV_RB32(const uint8_t* buffer) { return my_ntohl(*((int32_t*)buffer)); }
uint32_t AV_RB24(const uint8_t* buffer) { return (buffer[0] << 16) + (buffer[1] << 8) + buffer[2]; }

void AV_WB16(uint8_t* buffer, uint16_t value) { *((uint16_t*)buffer) = my_htons(value); }

void AV_WB24(uint8_t* buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value >> 16);
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)value;
}
void AV_WB32(uint8_t* buffer, uint32_t value) { *((uint32_t*)buffer) = my_htonl(value); }

std::string floatToTime(double time, char msSeparator)
{
    int iTime = (int)time;
    int hour = iTime / 3600;
    iTime -= hour * 3600;
    int min = iTime / 60;
    iTime -= min * 60;
    int sec = iTime;
    int msec = (int)((time - (int)time) * 1000.0);
    std::ostringstream str;
    str << strPadLeft(int32ToStr(hour), 2, '0') << ':';
    str << strPadLeft(int32ToStr(min), 2, '0') << ':';
    str << strPadLeft(int32ToStr(sec), 2, '0') << msSeparator << strPadLeft(int32ToStr(msec), 3, '0');
    return str.str();
}

double timeToFloat(const std::string& chapterStr)
{
    if (chapterStr.size() == 0)
        return 0;
    std::vector<std::string> timeParts = splitStr(chapterStr.c_str(), ':');
    double sec = 0;
    if (timeParts.size() > 0)
        sec = strToDouble(timeParts[timeParts.size() - 1].c_str());
    int min = 0;
    if (timeParts.size() > 1)
        min = strToInt32(timeParts[timeParts.size() - 2].c_str());
    int hour = 0;
    if (timeParts.size() > 2)
        hour = strToInt32(timeParts[timeParts.size() - 3].c_str());
    return hour * 3600 + min * 60 + sec;
}

double timeToFloatW(const std::wstring& chapterStr)
{
    if (chapterStr.size() == 0)
        return 0;
    std::vector<std::wstring> timeParts = splitStrW(chapterStr.c_str(), ':');
    double sec = 0;
    if (timeParts.size() > 0)
        sec = strWToDouble(timeParts[timeParts.size() - 1].c_str());
    int32_t min = 0;
    if (timeParts.size() > 1)
        min = strWToInt32(timeParts[timeParts.size() - 2].c_str());
    int32_t hour = 0;
    if (timeParts.size() > 2)
        hour = strWToInt32(timeParts[timeParts.size() - 3].c_str());
    return hour * 3600 + min * 60 + sec;
}
