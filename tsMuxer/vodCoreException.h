#ifndef VOD_CORE_EXCEPTION_H_
#define VOD_CORE_EXCEPTION_H_

#include <sstream>
#include <string>

class VodCoreException
{
   public:
    VodCoreException(const int errCode, std::string errStr) : m_errCode(errCode), m_errStr(std::move(errStr)) {}
    VodCoreException(const int errCode, const char* errStr) : m_errCode(errCode), m_errStr(errStr) {}

    int m_errCode;
    std::string m_errStr;
};

#define THROW(errCode, msg)                        \
    {                                              \
        std::ostringstream ss;                     \
        ss << msg;                                 \
        throw VodCoreException(errCode, ss.str()); \
    }

static constexpr int ERR_INVALID_HANDLE = 1;

static constexpr int ERR_CANT_EXEC_PROCESS = 2;

static constexpr int ERR_COMMON = 3;

static constexpr int ERR_FILE_NOT_FOUND = 100;
static constexpr int ERR_FILE_SEEK = 101;
static constexpr int ERR_CANT_OPEN_STREAM = 102;
static constexpr int ERR_CANT_CREATE_FILE = 103;
static constexpr int ERR_FILE_EXISTS = 104;
static constexpr int ERR_FILE_COMMON = 105;

static constexpr int ERR_TS_COMMON = 200;
static constexpr int ERR_TS_UNKNOWN_MEDIA = 201;
static constexpr int ERR_TS_RARE_PMT = 202;

static constexpr int ERR_H264_RARE_SPS = 300;
static constexpr int ERR_H264_PARSE_ERROR = 301;
static constexpr int ERR_H264_UNSUPPORTED_PARAMETER = 302;

static constexpr int ERR_MPEG2_ERR_FPS = 350;
static constexpr int ERR_VC1_ERR_FPS = 351;
static constexpr int ERR_VC1_ERR_PROFILE = 352;

static constexpr int ERR_AAC_RARE_FRAME = 400;
static constexpr int ERR_AAC_TO_SMALL_AV_PACKET = 401;
static constexpr int ERR_AC3_RARE_FRAME = 410;
static constexpr int ERR_AC3_TO_SMALL_AV_PACKET = 411;

static constexpr int ERR_MP3_RARE_FRAME = 420;

static constexpr int ERR_INVALID_CODEC_INFO_FORMAT = 500;
static constexpr int ERR_UNKNOWN_CODEC = 501;
static constexpr int ERR_INVALID_CODEC_FORMAT = 502;
static constexpr int ERR_INVALID_STREAMS_SELECTED = 504;

static constexpr int ERR_COMMON_SMALL_BUFFER = 600;
static constexpr int ERR_COMMON_NETWORK = 601;
static constexpr int ERR_COMMON_MEMORY = 602;

static constexpr int ERR_UNSUPPORTER_CONTAINER_FORMAT = 700;
static constexpr int ERR_CONTAINER_STREAM_NOT_SYNC = 701;

static constexpr int ERR_AV_FRAME_TOO_LARGE = 800;

static constexpr int ERR_MATROSKA_PARSE = 900;

static constexpr int ERR_MOV_PARSE = 950;

static constexpr int ERR_WAV_PARSE = 950;

static constexpr int ERR_INTERNAL_ERROR = 1000;
static constexpr char ERR_INTERNAL_ERROR_STR[] = "Internal error";

static constexpr int MSG_ENDOFFILE = 2000;

static constexpr int ERR_FEC_UNSUPPORTED_FEATURE = 3000;

#endif
