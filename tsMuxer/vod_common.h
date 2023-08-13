#ifndef VOD_COMMON_H_
#define VOD_COMMON_H_

#include <types/types.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if 1
extern bool sLastMsg;
#define LTRACE(level, errIndex, msg)               \
    do                                             \
    {                                              \
        {                                          \
            if (errIndex & 2)                      \
            {                                      \
                if (level <= LT_WARN)              \
                    std::cerr << msg << std::endl; \
                else if (level == LT_INFO)         \
                    std::cout << msg << std::endl; \
                if (level <= LT_INFO)              \
                    sLastMsg = true;               \
            }                                      \
        }                                          \
    } while (0)
class Process
{
   public:
    static void sleep(const int millisec) { std::this_thread::sleep_for(std::chrono::milliseconds(millisec)); }
};

#endif

#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define bswap_32(x) my_ntohl(x)

static constexpr unsigned DETECT_STREAM_BUFFER_SIZE = 1024 * 1024 * 64;
static constexpr unsigned TS_PID_NULL = 8191;
static constexpr unsigned TS_PID_PAT = 0;
static constexpr unsigned TS_PID_PMT = 1;
static constexpr unsigned TS_PID_CAS = 2;

static constexpr unsigned LT_ERR_COMMON = 0;
static constexpr unsigned LT_ERR_MPEG = 1;
static constexpr unsigned LT_MUXER = 2;
static constexpr unsigned LT_TRAFFIC = 3;

constexpr size_t TS_FRAME_SIZE = 188;

constexpr unsigned DEFAULT_FILE_BLOCK_SIZE = 1024 * 1024 * 2;
constexpr unsigned TS188_ROUND_BLOCK_SIZE = DEFAULT_FILE_BLOCK_SIZE / TS_FRAME_SIZE * TS_FRAME_SIZE;

constexpr unsigned PCR_FREQUENCY = 90000;

// const static int64_t FIXED_PTS_OFFSET = 378000000ll; //377910000ll;

static constexpr int64_t INTERNAL_PTS_FREQ = 196 * 27000000ll;
static constexpr int64_t INT_FREQ_TO_TS_FREQ = INTERNAL_PTS_FREQ / PCR_FREQUENCY;

static constexpr uint32_t GOP_BUFFER_SIZE = 2 * 1024 * 1024;  // 512*1024;

static constexpr int CANT_CREATE_INDEX_FILE = -1;
static constexpr int TOO_RARE_PCR = -2;
static constexpr int MULTIPLE_PMT_NOT_SUPPORTED = -3;
static constexpr int INVALID_PAT = -4;
static constexpr int MPEG_INTERNAL_ERROR = -5;
static constexpr int CANT_CREATE_ASSET_FILE = -6;
static constexpr int ERR_ENCODING_ALREADY_STARTED = -7;
static constexpr int CANT_FIND_VIDEO_DECODER = -8;
static constexpr int INVALID_TS_FRAME_SYNC_CODE = -9;
static constexpr int NOT_ENOUGH_BUFFER = -10;
static constexpr int INVALID_BITSTREAM_SYNTAX = -11;
static constexpr int MAX_ERROR = -100;

bool isFillerNullPacket(uint8_t* curBuf);

std::string unquoteStr(const std::string& val);
std::string quoteStr(const std::string& val);

std::vector<std::string> extractFileList(const std::string& val);

struct AVRational
{
    int num;  ///< numerator
    int den;  ///< denominator
    AVRational() { num = den = 0; }
    AVRational(const int _num, const int _den)
    {
        num = _num;
        den = _den;
    }
};

typedef std::vector<std::pair<int, int>> PriorityDataInfo;  // mark some data as priority data

enum class DiskType
{
    NONE,
    BLURAY,
    AVCHD
};

uint16_t AV_RB16(const uint8_t* buffer);
uint32_t AV_RB24(const uint8_t* buffer);
uint32_t AV_RB32(uint8_t* buffer);
void AV_WB16(uint8_t* buffer, uint16_t value);
void AV_WB24(uint8_t* buffer, uint32_t value);
void AV_WB32(uint8_t* buffer, uint32_t value);

std::string floatToTime(double time, char msSeparator = '.');
double timeToFloat(const std::string& chapterStr);
std::string toNativeSeparators(const std::string& dirName);
double correctFps(double fps);

static int64_t internalClockToPts(const int64_t value) { return value / INT_FREQ_TO_TS_FREQ; }
static int64_t ptsToInternalClock(const int64_t value) { return value * INT_FREQ_TO_TS_FREQ; }

struct PIPParams
{
    enum class PipCorner
    {
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft
    };

    PIPParams() : scaleIndex(1), corner(PipCorner::TopLeft), hOffset(0), vOffset(0), lumma(3) {}

    bool isFullScreen() const { return scaleIndex == 5; }

    float getScaleCoeff() const
    {
        if (scaleIndex == 2)
            return 0.5;
        if (scaleIndex == 3)
            return 0.25;
        if (scaleIndex == 4)
            return 1.5;

        return 1.0;
    }

    int scaleIndex;
    PipCorner corner;
    unsigned hOffset;
    unsigned vOffset;
    int lumma;
};

#endif
