#include "ac3StreamReader.h"

#include <fs/systemlog.h>

#include <sstream>

#include "avCodecs.h"
#include "bitStream.h"
#include "vodCoreException.h"
#include "vod_common.h"

const static int AC3_DESCRIPTOR_TAG = 0x6A;
const static int EAC3_DESCRIPTOR_TAG = 0x7A;

static const int64_t THD_EPS = 100;

bool AC3StreamReader::isPriorityData(AVPacket* packet)
{
    // return packet->size >= 2 && packet->data[0] == 0x0B && packet->data[1] == 0x77 && m_bsid <= 10 && m_state !=
    // stateDecodeTrueHD;
    bool rez = packet->size >= 2 && packet->data[0] == 0x0B && packet->data[1] == 0x77 && m_bsid <= 10;
    if (rez && m_state == stateDecodeTrueHD)
    {
        int gg = 4;
    }
    return rez;
}

void AC3StreamReader::writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket)
{
    if (m_useNewStyleAudioPES)
    {
        // 0f 81 71 - from blu ray DTS-HD
        // 0x01 0x81 0x71 - ordinal DTS == 0x71?
        pesPacket->flagsLo |= 1;  // enable PES extension for AC3 stream
        uint8_t* data = (uint8_t*)(pesPacket) + pesPacket->getHeaderLength();
        *data++ = 0x01;
        *data++ = 0x81;
        if (!m_true_hd_mode || m_downconvertToAC3)
        {
            if (m_bsid > 10)
                *data++ = 0x72;  // E-AC3 subtype
            else
                *data++ = 0x71;  // AC3 subtype
        }
        else
        {
            if (avPacket.flags & AVPacket::IS_CORE_PACKET)
                *data++ = 0x76;  // AC3 at TRUE-HD
            else
                *data++ = 0x72;  // TRUE-HD data
        }
        pesPacket->m_pesHeaderLen += 3;
    }
}

int AC3StreamReader::getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors)
{
    AC3Codec::setTestMode(true);
    uint8_t* frame = findFrame(m_buffer, m_bufEnd);
    if (frame == 0)
        return 0;
    for (int i = 0; i < 2 && frame < m_bufEnd;)
    {
        int skipBytes = 0;
        int skipBeforeBytes = 0;
        int len = decodeFrame(frame, m_bufEnd, skipBytes, skipBeforeBytes);
        if (len < 1)
        {
            // m_state = stateDecodeAC3;
            // AC3Codec::setTestMode(false);
            // return 0;
            break;
        }
        frame += len + skipBytes;
        if (getFrameDurationNano() > 0)
            i++;
    }
    m_state = stateDecodeAC3;
    AC3Codec::setTestMode(false);

    if (isAC3())
    {
        *dstBuff++ = 0x05;  // registration descriptor tag
        *dstBuff++ = 4;
        memcpy(dstBuff, "AC-3", 4);
        dstBuff += 4;

        *dstBuff++ = 0x81;  // AC-3_audio_stream_descriptor( )
        *dstBuff++ = 4;     // descriptor len
        BitStreamWriter bitWriter;
        bitWriter.setBuffer(dstBuff, dstBuff + 4);

        bitWriter.putBits(3, m_fscod);  // bitrate code;
        bitWriter.putBits(5, m_bsidBase);

        bitWriter.putBits(6, m_frmsizecod >> 1);  // // MSB == 0. bit rate is exact
        bitWriter.putBits(2, m_dsurmod);

        bitWriter.putBits(3, m_bsmod);
        bitWriter.putBits(4, m_acmod);  // when MSB == 0 then high (4-th) bit always 0
        bitWriter.putBit(0);            // full_svc

        bitWriter.putBits(8, 0);  // langcod
        bitWriter.flushBits();

        return 12;
    }

    *dstBuff++ = 0x05;  // registration descriptor tag
    *dstBuff++ = 4;
    memcpy(dstBuff, "EAC3", 4);
    dstBuff += 4;

    // ATSC Standard : Digital Audio Compression (AC-3, EAC3) Table G.1
    *dstBuff++ = 0xCC;  // EAC3_audio_stream_descriptor
    *dstBuff++ = 4;     // descriptor len
    BitStreamWriter bitWriter;
    bitWriter.setBuffer(dstBuff, dstBuff + 4);

    bitWriter.putBits(4, 12);
    bitWriter.putBits(1, m_mixinfoexists);
    bitWriter.putBits(3, 0);  // independant substreams not supported

    bitWriter.putBits(5, 24);
    int number_of_channels = (m_acmod == 0 ? 1 : (m_acmod == 1 ? 0 : (m_acmod == 2 ? (m_dsurmod ? 3 : 2) : 4)));
    if (m_extChannelsExists)
        number_of_channels = 5;
    bitWriter.putBits(3, number_of_channels);

    bitWriter.putBits(3, 1);
    bitWriter.putBits(5, m_bsid);

    bitWriter.putBits(8, 0x80);
    bitWriter.flushBits();

    return 12;
}

