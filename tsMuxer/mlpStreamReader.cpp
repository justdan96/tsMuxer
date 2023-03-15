#include "mlpStreamReader.h"

#include <fs/systemlog.h>

#include <iostream>

#include "avCodecs.h"
#include "nalUnits.h"
#include "vodCoreException.h"
#include "vod_common.h"

int MLPStreamReader::getHeaderLen() { return MLP_HEADER_LEN; }

const std::string MLPStreamReader::getStreamInfo()
{
    std::ostringstream str;

    if (m_subType == MlpSubType::stTRUEHD)
        str << "TRUE-HD";
    else if (m_subType == MlpSubType::stMLP)
        str << "MLP";
    else
        str << "UNKNOWN";

    if (m_substreams == 4)
        str << " + ATMOS";
    str << ". ";
    str << "Peak bitrate: " << m_bitrate / 1000 << "Kbps ";
    str << "Sample Rate: " << m_samplerate / 1000 << "KHz ";
    str << "Channels: " << m_channels;
    return str.str();
}

int MLPStreamReader::decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes)
{
    skipBytes = 0;
    skipBeforeBytes = 0;
    if (MLPCodec::decodeFrame(buff, end))
        return getFrameSize(buff);
    return 0;
}

int MLPStreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    // TODO: fix MLP descriptor

    *dstBuff++ = (int)TSDescriptorTag::REGISTRATION;  // descriptor tag
    *dstBuff++ = 4;                                   // descriptor length
    // https://smpte-ra.org/registered-mpeg-ts-ids
    memcpy(dstBuff, "mlpa", 4);  // format_identifier

    return 6;  // total descriptor length
}

int MLPStreamReader::readPacket(AVPacket& avPacket)
{
    while (1)
    {
        int rez = SimplePacketizerReader::readPacket(avPacket);
        if (rez != 0)
            return rez;

        // thg packet
        avPacket.dts = avPacket.pts = m_totalTHDSamples * 1000000000ll / m_samplerate;

        m_totalTHDSamples += m_samples;
        m_demuxedTHDSamples += m_samples;
        if (m_demuxedTHDSamples >= m_samples)
        {
            m_demuxedTHDSamples -= m_samples;
        }
        return 0;
    }
}

int MLPStreamReader::flushPacket(AVPacket& avPacket)
{
    int rez = SimplePacketizerReader::flushPacket(avPacket);
    if (rez > 0)
    {
        if (!(avPacket.flags & AVPacket::PRIORITY_DATA))
            avPacket.pts = avPacket.dts =
                m_totalTHDSamples * 1000000000ll / m_samplerate;  // replace time to a next HD packet
    }
    return rez;
}
