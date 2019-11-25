#ifndef __VOD_COMMON_H
#define __VOD_COMMON_H

#include <types/types.h>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>

#if 1
extern bool sLastMsg;
#define LTRACE(level, errIndex, msg) \
{\
	if (errIndex & 2) { \
		if (level <= LT_WARN) std::cerr << msg << std::endl;\
		else if (level == LT_INFO) std::cout << msg << std::endl;\
        if (level <= LT_INFO)\
        sLastMsg = true;\
	}\
}
class Process {
public:
	static void sleep(int millisec)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(millisec));
	}
};

#endif

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define bswap_32(x) my_ntohl(x)
//#define fabs(a) ((a)>=0?(a):-(a))

const static int DETECT_STREAM_BUFFER_SIZE = 1024*1024*16;
const static unsigned TS_PID_NULL = 8191;
const static unsigned TS_PID_PAT  = 0;
const static unsigned TS_PID_PMT  = 1;
const static unsigned TS_PID_CAS  = 2;

const static unsigned LT_ERR_COMMON = 0;
const static unsigned LT_ERR_MPEG = 1;
const static unsigned LT_MUXER = 2;
const static unsigned LT_TRAFFIC = 3;

const unsigned TS_FRAME_SIZE = 188;

const unsigned DEFAULT_FILE_BLOCK_SIZE = 1024 * 1024 * 2;
const unsigned TS188_ROUND_BLOCK_SIZE = DEFAULT_FILE_BLOCK_SIZE / TS_FRAME_SIZE * TS_FRAME_SIZE;

const unsigned PCR_FREQUENCY = 90000; 
const unsigned PCR_HALF_FREQUENCY = PCR_FREQUENCY/2; // (ignoring lower 33-th bit)

const unsigned DEFAULT_FRAME_FREQ = (uint32_t) (PCR_HALF_FREQUENCY / (4.8*1024.0*1024.0/8.0/TS_FRAME_SIZE));

//const static int64_t FIXED_PTS_OFFSET = 378000000ll; //377910000ll; 

const static int64_t INTERNAL_PTS_FREQ = 1000000000;
const static double INT_FREQ_TO_TS_FREQ = INTERNAL_PTS_FREQ / (double) PCR_FREQUENCY;

const static uint32_t GOP_BUFFER_SIZE = 2*1024*1024;//512*1024;

const static int CANT_CREATE_INDEX_FILE     = -1;
const static int TOO_RARE_PCR               = -2;
const static int MULTIPLE_PMT_NOT_SUPPORTED = -3;
const static int INVALID_PAT                = -4;
const static int MPEG_INTERNAL_ERROR        = -5;
const static int CANT_CREATE_ASSET_FILE     = -6;
const static int ERR_ENCODING_ALREADY_STARTED = -7;
const static int CANT_FIND_VIDEO_DECODER    = -8;
const static int INVALID_TS_FRAME_SYNC_CODE = -9;
const static int NOT_ENOUGHT_BUFFER         = -10;
const static int INVALID_BITSTREAM_SYNTAX   = -11;
const static int MAX_ERROR = -100;


bool isFillerNullPacket(uint8_t* curBuf);

std::string unquoteStr(const std::string& val);
std::wstring unquoteStrW(const std::wstring& val);
std::string quoteStr(const std::string& val);

std::vector<std::string> extractFileList(const std::string& val);

struct AVRational{
    int num; ///< numerator
    int den; ///< denominator
	AVRational() {num = den = 0;}
	AVRational(int _num, int _den) {num = _num; den = _den;}

};

typedef std::vector<std::pair<int, int> > PriorityDataInfo; // mark some data as priority data

enum DiskType {DT_NONE, DT_BLURAY, UHD_BLURAY, DT_AVCHD};

uint16_t AV_RB16(const uint8_t* buffer);
uint32_t AV_RB24(const uint8_t* buffer);
uint32_t AV_RB32(const uint8_t* buffer);
void AV_WB16(uint8_t* buffer, uint16_t value);
void AV_WB24(uint8_t* buffer, uint32_t value);
void AV_WB32(uint8_t* buffer, uint32_t value);

std::string floatToTime(double time, char msSeparator = '.');
double timeToFloat(const std::string& chapterStr);
double timeToFloatW(const std::wstring& chapterStr);
std::string toNativeSeparators(const std::string& dirName);

static int64_t nanoClockToPts(int64_t value) { return int64_t(value/INT_FREQ_TO_TS_FREQ + (value >= 0 ? 0.5 : -0.5)); }
static int64_t ptsToNanoClock(int64_t value) { return int64_t(value*INT_FREQ_TO_TS_FREQ + (value >= 0 ? 0.5 : -0.5)); }

struct PIPParams
{
    enum PipCorner
    {
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft
    };

    PIPParams(): scaleIndex(1), corner(TopLeft), hOffset(0), vOffset(0), lumma(3) {}

    bool isFullScreen() const { return scaleIndex == 5; }
    float getScaleCoeff() const
    {
        if (scaleIndex == 2)
            return 0.5;
        else if (scaleIndex == 3)
            return 0.25;
        else if (scaleIndex == 4)
            return 1.5;
        else
            return 1.0;
    }

    int scaleIndex;
    PipCorner corner;
    int hOffset;
    int vOffset;
    int lumma;
};

#endif
