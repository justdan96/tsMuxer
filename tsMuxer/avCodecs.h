#ifndef __AV_CODECS_H
#define __AV_CODECS_H

#include <types/types.h>

#include <string>

#include "avPacket.h"

const static int CODEC_ID_NONE = 0;
const static int CODEC_V_MPEG4_H264 = 1;
const static int CODEC_A_AAC = 2;
const static int CODEC_A_AC3 = 3;
const static int CODEC_A_EAC3 = 4;
const static int CODEC_A_DTS = 5;
const static int CODEC_V_VC1 = 6;
const static int CODEC_V_MPEG2 = 7;
const static int CODEC_A_HDAC3 = 8;
const static int CODEC_A_MPEG_AUDIO = 9;
const static int CODEC_A_LPCM = 10;
const static int CODEC_S_SUP = 11;
const static int CODEC_S_PGS = 12;
const static int CODEC_S_SRT = 13;
const static int CODEC_V_MPEG4_H264_DEP = 14;
const static int CODEC_V_MPEG4_H265 = 15;

struct CodecInfo
{
    CodecInfo() : codecID(0) {}
    CodecInfo(int codecID, const std::string& displayName, const std::string& programName)
    {
        this->codecID = codecID;
        this->displayName = displayName;
        this->programName = programName;
    }
    int codecID;
    std::string displayName;
    std::string programName;
};

struct CheckStreamRez
{
    CheckStreamRez() : trackID(0), delay(0), multiSubStream(false), isSecondary(false), unused(false) {}
    CodecInfo codecInfo;
    std::string streamDescr;
    std::string lang;
    uint32_t trackID;
    int64_t delay;  // auto delay for audio

    bool multiSubStream;
    bool isSecondary;
    bool unused;
};

const static CodecInfo hevcCodecInfo(CODEC_V_MPEG4_H265, "HEVC", "V_MPEGH/ISO/HEVC");
const static CodecInfo h264CodecInfo(CODEC_V_MPEG4_H264, "H.264", "V_MPEG4/ISO/AVC");
const static CodecInfo h264DepCodecInfo(CODEC_V_MPEG4_H264_DEP, "MVC",
                                        "V_MPEG4/ISO/MVC");  // H.264/MVC dependent stream
const static CodecInfo aacCodecInfo(CODEC_A_AAC, "AAC", "A_AAC");
const static CodecInfo dtsCodecInfo(CODEC_A_DTS, "DTS", "A_DTS");
const static CodecInfo dtshdCodecInfo(CODEC_A_DTS, "DTS-HD", "A_DTS");
const static CodecInfo ac3CodecInfo(CODEC_A_AC3, "AC3", "A_AC3");
const static CodecInfo eac3CodecInfo(CODEC_A_EAC3, "E-AC3 (DD+)", "A_AC3");
const static CodecInfo lpcmCodecInfo(CODEC_A_LPCM, "LPCM", "A_LPCM");
const static CodecInfo trueHDCodecInfo(CODEC_A_HDAC3, "TRUE-HD", "A_AC3");
const static CodecInfo vc1CodecInfo(CODEC_V_VC1, "VC-1", "V_MS/VFW/WVC1");
const static CodecInfo mpeg2CodecInfo(CODEC_V_MPEG2, "MPEG-2", "V_MPEG-2");
const static CodecInfo mpegAudioCodecInfo(CODEC_A_MPEG_AUDIO, "MPEG-Audio", "A_MP3");
const static CodecInfo dvbSubCodecInfo(CODEC_S_SUP, "SUP", "S_SUP");
const static CodecInfo pgsCodecInfo(CODEC_S_PGS, "PGS", "S_HDMV/PGS");
const static CodecInfo srtCodecInfo(CODEC_S_SRT, "SRT", "S_TEXT/UTF8");

#endif
