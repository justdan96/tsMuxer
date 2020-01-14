#ifndef __AV_PACKET_H
#define __AV_PACKET_H

#include <types/types.h>

#include "vod_common.h"

const static int MAX_AV_PACKET_SIZE = 32768 /*16 * 1024*/;

enum VideoAspectRatio
{
    AR_KEEP_DEFAULT,
    AR_VGA,
    AR_3_4,
    AR_16_9,
    AR_221_100
};

struct AVChapter
{
    AVChapter() : start(0) {}
    AVChapter(int64_t _start, const std::string _cTitle) : start(_start), cTitle(_cTitle) {}
    bool operator<(const AVChapter& other) const { return start < other.start; }
    int64_t start;
    std::string cTitle;
};
typedef std::vector<AVChapter> AVChapters;

class BaseAbstractStreamReader;

struct AVPacket
{
    int64_t pts;  // presentation time stamp in time_base units
    int64_t dts;  // decompression time stamp in time_base units
    uint8_t* data;
    int size;  // data len
    int stream_index;
    int flags;
    int64_t duration;  //  presentation duration in time_base units (0 if not available)
    int64_t pos;       //  byte position in stream, -1 if unknown
    int64_t pcr;       // last program clock reference if packet demuxed from TS container. used for VBV streaming in
                       // RTP/RTSP mode
    BaseAbstractStreamReader* codec;
    int codecID;
    const static unsigned PCR_STREAM = 1;
    const static unsigned IS_COMPLETE_FRAME = 2;
    const static unsigned FORCE_NEW_FRAME = 4;
    const static unsigned IS_IFRAME = 8;
    const static unsigned PRIORITY_DATA = 16;
    const static unsigned IS_SPS_PPS_IN_GOP = 32;

    // filled for THD only. need refactor for DTS as well
    const static unsigned IS_CORE_PACKET = 64;
};

class BaseAbstractStreamReader
{
   public:
    BaseAbstractStreamReader() {}
    ~BaseAbstractStreamReader() {}
    virtual int writeAdditionData(uint8_t* dst, uint8_t* dstEnd, AVPacket& avPacket, PriorityDataInfo* priorityData)
    {
        return 0;
    }
};

#endif
