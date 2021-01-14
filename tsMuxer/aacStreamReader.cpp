#include "aacStreamReader.h"

#include <fs/systemlog.h>

#include <iostream>

#include "avCodecs.h"
#include "nalUnits.h"
#include "vodCoreException.h"
#include "vod_common.h"

int AACStreamReader::getHeaderLen() { return AAC_HEADER_LEN; }

const std::string AACStreamReader::getStreamInfo()
{
    std::ostringstream str;
    str << "Sample Rate: " << m_sample_rate / 1000 << "KHz  ";
    str << "Channels: " << (int)m_channels;
    return str.str();
}

int AACStreamReader::decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes)
{
    skipBytes = 0;
    skipBeforeBytes = 0;
    if (AACCodec::decodeFrame(buff, end))
        return getFrameSize(buff);
    return 0;
}
