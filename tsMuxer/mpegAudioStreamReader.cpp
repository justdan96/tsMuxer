
#include "mpegAudioStreamReader.h"

#include <sstream>

int MpegAudioStreamReader::getTSDescriptor(uint8_t* dstBuff)
{
    uint8_t* frame = findFrame(m_buffer, m_bufEnd);
    if (frame == 0)
        return 0;
    int skipBytes = 0;
    int skipBeforeBytes = 0;
    int len = decodeFrame(frame, m_bufEnd, skipBytes, skipBeforeBytes);
    return 0;
}

uint8_t* MpegAudioStreamReader::findFrame(uint8_t* buff, uint8_t* end) { return mp3FindFrame(buff, end); }

int MpegAudioStreamReader::decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes)
{
    skipBytes = 0;
    skipBeforeBytes = 0;
    return mp3DecodeFrame(buff, end);
}

double MpegAudioStreamReader::getFrameDurationNano()
{
    // return (INTERNAL_PTS_FREQ * m_samples) / m_sample_rate;
    double rez = (INTERNAL_PTS_FREQ * m_samples) / m_sample_rate;
    if (m_layer == 3 && m_nb_channels == 1)
        rez /= 2;  // todo: what is that? I myself don't understand why it's necessary to leave it here.
    return rez;
}

const std::string MpegAudioStreamReader::getStreamInfo()
{
    std::ostringstream str;
    str << "Bitrate: " << m_bit_rate / 1000 << "Kbps  ";
    str << "Sample Rate: " << m_sample_rate / 1000 << "KHz  ";
    str << "Channels: " << (int)m_nb_channels << "  ";
    str << "Layer: " << m_layer;
    return str.str();
}
