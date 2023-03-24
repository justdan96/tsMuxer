#include "ac3StreamReader.h"

#include <fs/systemlog.h>

#include <sstream>

#include "avCodecs.h"
#include "bitStream.h"
#include "vodCoreException.h"
#include "vod_common.h"

bool AC3StreamReader::isPriorityData(AVPacket* packet)
{
    return (packet->size >= 2 && packet->data[0] == 0x0B && packet->data[1] == 0x77 && m_strmtyp != 1);
}

bool AC3StreamReader::isSecondary() { return m_secondary; };

void AC3StreamReader::writePESExtension(PESPacket* pesPacket, const AVPacket& avPacket)
{
    if (m_useNewStyleAudioPES)
    {
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
        if (getFrameDuration() > 0)
            i++;
    }
    m_state = AC3State::stateDecodeAC3;
    AC3Codec::setTestMode(false);
    BitStreamWriter bitWriter{};

    if (isAC3() || blurayMode)  // no point putting an EAC3 descriptor in blu-ray mode, it won't be recognized
    {
        // ATSC A/52 Annex A Table A3.1 AC-3 Registration Descriptor
        *dstBuff++ = (uint8_t)TSDescriptorTag::REGISTRATION;  // descriptor tag
        *dstBuff++ = 4;                                       // decriptor length
        memcpy(dstBuff, "AC-3", 4);                           // format_identifier
        dstBuff += 4;

        // ATSC A/52 Annex A Table A4.1 AC-3 Audio Descriptor Syntax
        *dstBuff++ = (uint8_t)TSDescriptorTag::AC3;  // AC-3_audio_stream_descriptor
        *dstBuff++ = 4;                              // descriptor len

        bitWriter.setBuffer(dstBuff, dstBuff + 4);

        bitWriter.putBits(3, m_fscod);     // bitrate code
        bitWriter.putBits(5, m_bsidBase);  // 6 = AC3

        bitWriter.putBits(6, m_frmsizecod >> 1);  // MSB == 0. bit rate is exact
        bitWriter.putBits(2, m_dsurmod);

        bitWriter.putBits(3, m_bsmod);
        bitWriter.putBits(4, m_acmod);  // when MSB == 0 then high (4-th) bit always 0
        bitWriter.putBit(0);            // full_svc

        bitWriter.putBits(8, 0);  // langcod
        bitWriter.flushBits();

        return 12;
    }

    // Not AC3 => EAC3
    // ATSC A/52 Annex G 2.EAC3 Registration Descriptor
    *dstBuff++ = (int)TSDescriptorTag::REGISTRATION;  // descriptor tag
    *dstBuff++ = 4;                                   // descriptor length
    memcpy(dstBuff, "EAC3", 4);                       // format_identifier
    dstBuff += 4;

    // ATSC A/52 Annex G Table G.1
    *dstBuff++ = (int)TSDescriptorTag::EAC3;  // EAC3_audio_stream_descriptor
    *dstBuff++ = 4;                           // descriptor len

    bitWriter.setBuffer(dstBuff, dstBuff + 4);

    bitWriter.putBits(4, 12);  // reserved = 1, bsid_flag = 1, mainid_flag = 0, asvc_flag = 0
    bitWriter.putBits(1, m_mixinfoexists);
    bitWriter.putBits(3, 0);  // independant substreams not supported

    bitWriter.putBits(5, 24);  // reserved = 1, full_service_flag = 1, audio_service_type = 0 (Complete Main)
    int number_of_channels = (m_acmod == 0 ? 1 : (m_acmod == 1 ? 0 : (m_acmod == 2 ? (m_dsurmod ? 3 : 2) : 4)));
    if (m_extChannelsExists)
        number_of_channels = 5;
    bitWriter.putBits(3, number_of_channels);

    bitWriter.putBits(3, 1);  // language_flag = 0, language_flag2 = 0, reserved = 1
    bitWriter.putBits(5, m_bsid);

    bitWriter.putBits(8, 0x80);  // additional_info_byte
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
            avPacket.pts = avPacket.dts =
                m_totalTHDSamples * INTERNAL_PTS_FREQ / mlp.m_samplerate;  // replace time to a next HD packet
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

        bool isAc3Packet = (m_state == AC3State::stateDecodeTrueHDFirst);

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
                               << floatToTime((avPacket.pts - PTS_CONST_OFFSET) / (double)INTERNAL_PTS_FREQ, ',')
                               << ". Remove frame.");
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
            avPacket.dts = avPacket.pts = m_totalTHDSamples * INTERNAL_PTS_FREQ / mlp.m_samplerate;

            m_totalTHDSamples += mlp.m_samples;
            m_demuxedTHDSamples += mlp.m_samples;
            if (m_demuxedTHDSamples >= m_samples)
            {
                m_demuxedTHDSamples -= m_samples;
                m_thdDemuxWaitAc3 = true;
            }
            return 0;
        }
    }
}
