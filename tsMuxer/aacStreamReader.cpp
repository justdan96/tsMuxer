#include "aacStreamReader.h"
#include "nalUnits.h"
#include "vodCoreException.h"

int AACStreamReader::getHeaderLen() { return AAC_HEADER_LEN; }

const std::string AACStreamReader::getStreamInfo()
{
    std::ostringstream str;
    str << "Sample Rate: " << m_sample_rate / 1000 << "KHz  ";
    str << "Channels: " << static_cast<int>(m_channels);
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

int AACStreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    return 0;
    // TODO: fix AAC descriptor

    // H.222 Table 2-94 - MPEG-2 AAC_audio_descriptor
    *dstBuff++ = static_cast<uint8_t>(TSDescriptorTag::AAC2);  // MPEG-2 AAC descriptor tag;
    *dstBuff++ = 3;                                            // descriptor length
    *dstBuff++ = m_profile;
    *dstBuff++ = m_channels_index;
    *dstBuff++ = 0;  // MPEG-2_AAC_additional_information

    return 5;  // total descriptor length
}
