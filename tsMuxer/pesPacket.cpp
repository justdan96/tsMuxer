
#include "pesPacket.h"

void PESPacket::serialize(const int64_t pts, const uint8_t streamID)
{
    setStreamID(streamID);
    setPacketLength(0);
    resetFlags();
    setHeaderLength(HEADER_SIZE + PTS_SIZE);  // 14 bytes
    setPts(pts);
}

void PESPacket::serialize(const int64_t pts, const int64_t dts, const uint8_t streamID)
{
    setStreamID(streamID);
    setPacketLength(0);
    setHeaderLength(HEADER_SIZE + PTS_SIZE * 2);  // 19 bytes
    resetFlags();
    setPtsAndDts(pts, dts);
}

void PESPacket::serialize(const uint8_t streamID)
{
    setStreamID(streamID);
    setPacketLength(0);
    setHeaderLength(HEADER_SIZE);  // 19 bytes
    resetFlags();
}