int AC3StreamReader::readPacket(AVPacket& avPacket)
{
    if (m_true_hd_mode && !m_downconvertToAC3)
        return readPacketTHD(avPacket);
    else
        return SimplePacketizerReader::readPacket(avPacket);
}

int AC3StreamReader::flushPacket(AVPacket& avPacket)
{
    int rez = SimplePacketizerReader::flushPacket(avPacket);
    if (rez > 0 && m_true_hd_mode && !m_downconvertToAC3)
    {
        if (!(avPacket.flags & AVPacket::PRIORITY_DATA))
            avPacket.pts = avPacket.dts = m_totalTHDSamples * 1000000000ll / mh.group1_samplerate -
                                          m_thdFrameOffset;  // replace time to a next HD packet
    }
    return rez;
}

bool AC3StreamReader::needMPLSCorrection() const { return !m_true_hd_mode || m_downconvertToAC3; }

int AC3StreamReader::readPacketTHD(AVPacket& avPacket)
{
    if (m_thdDemuxWaitAc3 && !m_delayedAc3Buffer.isEmpty())
    {
        avPacket = m_delayedAc3Packet;
        m_delayedAc3Buffer.clear();
        m_thdDemuxWaitAc3 = false;
        avPacket.dts = avPacket.pts = m_nextAc3Time;
        avPacket.flags |= AVPacket::IS_CORE_PACKET;
        m_nextAc3Time += m_frameDuration;
        return 0;
    }

    while (1)
    {
        int rez = SimplePacketizerReader::readPacket(avPacket);
        if (rez != 0)
            return rez;

        bool isAc3Packet = (m_state == stateDecodeTrueHDFirst);

        if (isAc3Packet)
        {
            if (m_thdDemuxWaitAc3)
            {
                m_thdDemuxWaitAc3 = false;
                avPacket.dts = avPacket.pts = m_nextAc3Time;
                avPacket.flags |= AVPacket::IS_CORE_PACKET;
                m_nextAc3Time += m_frameDuration;
                return 0;
            }
            else
            {
                if (!m_delayedAc3Buffer.isEmpty())
                {
                    LTRACE(LT_INFO, 2,
                           getCodecInfo().displayName
                               << " stream (track " << m_streamIndex << "): overlapped frame detected at position "
                               << floatToTime((avPacket.pts - PTS_CONST_OFFSET) / 1e9, ',') << ". Remove frame.");
                }

                m_delayedAc3Packet = avPacket;
                m_delayedAc3Buffer.clear();
                m_delayedAc3Buffer.append(avPacket.data, avPacket.size);
                m_delayedAc3Packet.data = m_delayedAc3Buffer.data();
            }
        }
        else
        {
            // thg packet
            avPacket.dts = avPacket.pts = m_totalTHDSamples * 1000000000ll / mh.group1_samplerate - m_thdFrameOffset;

#if 0
            if (m_curMplsIndex >= 0 && avPacket.pts >= m_lastMplsTime && m_curMplsIndex < m_mplsInfo.size()-1) 
            {
                m_curMplsIndex++;
                if (m_mplsInfo[m_curMplsIndex].connection_condition == 5) {
                    int64_t delta = avPacket.pts - m_lastMplsTime;
                    m_thdFrameOffset += delta;
                    avPacket.pts -= delta;
                    avPacket.dts = avPacket.pts;
                    m_nextAc3Time -= delta;
                }
                m_lastMplsTime += (int64_t) ((m_mplsInfo[m_curMplsIndex].OUT_time - m_mplsInfo[m_curMplsIndex].IN_time)*(1000000000/45000.0));
                /*
                if (m_thdFrameOffset >= mh.frame_duration_nano) {
                    m_thdFrameOffset -= mh.frame_duration_nano;
                    continue; // skip thd frame
                }
                */
            }
#endif

            m_totalTHDSamples += mh.access_unit_size;
            m_demuxedTHDSamples += mh.access_unit_size;
            if (m_demuxedTHDSamples >= m_samples)
            {
                m_demuxedTHDSamples -= m_samples;
                m_thdDemuxWaitAc3 = true;
            }
            return 0;
        }
    }
}
