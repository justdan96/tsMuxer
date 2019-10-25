#ifndef __VOD_CORE_EXCEPTION_H
#define __VOD_CORE_EXCEPTION_H

#include <string>
#include <sstream>

class VodCoreException {
public:
	VodCoreException(int errCode, const std::string& errStr): m_errCode(errCode), m_errStr(errStr) {}
	VodCoreException(int errCode, const char* errStr): m_errCode(errCode), m_errStr(errStr) {}
	const int m_errCode;
	const std::string m_errStr;
};

#define THROW(errCode, msg) \
{ \
	std::ostringstream ss; \
	ss << msg; \
	throw VodCoreException(errCode, ss.str()); \
 }

const static int ERR_INVALID_HANDLE = 1;

const static int ERR_CANT_EXEC_PROCESS = 2;

const static int ERR_COMMON = 3;

const static int ERR_FILE_NOT_FOUND   = 100;
const static int ERR_FILE_SEEK        = 101;
const static int ERR_CANT_OPEN_STREAM = 102;
const static int ERR_CANT_CREATE_FILE = 103;
const static int ERR_FILE_EXISTS      = 104;
const static int ERR_FILE_COMMON      = 105;

const static int ERR_TS_COMMON        = 200;
const static int ERR_TS_UNKNOWN_MEDIA = 201;
const static int ERR_TS_RARE_PMT      = 202;

const static int ERR_H264_RARE_SPS = 300;
const static int ERR_H264_PARSE_ERROR = 301;
const static int ERR_H264_UNSUPPORTED_PARAMETER = 302;

const static int ERR_MPEG2_ERR_FPS = 350;
const static int ERR_VC1_ERR_FPS = 351;
const static int ERR_VC1_ERR_PROFILE = 352;


const static int ERR_AAC_RARE_FRAME = 400;
const static int ERR_AAC_TO_SMALL_AV_PACKET = 401;
const static int ERR_AC3_RARE_FRAME = 410;
const static int ERR_AC3_TO_SMALL_AV_PACKET = 411;

const static int ERR_MP3_RARE_FRAME = 420;

const static int ERR_INVALID_CODEC_INFO_FORMAT = 500;
const static int ERR_UNKNOWN_CODEC = 501;
const static int ERR_INVALID_CODEC_FORMAT = 502;
const static int ERR_INVALID_STREAMS_SELECTED = 504;

const static int ERR_COMMON_SMALL_BUFFER  = 600;
const static int ERR_COMMON_NETWORK       = 601;
const static int ERR_COMMON_MEMORY       = 602;

const static int ERR_UNSUPPORTER_CONTAINER_FORMAT = 700;
const static int ERR_CONTAINER_STREAM_NOT_SYNC = 701;

const static int ERR_AV_FRAME_TOO_LARGE = 800;

const static int ERR_MATROSKA_PARSE = 900;

const static int ERR_MOV_PARSE = 950;

const static int ERR_WAV_PARSE = 950;

const static int ERR_INTERNAL_ERROR = 1000;
const static char ERR_INTERNAL_ERROR_STR[] = "Internal error";


const static int MSG_ENDOFFILE = 2000;

const static int ERR_FEC_UNSUPPORTED_FEATURE = 3000;

#endif
