
#include "pesPacket.h"


void PESPacket::serialize(uint64_t pts, uint8_t streamID)
{
	setStreamID(streamID);
	setPacketLength(0);
	resetFlags();
	setHeaderLength(PESPacket::HEADER_SIZE + PESPacket::PTS_SIZE); // 14 bytes
	setPts(pts);
}

void PESPacket::serialize(uint64_t pts, uint64_t dts, uint8_t streamID)
{
	setStreamID(streamID);
	setPacketLength(0);
	setHeaderLength(PESPacket::HEADER_SIZE + PESPacket::PTS_SIZE * 2); // 19 bytes
	resetFlags();
	setPtsAndDts(pts, dts);
}

void PESPacket::serialize(uint8_t streamID)
{
	setStreamID(streamID);
	setPacketLength(0);
	setHeaderLength(PESPacket::HEADER_SIZE); // 19 bytes
	resetFlags();
}
