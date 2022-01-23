
#ifndef _WIN32
#endif
#include "tsPacket.h"

#include <fs/file.h>
#include <fs/systemlog.h>

#include <string>

#include "bitStream.h"
#include "crc32.h"
#include "h264StreamReader.h"
#include "math.h"
#include "mpegStreamReader.h"
#include "simplePacketizerReader.h"
#include "tsMuxer.h"
#include "vodCoreException.h"

using namespace std;

bool isVideoStreamType(StreamType stream_coding_type)
{
    switch (stream_coding_type)
    {
    case StreamType::VIDEO_MPEG2:
    case StreamType::VIDEO_H264:
    case StreamType::VIDEO_VC1:
    case StreamType::VIDEO_MVC:
    case StreamType::VIDEO_H265:
    case StreamType::VIDEO_H266:
        return true;
    default:
        return false;
    }
}

bool isAudioStreamType(StreamType stream_coding_type)
{
    switch (stream_coding_type)
    {
    case StreamType::AUDIO_LPCM:
    case StreamType::AUDIO_AC3:
    case StreamType::AUDIO_DTS:
    case StreamType::AUDIO_TRUE_HD:
    case StreamType::AUDIO_EAC3:
    case StreamType::AUDIO_DTS_HD:
    case StreamType::AUDIO_DTS_HD_MA:
    case StreamType::AUDIO_EAC3_SECONDARY:
    case StreamType::AUDIO_DTS_HD_SECONDARY:
    case StreamType::AUDIO_AAC:
    case StreamType::AUDIO_AAC_RAW:
    case StreamType::AUDIO_MPEG1:
    case StreamType::AUDIO_MPEG2:
        return true;
    default:
        return false;
    }
}

// ------------ PS PACK -------------------
bool PS_stream_pack::deserialize(uint8_t* buffer, int buf_size)
{
    m_pts = 0;
    BitStreamReader bitReader{};
    bitReader.setBuffer(buffer, buffer + buf_size);
    if (bitReader.getBits(2) != 1)
        return false;  // 0b01 required
    m_pts = ((uint64_t)bitReader.getBits(3) << 30);
    if (bitReader.getBit() != 1)
        return false;
    m_pts += ((uint64_t)bitReader.getBits(15) << 15);
    if (bitReader.getBit() != 1)
        return false;
    m_pts += bitReader.getBits(15);
    if (bitReader.getBit() != 1)
        return false;
    m_pts_ext = bitReader.getBits(9);
    if (bitReader.getBit() != 1)
        return false;
    m_program_mux_rate = bitReader.getBits(22) * 50 * 8;  // convert to bits/sec
    if (bitReader.getBits(2) != 3)
        return false;
    bitReader.skipBits(5);  // reserved
    m_pack_stuffing_length = bitReader.getBits(3);
    return true;
    try
    {
    }
    catch (BitStreamException)
    {
        return false;
    }
}

// ------------- PAT -----------------------

static inline int get16(uint8_t** pp)
{
    uint8_t* p;
    int c;
    p = *pp;
    c = (p[0] << 8) | p[1];
    p += 2;
    *pp = p;
    return c;
}

TS_program_association_section::TS_program_association_section() : transport_stream_id(1)
{
    m_nitPID = -1;
    // av_crc_init(tmpAvCrc, 0, 32, 0x04c11db7, sizeof(AVCRC)*257);
}

bool TS_program_association_section::deserialize(uint8_t* buffer, int buf_size)
{
    m_nitPID = -1;
    buffer++;
    BitStreamReader bitReader{};
    try
    {
        bitReader.setBuffer(buffer, buffer + buf_size);

        int table_id = bitReader.getBits(8);
        if (table_id != 0x0)
            return false;

        int indicator = bitReader.getBits(2);  // section syntax indicator and reserved '0' bit
        if (indicator != 2)
            return false;
        bitReader.skipBits(2);  // reserved

        unsigned section_length = bitReader.getBits(12);
        unsigned crcBit = bitReader.getBitsCount() + (section_length - 4) * 8;

        transport_stream_id = bitReader.getBits(16);
        bitReader.skipBits(2);  // reserved
        bitReader.getBits(5);   // int version_number
        bitReader.getBit();     // int current_next_indicator

        bitReader.getBits(8);  // int section_number
        bitReader.getBits(8);  // int last_section_number

        pmtPids.clear();

        while (bitReader.getBitsCount() < crcBit)
        {
            int program_number = bitReader.getBits(16);
            bitReader.skipBits(3);  // reserved
            int program_pid = bitReader.getBits(13);
            if (program_number != 0)  // not a network pid
                pmtPids[program_pid] = program_number;
            else
                m_nitPID = program_pid;
        }
        // if (get_bits_count(&bitContext) != crcBit)
        if (bitReader.getBitsCount() != crcBit)
            return false;
        // uint32_t crc = bitReader.getBits(32);
        // uint32_t rez = av_crc(tmpAvCrc, -1, buffer, get_bits_count(&bitContext)/8-4);
        // rez = my_htonl(rez);
        return true;
    }
    catch (BitStreamReader&)
    {
        return false;
    }
}

uint32_t TS_program_association_section::serialize(uint8_t* buffer, int buf_size)
{
    buffer[0] = 0;
    buffer++;
    BitStreamWriter bitWriter{};

    // init_bitWriter.putBits(buffer, buf_size*8);
    bitWriter.setBuffer(buffer, buffer + buf_size);

    bitWriter.putBits(8, 0);
    bitWriter.putBits(2, 2);  // indicator
    bitWriter.putBits(2, 3);  // reserved

    auto section_length = (unsigned)(9 + pmtPids.size() * 4);
    bitWriter.putBits(12, section_length);
    bitWriter.putBits(16, transport_stream_id);
    bitWriter.putBits(2, 3);  // reserved
    bitWriter.putBits(5, 0);  // version
    bitWriter.putBits(1, 1);  // current next indicator

    bitWriter.putBits(16, 0);  // section and last section number
    for (std::map<int, int>::const_iterator itr = pmtPids.begin(); itr != pmtPids.end(); ++itr)
    {
        bitWriter.putBits(16, itr->second);  // program number
        bitWriter.putBits(3, 7);             // current next indicator
        bitWriter.putBits(13, itr->first);   // pid
    }
    bitWriter.flushBits();
    uint32_t crc = calculateCRC32(buffer, bitWriter.getBitsCount() / 8);
    uint32_t* crcPtr = (uint32_t*)(buffer + bitWriter.getBitsCount() / 8);
    *crcPtr = my_htonl(crc);

    return bitWriter.getBitsCount() / 8 + 5;
}

// ------------- PMT ------------------------

TS_program_map_section::TS_program_map_section()
    : video_pid(0), audio_pid(0), sub_pid(0), pcr_pid(0), casPID(0), casID(0), program_number(0)
{
    video_type = -1;
    audio_type = -1;
}

bool TS_program_map_section::isFullBuff(uint8_t* buffer, int buf_size)
{
    uint8_t pointerField = *buffer;
    uint8_t* bufEnd = buffer + buf_size;
    BitStreamReader bitReader{};
    try
    {
        bitReader.setBuffer(buffer + 1 + pointerField, buffer + buf_size);

        int table_id = bitReader.getBits(8);
        if (table_id != 0x02)
            return false;

        int indicator = bitReader.getBits(2);  // section syntax indicator and reserved '0' bit
        if (indicator != 2)
            return false;
        bitReader.skipBits(2);  // reserved

        int section_length = bitReader.getBits(12);
        return bitReader.getBuffer() + bitReader.getBitsCount() / 8 + section_length <= bufEnd;
    }
    catch (BitStreamException&)
    {
        return false;
    }
}

void TS_program_map_section::extractPMTDescriptors(uint8_t* curPos, int es_info_len)
{
    uint8_t* end = curPos + es_info_len;
    while (curPos < end)
    {
        auto tag = static_cast<TSDescriptorTag>(*curPos);
        uint8_t len = curPos[1];
        curPos += 2;
        if (tag == TSDescriptorTag::CAS && len >= 4)
        {
            casID = (curPos[0] << 8) + curPos[1];
            casPID = ((curPos[2] & 0x0f) << 8) + curPos[3];
        }
        curPos += len;
    }
}

bool TS_program_map_section::deserialize(uint8_t* buffer, int buf_size)
{
    if (buf_size < 1)
        return false;
    uint8_t pointerField = *buffer;
    uint8_t* bufferEnd = buffer + buf_size;
    BitStreamReader bitReader{};
    try
    {
        bitReader.setBuffer(buffer + 1 + pointerField, buffer + buf_size);

        int table_id = bitReader.getBits(8);
        if (table_id != 0x02)
            return false;

        int indicator = bitReader.getBits(2);  // section syntax indicator and reserved '0' bit
        if (indicator != 2)
            return false;
        bitReader.skipBits(2);  // reserved

        int section_length = bitReader.getBits(12);
        uint8_t* crcPos = bitReader.getBuffer() + bitReader.getBitsCount() / 8 + section_length - 4;
        if (crcPos > bufferEnd)
        {
            LTRACE(LT_WARN, 0, "Bad PMT table. skipped");
            return false;
        }

        program_number = bitReader.getBits(16);
        // reserved, version_number, current_next_indicator, section_number, last_section_number
        bitReader.skipBits(27);
        pcr_pid = bitReader.getBits(13);

        // we set video=pcr pid by default.
        // the video PID is not available in the scrambled channel veriMatrix 239.255.2.58:5500.
        // Is that similar to the es_info_len crap?
        video_pid = pcr_pid;

        bitReader.skipBits(4);  // reserved
        int program_info_len = bitReader.getBits(12);
        uint8_t* curPos = bitReader.getBuffer() + bitReader.getBitsCount() / 8;
        extractPMTDescriptors(curPos, program_info_len);
        curPos += program_info_len;
        while (curPos < crcPos)
        {
            StreamType stream_type = (StreamType)*curPos++;
            int elementary_pid = get16(&curPos) & 0x1fff;
            switch (stream_type)
            {
            case StreamType::VIDEO_MPEG1:
            case StreamType::VIDEO_MPEG2:
            case StreamType::VIDEO_MPEG4:
            case StreamType::VIDEO_H264:
            case StreamType::VIDEO_H265:
            case StreamType::VIDEO_MVC:
            case StreamType::VIDEO_VC1:
                video_pid = elementary_pid;
                video_type = (int)stream_type;
                break;
            case StreamType::AUDIO_MPEG1:
            case StreamType::AUDIO_MPEG2:
            case StreamType::AUDIO_AAC:
            case StreamType::AUDIO_AC3:
            case StreamType::AUDIO_EAC3:
            case StreamType::AUDIO_EAC3_ATSC:
            case StreamType::AUDIO_DTS:
                audio_pid = elementary_pid;
                audio_type = (int)stream_type;
                break;
            case StreamType::SUB_DVB:
                sub_pid = elementary_pid;
                break;
            default:
                break;
            }
            PMTStreamInfo pmtStreamInfo(stream_type, elementary_pid, 0, 0, 0, "", false);
            int es_info_len = get16(&curPos) & 0xfff;
            if (curPos + es_info_len > crcPos)
            {
                LTRACE(LT_WARN, 0, "Bad PMT table. skipped");
                return false;
            }
            extractDescriptors(curPos, es_info_len, pmtStreamInfo);
            pidList[elementary_pid] = pmtStreamInfo;
            curPos += es_info_len;
        }
        if (curPos != crcPos)
            return false;

        uint32_t rez = calculateCRC32(buffer, curPos - buffer);
        rez = my_htonl(rez);
        return true;
    }
    catch (BitStreamException&)
    {
        return false;
    }
}

void TS_program_map_section::extractDescriptors(uint8_t* curPos, int es_info_len, PMTStreamInfo& pmtInfo)
{
    uint8_t* end = curPos + es_info_len;
    while (curPos < end)
    {
        auto tag = static_cast<TSDescriptorTag>(*curPos);
        uint8_t len = curPos[1];
        curPos += 2;
        uint8_t* descrBuf = curPos;
        if (tag == TSDescriptorTag::REGISTRATION)
        {
            descrBuf += 4;
        }
        else if (tag == TSDescriptorTag::LANG)
        {
            for (int i = 0; i < 3; i++) pmtInfo.m_lang[i] = descrBuf[i];
        }
        curPos += len;
    }
}

uint32_t TS_program_map_section::serialize(uint8_t* buffer, int max_buf_size, bool blurayMode, bool hdmvDescriptors)
{
    buffer[0] = 0;
    buffer++;
    BitStreamWriter bitWriter{};
    bitWriter.setBuffer(buffer, buffer + max_buf_size);
    bitWriter.putBits(8, 2);  // table id

    uint16_t* LengthPos1 = (uint16_t*)(bitWriter.getBuffer() + bitWriter.getBitsCount() / 8);
    bitWriter.putBits(2, 2);   // indicator
    bitWriter.putBits(2, 3);   // reserved
    bitWriter.putBits(12, 0);  // length
    int beforeCount1 = bitWriter.getBitsCount() / 8;

    bitWriter.putBits(16, program_number);
    bitWriter.putBits(2, 3);         // reserved
    bitWriter.putBits(5, 0);         // version_number
    bitWriter.putBits(1, 1);         // current next indicator
    bitWriter.putBits(16, 0);        // section_number and last_section_number
    bitWriter.putBits(3, 7);         // reserved
    bitWriter.putBits(13, pcr_pid);  // reserved

    uint16_t* LengthPos2 = (uint16_t*)(bitWriter.getBuffer() + bitWriter.getBitsCount() / 8);
    bitWriter.putBits(4, 15);  // reserved
    bitWriter.putBits(12, 0);  // program info len
    int beforeCount2 = bitWriter.getBitsCount() / 8;

    if (hdmvDescriptors)
    {
        // put 'HDMV' registration descriptor
        bitWriter.putBits(8, (uint8_t)TSDescriptorTag::HDMV);
        bitWriter.putBits(8, 0x04);
        bitWriter.putBits(32, 0x48444d56);

        // put DTCP descriptor
        bitWriter.putBits(8, (uint8_t)TSDescriptorTag::COPY_CONTROL);
        bitWriter.putBits(8, 0x04);
        bitWriter.putBits(32, 0x0ffffcfc);
    }

    if (casPID)
    {
        // put CAS descriptor
        bitWriter.putBits(8, (uint8_t)TSDescriptorTag::CAS);
        bitWriter.putBits(8, 0x04);
        bitWriter.putBits(16, casID);
        bitWriter.putBits(16, casPID);
    }
    *LengthPos2 = my_htons(0xf000 + bitWriter.getBitsCount() / 8 - beforeCount2);

    if (video_pid)
    {
        bitWriter.putBits(8, video_type);
        bitWriter.putBits(3, 7);  // reserved
        bitWriter.putBits(13, video_pid);
        bitWriter.putBits(4, 15);  // reserved
        bitWriter.putBits(12, 0);  // es_info_len
    }

    if (audio_pid)
    {
        bitWriter.putBits(8, audio_type);
        bitWriter.putBits(3, 7);  // reserved
        bitWriter.putBits(13, audio_pid);
        bitWriter.putBits(4, 15);  // reserved
        bitWriter.putBits(12, 0);  // es_info_len
    }

    if (sub_pid)
    {
        bitWriter.putBits(8, (uint8_t)StreamType::SUB_DVB);
        bitWriter.putBits(3, 7);  // reserved
        bitWriter.putBits(13, sub_pid);
        bitWriter.putBits(4, 15);  // reserved
        bitWriter.putBits(12, 0);  // es_info_len
    }

    for (PIDListMap::const_iterator itr = pidList.begin(); itr != pidList.end(); ++itr)
    {
        if (itr->second.m_streamType == StreamType::SUB_PGS && !hdmvDescriptors)
            LTRACE(LT_WARN, 2, "Warning: PGS might not work without HDMV descriptors.");

        bitWriter.putBits(8, (int)itr->second.m_streamType);
        bitWriter.putBits(3, 7);  // reserved
        bitWriter.putBits(13, itr->second.m_pid);

        uint16_t* esInfoLen = (uint16_t*)(bitWriter.getBuffer() + bitWriter.getBitsCount() / 8);
        bitWriter.putBits(4, 15);  // reserved
        bitWriter.putBits(12, 0);  // es_info_len
        int beforeCount = bitWriter.getBitsCount() / 8;

        for (int j = 0; j < itr->second.m_esInfoLen; j++)
            bitWriter.putBits(8, itr->second.m_esInfoData[j]);  // es_info_len

        if (*itr->second.m_lang && !blurayMode)
        {
            bitWriter.putBits(8, (unsigned)TSDescriptorTag::LANG);                    // lang descriptor ID
            bitWriter.putBits(8, 4);                                                  // lang descriptor len
            for (int k = 0; k < 3; k++) bitWriter.putBits(8, itr->second.m_lang[k]);  // lang code[i]
            bitWriter.putBits(8, 0);
        }
        *esInfoLen = my_htons(0xf000 + bitWriter.getBitsCount() / 8 - beforeCount);
    }
    *LengthPos1 = my_htons(0xb000 + bitWriter.getBitsCount() / 8 - beforeCount1 + 4);
    bitWriter.flushBits();

    uint32_t crc = calculateCRC32(buffer, bitWriter.getBitsCount() / 8);

    uint32_t* crcPtr = (uint32_t*)(buffer + bitWriter.getBitsCount() / 8);
    *crcPtr = my_htonl(crc);

    return bitWriter.getBitsCount() / 8 + 5;
}

// --------------------- CLPIParser -----------------------------

void CLPIStreamInfo::ISRC(BitStreamReader& reader)
{
    readString(country_code, reader, 2);
    readString(copyright_holder, reader, 3);
    readString(recording_year, reader, 2);
    readString(recording_number, reader, 5);
}

void CLPIStreamInfo::composeISRC(BitStreamWriter& writer) const
{
    writeString("\x30\x30", writer, 2);              // country_code
    writeString("\x30\x30\x30", writer, 3);          // copyright_holder
    writeString("\x30\x30", writer, 2);              // recording_year
    writeString("\x30\x30\x30\x30\x30", writer, 5);  // recording_number
}

void CLPIStreamInfo::parseStreamCodingInfo(BitStreamReader& reader)
{
    reader.skipBits(8);  // length
    stream_coding_type = (StreamType)reader.getBits(8);

    if (isVideoStreamType(stream_coding_type))
    {
        video_format = reader.getBits(4);
        frame_rate_index = reader.getBits(4);
        aspect_ratio_index = reader.getBits(4);
        reader.skipBits(20);  // reserved_for_future_use, cc_flag
        ISRC(reader);
        reader.skipBits(32);  // reserved_for_future_use
    }
    else if (isAudioStreamType(stream_coding_type))
    {
        audio_presentation_type = reader.getBits(4);
        sampling_frequency_index = reader.getBits(4);
        readString(language_code, reader, 3);
        ISRC(reader);
        reader.skipBits(32);
    }
    else if (stream_coding_type == StreamType::SUB_PGS)
    {
        // Presentation Graphics stream
        readString(language_code, reader, 3);
        reader.skipBits(8);  // reserved_for_future_use
        ISRC(reader);
        reader.skipBits(32);  // reserved_for_future_use
    }
    else if (stream_coding_type == StreamType::SUB_IGS)
    {
        // Interactive Graphics stream
        readString(language_code, reader, 3);
        reader.skipBits(8);  // reserved_for_future_use
        ISRC(reader);
        reader.skipBits(32);  // reserved_for_future_use
    }
    else if (stream_coding_type == StreamType::SUB_TGS)
    {
        // Text subtitle stream
        character_code = reader.getBits(8);
        readString(language_code, reader, 3);
        ISRC(reader);
        reader.skipBits(32);  // reserved_for_future_use
    }
}

void CLPIStreamInfo::composeStreamCodingInfo(BitStreamWriter& writer) const
{
    uint8_t* lengthPos = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, (int)stream_coding_type);

    if (isVideoStreamType(stream_coding_type))
    {
        writer.putBits(4, video_format);
        writer.putBits(4, frame_rate_index);
        writer.putBits(4, aspect_ratio_index);
        writer.putBits(2, 0);  // reserved_for_future_use
        writer.putBit(0);      // cc_flag
        writer.putBit(0);      // reserved
        if (HDR & 18)
            writer.putBits(8, 0x12);  // HDR10 or HDR10plus
        else if (HDR == 4)
            writer.putBits(8, 0x22);  // DV
        else
            writer.putBits(8, 0);
        if (HDR == 16)
            writer.putBits(8, 0x80);  // HDR10plus
        else
            writer.putBits(8, 0);
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use
    }
    else if (isAudioStreamType(stream_coding_type))
    {
        writer.putBits(4, audio_presentation_type);
        writer.putBits(4, sampling_frequency_index);
        writeString(language_code, writer, 3);
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use
    }
    else if (stream_coding_type == StreamType::SUB_PGS)
    {
        // Presentation Graphics stream
        writeString(language_code, writer, 3);
        writer.putBits(8, 0);  // reserved_for_future_use
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use
    }
    else if (stream_coding_type == StreamType::SUB_IGS)
    {
        // Interactive Graphics stream
        writeString(language_code, writer, 3);
        writer.putBits(8, 0);  // reserved_for_future_use
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use
    }
    else if (stream_coding_type == StreamType::SUB_TGS)
    {
        // Text subtitle stream
        writer.putBits(8, character_code);
        writeString(language_code, writer, 3);
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use
    }
    *lengthPos = writer.getBitsCount() / 8 - beforeCount;
}

void CLPIParser::parseProgramInfo(uint8_t* buffer, uint8_t* end, std::vector<CLPIProgramInfo>& programInfoMap,
                                  std::map<int, CLPIStreamInfo>& streamInfoMap)
{
    BitStreamReader reader{};
    reader.setBuffer(buffer, end);
    reader.skipBits(32);  // length
    reader.skipBits(8);   // reserved_for_word_align
    uint8_t number_of_program_sequences = reader.getBits(8);
    for (int i = 0; i < number_of_program_sequences; i++)
    {
        programInfoMap.push_back(CLPIProgramInfo());
        programInfoMap[i].SPN_program_sequence_start = reader.getBits(32);
        programInfoMap[i].program_map_PID = reader.getBits(16);
        programInfoMap[i].number_of_streams_in_ps = reader.getBits(8);
        reader.skipBits(8);
        for (int stream_index = 0; stream_index < programInfoMap[i].number_of_streams_in_ps; stream_index++)
        {
            int pid = reader.getBits(16);
            CLPIStreamInfo streamInfo;
            streamInfo.parseStreamCodingInfo(reader);
            streamInfoMap[pid] = streamInfo;
        }
    }
}

void CLPIParser::composeProgramInfo(BitStreamWriter& writer, bool isSsExt)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length

    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);  // reserved
    writer.putBits(8, 1);  // number_of_program_sequences = 1
    // for (int i=0; i < number_of_program_sequences; i++)
    {
        writer.putBits(32, 0);                // m_programInfo[i].SPN_program_sequence_start
        writer.putBits(16, DEFAULT_PMT_PID);  // m_programInfo[i].program_map_PID =

        int streams = 0;
        for (std::map<int, CLPIStreamInfo>::const_iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
        {
            const CLPIStreamInfo& si = itr->second;
            bool streamOK = (isSsExt && si.stream_coding_type == StreamType::VIDEO_MVC) ||
                            (!isSsExt && si.stream_coding_type != StreamType::VIDEO_MVC);
            if (streamOK)
                streams++;
        }

        writer.putBits(8, streams);  // m_programInfo[i].number_of_streams_in_ps
        writer.putBits(8, 0);        // reserved_for_future_use
        // for (int i=0; i < m_streamInfo.size(); i++)
        for (std::map<int, CLPIStreamInfo>::const_iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
        {
            const CLPIStreamInfo& si = itr->second;
            bool streamOK = (isSsExt && si.stream_coding_type == StreamType::VIDEO_MVC) ||
                            (!isSsExt && si.stream_coding_type != StreamType::VIDEO_MVC);
            if (!streamOK)
                continue;
            writer.putBits(16, itr->first);  // pid
            itr->second.composeStreamCodingInfo(writer);
        }
    }

    if (isSsExt && writer.getBitsCount() % 32 != 0)
        writer.putBits(16, 0);

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::TS_type_info_block(BitStreamReader& reader)
{
    reader.skipBits(24);                                       // length // Validity_flags, 1000 0000b is typical value
    CLPIStreamInfo::readString(format_identifier, reader, 4);  // HDMV
    // Network_information 8*9
    for (int i = 0; i < 9; i++) reader.skipBits(8);
    // Stream_format_name 8*16
    for (int i = 0; i < 4; i++) reader.skipBits(32);
}

void CLPIParser::composeTS_type_info_block(BitStreamWriter& writer)
{
    uint16_t* lengthPos = (uint16_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(16, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0x80);  // Validity_flags
    CLPIStreamInfo::writeString("HDMV", writer, 4);
    // Network_information 8*9
    for (int i = 0; i < 9; i++) writer.putBits(8, 0);
    // Stream_format_name 8*16
    for (int i = 0; i < 4; i++) writer.putBits(32, 0);
    *lengthPos = my_ntohs(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::parseClipInfo(BitStreamReader& reader)
{
    reader.skipBits(32);                   // length
    reader.skipBits(16);                   // reserved_for_future_use
    clip_stream_type = reader.getBits(8);  // 1 - AV stream
    application_type = reader.getBits(8);  // 1 - Main TS for a main-path of Movie
    reader.skipBits(31);                   // reserved_for_future_use
    is_ATC_delta = reader.getBit();
    TS_recording_rate = reader.getBits(32);         // kbps in bytes/sec
    number_of_source_packets = reader.getBits(32);  // number of TS packets?
    for (int i = 0; i < 32; i++) reader.skipBits(32);
    TS_type_info_block(reader);
}

void CLPIParser::composeClipInfo(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(16, 0);                // reserved_for_future_use
    writer.putBits(8, clip_stream_type);  // 1 - AV stream
    writer.putBits(8, application_type);  // 1 - Main TS for a main-path of Movie
    writer.putBits(31, 0);                // reserved_for_future_use
    writer.putBit(is_ATC_delta);
    writer.putBits(32, TS_recording_rate);               // kbps in bytes/sec
    writer.putBits(32, number_of_source_packets);        // number of TS packets?
    for (int i = 0; i < 32; i++) writer.putBits(32, 0);  // reserved
    composeTS_type_info_block(writer);
    if (is_ATC_delta)
        THROW(ERR_COMMON, "CLPI is_ATC_delta is not implemented yet.");
    if (application_type == 6)
        THROW(ERR_COMMON, "CLPI application_type==6 is not implemented yet.");
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::parseSequenceInfo(uint8_t* buffer, uint8_t* end)
{
    BitStreamReader reader{};
    reader.setBuffer(buffer, end);
    reader.skipBits(32);                                  // length
    reader.skipBits(8);                                   // reserved_for_word_align
    uint8_t number_of_ATC_sequences = reader.getBits(8);  // 1 is tipical value
    for (uint8_t atc_id = 0; atc_id < number_of_ATC_sequences; atc_id++)
    {
        reader.skipBits(32);  // SPN_ATC_start, 0 is tipical value
        uint8_t number_of_STC_sequences = reader.getBits(8);
        uint8_t offset_STC_id = reader.getBits(8);
        for (uint8_t stc_id = offset_STC_id; stc_id < number_of_STC_sequences + offset_STC_id; stc_id++)
        {
            reader.skipBits(16);  // PCR_PID
            reader.skipBits(32);  // SPN_STC_start
            presentation_start_time = reader.getBits(32);
            presentation_end_time = reader.getBits(32);
        }
    }
}

void CLPIParser::composeSequenceInfo(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);  // reserved_for_word_align
    writer.putBits(8, 1);  // number_of_ATC_sequences
    // for (int atc_id = 0; atc_id < number_of_ATC_sequences; atc_id++)
    {
        writer.putBits(32, 0);  // SPN_ATC_start
        writer.putBits(8, 1);   // number_of_STC_sequences
        writer.putBits(8, 0);   // offset_STC_id
        // for (int stc_id=offset_STC_id; stc_id < number_of_STC_sequences + offset_STC_id; stc_id++)
        {
            writer.putBits(16, DEFAULT_PCR_PID);
            writer.putBits(32, 0);  // SPN_STC_start
            writer.putBits(32, presentation_start_time);
            writer.putBits(32, presentation_end_time);
        }
    }
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::parseCPI(uint8_t* buffer, uint8_t* end)
{
    BitStreamReader reader{};
    reader.setBuffer(buffer, end);
    if (reader.getBits(32) != 0)  // length
        reader.skipBits(16);      // reserved_for_word_align, CPI_type (1 is typical value)
}

void CLPIParser::EP_map(BitStreamReader& reader) {}

void CLPIParser::composeCPI(BitStreamWriter& writer, bool isCPIExt)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length

    if ((isDependStream && !isCPIExt) || (!isDependStream && isCPIExt))
        return;  // CPI_SS for MVC depend stream only and vice versa: standard CPI for standard video stream

    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(12, 0);  // reserved_for_word_align
    writer.putBits(4, 1);   // CPI_type
    composeEP_map(writer, isCPIExt);

    if (isCPIExt && writer.getBitsCount() % 32 != 0)
        writer.putBits(16, 0);

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::composeEP_map(BitStreamWriter& writer, bool isSSExt)
{
    uint32_t beforeCount = writer.getBitsCount() / 8;
    std::vector<CLPIStreamInfo> processStream;
    int EP_stream_type = 1;
    for (std::map<int, CLPIStreamInfo>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
    {
        StreamType coding_type = itr->second.stream_coding_type;
        if (isSSExt)
        {
            if (coding_type == StreamType::VIDEO_MVC)
                processStream.push_back(itr->second);
        }
        else
        {
            if (coding_type != StreamType::VIDEO_MVC && isVideoStreamType(coding_type))
                processStream.push_back(itr->second);
        }
    }
    if (processStream.size() == 0)
        for (std::map<int, CLPIStreamInfo>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
        {
            StreamType coding_type = itr->second.stream_coding_type;
            if (isAudioStreamType(coding_type))
            {
                processStream.push_back(itr->second);
                if (itr->second.isSecondary)
                    EP_stream_type = 4;
                else
                    EP_stream_type = 3;
                break;
            }
        }
    if (processStream.size() == 0)
        THROW(ERR_COMMON, "Can't create EP map. One audio or video stream is needed.");
    // ------------------
    writer.putBits(8, 0);                          // reserved_for_word_align
    writer.putBits(8, (int)processStream.size());  // number_of_stream_PID_entries
    std::vector<uint32_t*> epStartAddrPos;

    for (auto& i : processStream)
    {
        writer.putBits(16, i.streamPID);  // stream_PID[k]
        writer.putBits(10, 0);            // reserved_for_word_align
        writer.putBits(4, EP_stream_type);
        std::vector<BluRayCoarseInfo> coarseInfo = buildCoarseInfo(i);
        writer.putBits(16, (int)coarseInfo.size());  // number_of_EP_coarse_entries[k]
        if (i.m_index.size() > 0)
            writer.putBits(18, (int)i.m_index[m_clpiNum].size());  // number_of_EP_fine_entries[k]
        else
            writer.putBits(18, 0);
        epStartAddrPos.push_back((uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8));
        writer.putBits(32, 0);  // EP_map_for_one_stream_PID_start_address[k]
    }
    if (writer.getBitsCount() % 16 != 0)
        writer.putBits(8, 0);  // padding_word

    for (size_t i = 0; i < processStream.size(); ++i)
    {
        *epStartAddrPos[i] = my_htonl(writer.getBitsCount() / 8 - beforeCount);
        composeEP_map_for_one_stream_PID(writer, processStream[i]);
        if (writer.getBitsCount() % 16 != 0)
            writer.putBits(8, 0);  // padding_word
    }
}

std::vector<BluRayCoarseInfo> CLPIParser::buildCoarseInfo(M2TSStreamInfo& streamInfo)
{
    std::vector<BluRayCoarseInfo> rez;
    if (streamInfo.m_index.size() == 0)
        return rez;
    uint32_t cnt = 0;
    int64_t lastPktCnt = 0;
    int64_t lastCoarsePts = 0;
    PMTIndex& curIndex = streamInfo.m_index[m_clpiNum];
    for (PMTIndex::const_iterator itr = curIndex.begin(); itr != curIndex.end(); ++itr)
    {
        const PMTIndexData& indexData = itr->second;
        uint32_t newCoarsePts = (uint32_t)(itr->first >> 19);
        uint32_t lastCoarseSPN = lastPktCnt & 0xfffe0000;
        uint32_t newCoarseSPN = indexData.m_pktCnt & 0xfffe0000;
        if (rez.size() == 0 || newCoarsePts != lastCoarsePts || lastCoarseSPN != newCoarseSPN)
        {
            rez.push_back(BluRayCoarseInfo(newCoarsePts, cnt, indexData.m_pktCnt));
        }
        lastCoarsePts = newCoarsePts;
        lastPktCnt = indexData.m_pktCnt;
        cnt++;
    }
    return rez;
}

void CLPIParser::composeEP_map_for_one_stream_PID(BitStreamWriter& writer, M2TSStreamInfo& streamInfo)
{
    uint32_t* epFineStartAddr = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    uint32_t beforePos = writer.getBitsCount() / 8;
    writer.putBits(32, 0);  // EP_fine_table_start_address
    std::vector<BluRayCoarseInfo> coarseInfo = buildCoarseInfo(streamInfo);
    for (auto& i : coarseInfo)
    {
        writer.putBits(18, i.m_fineRefID);  // ref_to_EP_fine_id[i]
        writer.putBits(14, i.m_coarsePts);  // PTS_EP_coarse[i]
        writer.putBits(32, i.m_pktCnt);     // SPN_EP_coarse[i]
    }
    if (writer.getBitsCount() % 16 != 0)
        writer.putBits(8, 0);  // padding_word
    *epFineStartAddr = my_htonl(writer.getBitsCount() / 8 - beforePos);
    if (streamInfo.m_index.size() > 0)
    {
        const PMTIndex& curIndex = streamInfo.m_index[m_clpiNum];
        for (PMTIndex::const_iterator itr = curIndex.begin(); itr != curIndex.end(); ++itr)
        {
            const PMTIndexData& indexData = itr->second;
            writer.putBit(0);  // is_angle_change_point[EP_fine_id]
            int endCode = 0;
            if (indexData.m_frameLen > 0)
            {
                if (is4K())
                {
                    if (indexData.m_frameLen < 786432)
                        endCode = 1;
                    else if (indexData.m_frameLen < 1572864)
                        endCode = 2;
                    else if (indexData.m_frameLen < 2359296)
                        endCode = 3;
                    else if (indexData.m_frameLen < 3145728)
                        endCode = 4;
                    else if (indexData.m_frameLen < 3932160)
                        endCode = 5;
                    else if (indexData.m_frameLen < 4718592)
                        endCode = 6;
                    else
                        endCode = 7;
                }
                else
                {
                    if (indexData.m_frameLen < 131072)
                        endCode = 1;
                    else if (indexData.m_frameLen < 262144)
                        endCode = 2;
                    else if (indexData.m_frameLen < 393216)
                        endCode = 3;
                    else if (indexData.m_frameLen < 589824)
                        endCode = 4;
                    else if (indexData.m_frameLen < 917504)
                        endCode = 5;
                    else if (indexData.m_frameLen < 1310720)
                        endCode = 6;
                    else
                        endCode = 7;
                }
            }
            writer.putBits(3, endCode);                            // I_end_position_offset[EP_fine_id]
            writer.putBits(11, (itr->first >> 9) % 2048);          // PTS_EP_fine[EP_fine_id]
            writer.putBits(17, indexData.m_pktCnt % (65536 * 2));  // SPN_EP_fine[EP_fine_id]
        }
    }
}

void CLPIParser::parseClipMark(uint8_t* buffer, uint8_t* end)
{
    BitStreamReader reader{};
    reader.setBuffer(buffer, end);
    reader.skipBits(32);  // length
}

void CLPIParser::composeClipMark(BitStreamWriter& writer) { writer.putBits(32, 0); }

bool CLPIParser::parse(const char* fileName)
{
    File file;
    if (!file.open(fileName, File::ofRead))
        return false;
    uint64_t fileSize;
    if (!file.size(&fileSize))
        return false;
    uint8_t* buffer = new uint8_t[fileSize];
    if (!file.read(buffer, (uint32_t)fileSize))
    {
        delete[] buffer;
        return false;
    }
    try
    {
        parse(buffer, fileSize);
        delete[] buffer;
        return true;
    }
    catch (...)
    {
        delete[] buffer;
        return false;
    }
}

void CLPIParser::HDMV_LPCM_down_mix_coefficient(uint8_t* buffer, int dataLength) {}

void CLPIParser::Extent_Start_Point(uint8_t* buffer, int dataLength)
{
    BitStreamReader reader{};
    reader.setBuffer(buffer, buffer + dataLength);
    reader.skipBits(32);  // length

    reader.skipBits(16);  // reserved_for_future_use
    int number_of_extent_start_points = reader.getBits(16);
    SPN_extent_start.resize(number_of_extent_start_points);
    for (int i = 0; i < number_of_extent_start_points; ++i)
    {
        SPN_extent_start[i] = reader.getBits(32);
    }
}

void CLPIParser::ProgramInfo_SS(uint8_t* buffer, int dataLength)
{
    parseProgramInfo(buffer, buffer + dataLength, m_programInfoMVC, m_streamInfoMVC);
}

void CLPIParser::CPI_SS(uint8_t* buffer, int dataLength) { parseCPI(buffer, buffer + dataLength); }

void CLPIParser::composeExtentStartPoint(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(16, 0);  // reserved
    writer.putBits(16, (unsigned)interleaveInfo.size());

    uint32_t sum = 0;
    for (auto& i : interleaveInfo)
    {
        sum += i;
        writer.putBits(32, sum);
    }

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::composeExtentInfo(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(32, 0);  // length

    if (interleaveInfo.empty())
        return;

    writer.putBits(32, 0);  // skip data block start address
    writer.putBits(24, 0);  // reserved for world align

    int entries = isDependStream ? 3 : 1;
    writer.putBits(8, entries);

    // write Extent_Start_Point header

    writer.putBits(32, 0x00020004);  // extent start point
    uint32_t* extentStartPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);

    writer.putBits(32, 0);  // skip extent start address
    writer.putBits(32, 0);  // skip extent dataLen

    uint32_t* CPI_SS_StartPos = 0;
    uint32_t* ProgramInfo_StartPos = 0;
    if (isDependStream)
    {
        // write ProgramInfo_SS header
        writer.putBits(32, 0x00020005);  // extent start point
        ProgramInfo_StartPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);

        writer.putBits(32, 0);  // skip extent start address
        writer.putBits(32, 0);  // skip extent dataLen

        // write CPI_SS header
        writer.putBits(32, 0x00020006);  // extent start point
        CPI_SS_StartPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);

        writer.putBits(32, 0);  // skip extent start address
        writer.putBits(32, 0);  // skip extent dataLen
    }

    if (writer.getBitsCount() % 32)
        writer.putBits(16, 0);

    lengthPos[1] =
        my_htonl(writer.getBitsCount() / 8 - beforeCount);  // data_block_start_address, same as extentStart point start

    // write Extent_Start_Point body
    *extentStartPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
    int beforeExtentCount = writer.getBitsCount() / 8;
    composeExtentStartPoint(writer);
    extentStartPos[1] = my_htonl(writer.getBitsCount() / 8 - beforeExtentCount);

    if (isDependStream)
    {
        // write ProgramInfo_SS body
        *ProgramInfo_StartPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
        beforeExtentCount = writer.getBitsCount() / 8;
        composeProgramInfo(writer, true);
        ProgramInfo_StartPos[1] = my_htonl(writer.getBitsCount() / 8 - beforeExtentCount);

        // write CPI_SS body
        *CPI_SS_StartPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
        beforeExtentCount = writer.getBitsCount() / 8;
        composeCPI(writer, true);
        CPI_SS_StartPos[1] = my_htonl(writer.getBitsCount() / 8 - beforeExtentCount);
    }

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount - 4);
}

void CLPIParser::parseExtensionData(uint8_t* buffer, uint8_t* end)
{
    // added for 3D compatibility
    BitStreamReader reader{};
    reader.setBuffer(buffer, end);
    if (reader.getBits(32) == 0)  // length
        return;

    reader.skipBits(32);  // data_block_start_address
    reader.skipBits(24);
    int entries = reader.getBits(8);
    for (int i = 0; i < entries; ++i)
    {
        uint32_t dataID = reader.getBits(32);
        uint32_t dataAddress = reader.getBits(32);
        uint32_t dataLength = reader.getBits(32);

        if (dataAddress + dataLength > (uint32_t)(end - buffer))
        {
            LTRACE(LT_WARN, 2, "Invalid extended clip info entry skipped.");
            continue;
        }

        switch (dataID)
        {
        case 0x00010002:
            HDMV_LPCM_down_mix_coefficient(buffer + dataAddress, dataLength);
            break;
        case 0x00020004:
            Extent_Start_Point(buffer + dataAddress, dataLength);
            break;
        case 0x00020005:
            ProgramInfo_SS(buffer + dataAddress, dataLength);
            break;
        case 0x00020006:
            CPI_SS(buffer + dataAddress, dataLength);
            break;
        }
    }
}

void CLPIParser::parse(uint8_t* buffer, int64_t len)
{
    BitStreamReader reader{};
    try
    {
        reader.setBuffer(buffer, buffer + len);

        CLPIStreamInfo::readString(type_indicator, reader, 4);
        CLPIStreamInfo::readString(version_number, reader, 4);
        uint32_t sequenceInfo_start_address = reader.getBits(32);
        uint32_t programInfo_start_address = reader.getBits(32);
        uint32_t CPI_start_address = reader.getBits(32);
        uint32_t clipMark_start_address = reader.getBits(32);
        uint32_t extensionData_start_address = reader.getBits(32);
        for (int i = 0; i < 3; i++) reader.skipBits(32);  // reserved_for_future_use
        parseClipInfo(reader);
        parseSequenceInfo(buffer + sequenceInfo_start_address, buffer + len);
        parseProgramInfo(buffer + programInfo_start_address, buffer + len, m_programInfo, m_streamInfo);
        parseCPI(buffer + CPI_start_address, buffer + len);
        parseClipMark(buffer + clipMark_start_address, buffer + len);
        if (extensionData_start_address)
            parseExtensionData(buffer + extensionData_start_address, buffer + len);
    }
    catch (BitStreamException&)
    {
        THROW(ERR_COMMON, "Can't parse clip info file: unexpected end of data");
    }
}

int CLPIParser::compose(uint8_t* buffer, int bufferSize)
{
    BitStreamWriter writer{};
    writer.setBuffer(buffer, buffer + bufferSize);
    CLPIStreamInfo::writeString("HDMV", writer, 4);
    CLPIStreamInfo::writeString(version_number, writer, 4);
    uint32_t* sequenceInfo_pos = (uint32_t*)(buffer + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // sequenceInfo_start_address
    uint32_t* programInfo_pos = (uint32_t*)(buffer + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // programInfo_start_address
    uint32_t* CPI_pos = (uint32_t*)(buffer + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // CPI start address
    uint32_t* clipMark_pos = (uint32_t*)(buffer + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // clipMark
    uint32_t* extentInfo_pos = (uint32_t*)(buffer + writer.getBitsCount() / 8);
    writer.putBits(32, 0);                              // extent info
    for (int i = 0; i < 3; i++) writer.putBits(32, 0);  // reserved_for_future_use

    composeClipInfo(writer);
    while (writer.getBitsCount() % 32 != 0) writer.putBits(8, 0);
    *sequenceInfo_pos = my_htonl(writer.getBitsCount() / 8);
    composeSequenceInfo(writer);
    while (writer.getBitsCount() % 32 != 0) writer.putBits(8, 0);

    *programInfo_pos = my_htonl(writer.getBitsCount() / 8);
    composeProgramInfo(writer, false);
    while (writer.getBitsCount() % 32 != 0) writer.putBits(8, 0);

    *CPI_pos = my_htonl(writer.getBitsCount() / 8);
    composeCPI(writer, false);
    while (writer.getBitsCount() % 32 != 0) writer.putBits(8, 0);

    *clipMark_pos = my_htonl(writer.getBitsCount() / 8);
    composeClipMark(writer);
    while (writer.getBitsCount() % 32 != 0) writer.putBits(8, 0);

    *extentInfo_pos = my_htonl(writer.getBitsCount() / 8);
    composeExtentInfo(writer);
    while (writer.getBitsCount() % 32 != 0) writer.putBits(8, 0);

    writer.flushBits();
    return writer.getBitsCount() / 8;
}

// -------------------------- MPLSParser ----------------------------

MPLSParser::MPLSParser()
    : PlayList_playback_type(0),
      playback_count(0),
      number_of_SubPaths(0),
      is_multi_angle(false),
      ref_to_STC_id(0),
      PlayItem_random_access_flag(false),
      number_of_angles(0),
      is_different_audios(false),
      is_seamless_angle_change(false),
      m_chapterLen(0),
      IN_time(0),
      OUT_time(0),
      m_m2tsOffset(0),
      isDependStreamExist(false),
      mvc_base_view_r(false),
      subPath_type(0)
{
    number_of_primary_video_stream_entries = 0;
    number_of_primary_audio_stream_entries = 0;
    number_of_PG_textST_stream_entries = 0;
    number_of_IG_stream_entries = 0;
    number_of_secondary_audio_stream_entries = 0;
    number_of_secondary_video_stream_entries = 0;
    number_of_PiP_PG_textST_stream_entries_plus = 0;
    number_of_DolbyVision_video_stream_entries = 0;
}

bool MPLSParser::parse(const char* fileName)
{
    File file;
    if (!file.open(fileName, File::ofRead))
        return false;
    uint64_t fileSize;
    if (!file.size(&fileSize))
        return false;
    uint8_t* buffer = new uint8_t[fileSize];
    if (!file.read(buffer, (uint32_t)fileSize))
    {
        delete[] buffer;
        return false;
    }
    try
    {
        parse(buffer, (int)fileSize);
        delete[] buffer;
        return true;
    }
    catch (...)
    {
        delete[] buffer;
        return false;
    }
}

void MPLSParser::parse(uint8_t* buffer, int len)
{
    BitStreamReader reader{};
    try
    {
        reader.setBuffer(buffer, buffer + len);
        char type_indicator[5];
        char version_number[5];
        CLPIStreamInfo::readString(type_indicator, reader, 4);
        CLPIStreamInfo::readString(version_number, reader, 4);
        int playList_start_address = reader.getBits(32);
        int playListMark_start_address = reader.getBits(32);
        int extensionData_start_address = reader.getBits(32);
        for (int i = 0; i < 5; i++) reader.skipBits(32);  // reserved_for_future_use
        AppInfoPlayList(reader);
        parsePlayList(buffer + playList_start_address, len - playList_start_address);
        parsePlayListMark(buffer + playListMark_start_address, len - playListMark_start_address);

        if (extensionData_start_address)
        {
            parseExtensionData(buffer + extensionData_start_address, buffer + len);
        }
    }
    catch (BitStreamException&)
    {
        THROW(ERR_COMMON, "Can't parse media playlist file: unexpected end of data");
    }
}

void MPLSParser::SubPath_extension(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);   // reserved
    writer.putBits(8, 8);   // SubPath_type = 8
    writer.putBits(15, 0);  // reserved
    writer.putBit(0);       // is_repeat_SubPath
    writer.putBits(8, 0);   // reserved

    std::vector<PMTIndex> pmtIndexList = getMVCDependStream().m_index;
    writer.putBits(8, (unsigned)pmtIndexList.size());  // number_of_SubPlayItems
    for (size_t i = 0; i < pmtIndexList.size(); ++i) composeSubPlayItem(writer, i, 0, pmtIndexList);

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

int MPLSParser::composeSubPathEntryExtension(uint8_t* buffer, int bufferSize)
{
    BitStreamWriter writer{};
    writer.setBuffer(buffer, buffer + bufferSize);
    try
    {
        uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
        writer.putBits(32, 0);  // length
        int beforeCount = writer.getBitsCount() / 8;

        writer.putBits(16, 1);  // one subpath
        SubPath_extension(writer);

        if (writer.getBitsCount() % 32 != 0)
            writer.putBits(16, 0);

        writer.flushBits();
        *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
        return writer.getBitsCount() / 8;
    }
    catch (...)
    {
        return 0;
    }
}

int MPLSParser::composeSTN_tableSS(uint8_t* buffer, int bufferSize)
{
    BitStreamWriter writer{};
    writer.setBuffer(buffer, buffer + bufferSize);
    try
    {
        MPLSStreamInfo streamInfoMVC = getMVCDependStream();
        for (size_t PlayItem_id = 0; PlayItem_id < streamInfoMVC.m_index.size(); PlayItem_id++)
        {
            composeSTN_table(writer, PlayItem_id, true);
            // connection_condition = 6;
        }
        writer.flushBits();
        return writer.getBitsCount() / 8;
    }
    catch (...)
    {
        return 0;
    }
};

int MPLSParser::composeUHD_metadata(uint8_t* buffer, int bufferSize)
{
    BitStreamWriter writer{};
    writer.setBuffer(buffer, buffer + bufferSize);
    try
    {
        writer.putBits(32, 0x20);
        writer.putBits(32, 1 << 24);
        writer.putBits(32, 1 << 28);
        for (int i = 0; i < 6; i++) writer.putBits(32, HDR10_metadata[i]);
        writer.flushBits();
        return writer.getBitsCount() / 8;
    }
    catch (...)
    {
        return 0;
    }
}

int MPLSParser::compose(uint8_t* buffer, int bufferSize, DiskType dt)
{
    for (auto& i : m_streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (isVideoStreamType(stream_coding_type))
        {
            if (i.isSecondary)
            {
                number_of_SubPaths++;
                subPath_type = 7;  // PIP not fully implemented yet
            }
            else if (i.HDR & 4)
            {
                number_of_SubPaths++;
                subPath_type = 10;
            }
        }
    }

    BitStreamWriter writer{};
    writer.setBuffer(buffer, buffer + bufferSize);

    std::string type_indicator = "MPLS";
    std::string version_number;
    if (dt == DiskType::BLURAY)
        version_number = (isV3() ? "0300" : "0200");
    else
        version_number = "0100";
    CLPIStreamInfo::writeString(type_indicator.c_str(), writer, 4);
    CLPIStreamInfo::writeString(version_number.c_str(), writer, 4);
    uint32_t* playList_bit_pos = (uint32_t*)(buffer + writer.getBitsCount() / 8);
    writer.putBits(32, 0);
    uint32_t* playListMark_bit_pos = (uint32_t*)(buffer + writer.getBitsCount() / 8);
    writer.putBits(32, 0);
    uint32_t* extDataStartAddr = (uint32_t*)(buffer + writer.getBitsCount() / 8);
    writer.putBits(32, 0);                              // extension data start address
    for (int i = 0; i < 5; i++) writer.putBits(32, 0);  // reserved_for_future_use
    composeAppInfoPlayList(writer);

    if (writer.getBitsCount() % 16 != 0)
        writer.putBits(8, 0);
    *playList_bit_pos = my_htonl(writer.getBitsCount() / 8);
    composePlayList(writer);

    if (writer.getBitsCount() % 16 != 0)
        writer.putBits(8, 0);
    *playListMark_bit_pos = my_htonl(writer.getBitsCount() / 8);
    composePlayListMark(writer);

    if (writer.getBitsCount() % 16 != 0)
        writer.putBits(8, 0);

    if (number_of_SubPaths > 0 || isDependStreamExist || isV3())
    {
        *extDataStartAddr = my_htonl(writer.getBitsCount() / 8);
        uint8_t buffer[1024 * 4];
        MPLSStreamInfo& mainStreamInfo = getMainStream();
        vector<ExtDataBlockInfo> blockVector;

        if (number_of_SubPaths > 0 && subPath_type == 7)
        {
            int bufferSize = composePip_metadata(buffer, sizeof(buffer), mainStreamInfo.m_index);
            ExtDataBlockInfo extDataBlock(buffer, bufferSize, 1, 1);
            blockVector.push_back(extDataBlock);
        }

        if (isDependStreamExist)
        {
            int bufferSize = composeSTN_tableSS(buffer, sizeof(buffer));
            ExtDataBlockInfo extDataBlock(buffer, bufferSize, 2, 1);
            blockVector.push_back(extDataBlock);

            bufferSize = composeSubPathEntryExtension(buffer, sizeof(buffer));
            ExtDataBlockInfo extDataBlock2(buffer, bufferSize, 2, 2);
            blockVector.push_back(extDataBlock2);
        }

        if (isV3())
        {
            int bufferSize = composeUHD_metadata(buffer, sizeof(buffer));
            ExtDataBlockInfo extDataBlock(buffer, bufferSize, 3, 5);
            blockVector.push_back(extDataBlock);
        }

        composeExtensionData(writer, blockVector);
        if (writer.getBitsCount() % 16 != 0)
            writer.putBits(8, 0);
    }

    writer.flushBits();
    return writer.getBitsCount() / 8;
}

void MPLSParser::AppInfoPlayList(BitStreamReader& reader)
{
    reader.skipBits(32);  // length
    reader.skipBits(8);   // reserved_for_future_use
    PlayList_playback_type = reader.getBits(8);
    if (PlayList_playback_type == 2 || PlayList_playback_type == 3)
    {  // 1 == Sequential playback of PlayItems
        playback_count = reader.getBits(16);
    }
    else
    {
        reader.skipBits(16);  // reserved_for_future_use
    }
    UO_mask_table(reader);
    reader.skipBits(3);  // PlayList_random_access_flag, audio_mix_app_flag, lossless_may_bypass_mixer_flag
    mvc_base_view_r = reader.getBit();
    reader.skipBits(12);  // reserved_for_future_use
}

void MPLSParser::composeAppInfoPlayList(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);  // reserved_for_future_use
    writer.putBits(8, PlayList_playback_type);
    if (PlayList_playback_type == 2 || PlayList_playback_type == 3)
    {  // 1 == Sequential playback of PlayItems
        writer.putBits(16, playback_count);
    }
    else
    {
        writer.putBits(16, 0);  // reserved_for_future_use
    }
    writer.putBits(28, 0);               // UO_mask_table;
    writer.putBits(4, isV3() ? 15 : 0);  // UO_mask_table;
    writer.putBit(0);                    // reserved
    writer.putBit(isV3() ? 1 : 0);       // UO_mask_table: SecondaryPGStreamNumberChange
    writer.putBits(30, 0);               // UO_mask_table cont;
    writer.putBit(0);                    // PlayList_random_access_flag
    writer.putBit(1);  // audio_mix_app_flag. 0 == no secondary audio, 1- allow secondary audio if exist
    writer.putBit(0);  // lossless_may_bypass_mixer_flag
    writer.putBit(mvc_base_view_r);
    writer.putBits(12, 0);  // reserved_for_future_use
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::UO_mask_table(BitStreamReader& reader)
{
    reader.skipBits(32);  // UO_mask_table;
    reader.skipBits(32);  // UO_mask_table cont;
}

void MPLSParser::parsePlayList(uint8_t* buffer, int len)
{
    BitStreamReader reader{};
    reader.setBuffer(buffer, buffer + len);
    reader.skipBits(32);  // length
    reader.skipBits(16);  // reserved_for_future_use
    int number_of_PlayItems = reader.getBits(16);
    number_of_SubPaths = reader.getBits(16);
    for (int PlayItem_id = 0; PlayItem_id < number_of_PlayItems; PlayItem_id++)
    {
        parsePlayItem(reader, PlayItem_id);
    }
    for (int SubPath_id = 0; SubPath_id < number_of_SubPaths; SubPath_id++)
    {
        // SubPath(); // not implemented now
    }
}

MPLSStreamInfo& MPLSParser::getMainStream()
{
    for (auto& i : m_streamInfo)
    {
        StreamType coding_type = i.stream_coding_type;
        if (isVideoStreamType(coding_type))
            return i;
    }
    for (auto& i : m_streamInfo)
    {
        StreamType coding_type = i.stream_coding_type;
        if (isAudioStreamType(coding_type))
            return i;
    }
    THROW(ERR_COMMON, "Can't find stream index. One audio or video stream is needed.");
}

int MPLSParser::pgIndexToFullIndex(int value)
{
    int cnt = 0;
    for (size_t i = 0; i < m_streamInfo.size(); ++i)
    {
        if (m_streamInfo[i].stream_coding_type == StreamType::SUB_PGS)
        {
            if (cnt++ == value)
                return (int)i;
        }
    }
    return -1;
}

MPLSStreamInfo MPLSParser::getStreamByPID(int pid) const
{
    for (auto& i : m_streamInfo)
    {
        if (i.streamPID == pid)
            return i;
    }
    return MPLSStreamInfo();
}

std::vector<MPLSStreamInfo> MPLSParser::getPgStreams() const
{
    std::vector<MPLSStreamInfo> pgStreams;
    for (auto& i : m_streamInfo)
    {
        StreamType coding_type = i.stream_coding_type;
        if (coding_type == StreamType::SUB_PGS)
            pgStreams.push_back(i);
    }
    return pgStreams;
}

MPLSStreamInfo& MPLSParser::getMVCDependStream()
{
    for (auto& i : m_streamInfoMVC)
    {
        StreamType coding_type = i.stream_coding_type;
        if (coding_type == StreamType::VIDEO_MVC)
            return i;
    }
    THROW(ERR_COMMON, "Can't find stream index. One audio or video stream is needed.");
}

void MPLSParser::composePlayList(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;
    writer.putBits(16, 0);  // reserved_for_future_use
    MPLSStreamInfo& mainStreamInfo = getMainStream();
    writer.putBits(16, (unsigned)mainStreamInfo.m_index.size());  // number_of_PlayItems
    writer.putBits(16, number_of_SubPaths);                       // number_of_SubPaths
    // connection_condition = 1;
    for (size_t PlayItem_id = 0; PlayItem_id < mainStreamInfo.m_index.size(); PlayItem_id++)
    {
        composePlayItem(writer, PlayItem_id, mainStreamInfo.m_index);
        // connection_condition = 6;
    }

    MPLSStreamInfo& dependStreamInfo = mainStreamInfo;

    for (size_t SubPath_id = 0; SubPath_id < number_of_SubPaths * dependStreamInfo.m_index.size(); SubPath_id++)
    {
        composeSubPath(writer, SubPath_id, dependStreamInfo.m_index, subPath_type);  // pip
    }

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::composeSubPath(BitStreamWriter& writer, size_t subPathNum, std::vector<PMTIndex>& pmtIndexList,
                                int type)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);     // reserved_for_future_use
    writer.putBits(8, type);  // SubPath_type = 7 (In-mux and Synchronous type of Picture-in-Picture), 5
                              // - MVC depend stream
    writer.putBits(15, 0);    // reserved_for_future_use
    writer.putBits(1, 0);     // is_repeat_SubPath = false
    writer.putBits(8, 0);     // reserved_for_future_use

    writer.putBits(8, (unsigned)pmtIndexList.size());  // number_of_SubPlayItems
    for (size_t i = 0; i < pmtIndexList.size(); i++)
    {
        composeSubPlayItem(writer, i, subPathNum, pmtIndexList);
    }
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::composeSubPlayItem(BitStreamWriter& writer, size_t playItemNum, size_t subPathNum,
                                    std::vector<PMTIndex>& pmtIndexList)
{
    uint16_t* lengthPos = (uint16_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(16, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;

    int fileNum = (int)playItemNum;
    if (isDependStreamExist)
    {
        fileNum *= 2;
        fileNum++;
    }

    std::string clip_Information_file_name = strPadLeft(int32ToStr(fileNum + m_m2tsOffset), 5, '0');
    CLPIStreamInfo::writeString(clip_Information_file_name.c_str(), writer, 5);
    char clip_codec_identifier[] = "M2TS";
    CLPIStreamInfo::writeString(clip_codec_identifier, writer, 4);
    int connection_condition = playItemNum == 0 ? 1 : 6;
    writer.putBits(27, 0);  // reserved_for_future_use
    writer.putBits(4, connection_condition);
    writer.putBit(0);  // is_multi_Clip_entries
    writer.putBits(8, ref_to_STC_id);

    if (playItemNum == 0)
        writer.putBits(32, IN_time);
    else if (pmtIndexList[playItemNum - 1].size() > 0)
        writer.putBits(32, (unsigned)(pmtIndexList[playItemNum].begin()->first / 2));
    else
        writer.putBits(32, IN_time);

    if (playItemNum == pmtIndexList.size() - 1)
        writer.putBits(32, OUT_time);
    else if (pmtIndexList[playItemNum + 1].size() > 0)
        writer.putBits(32, (unsigned)(pmtIndexList[playItemNum + 1].begin()->first / 2));
    else
        writer.putBits(32, OUT_time);

    writer.putBits(16, (unsigned)playItemNum);  // sync_PlayItem_id. reference to play_item id.
    // sync_start_PTS_of_PlayItem
    if (playItemNum == 0)
        writer.putBits(32, IN_time);
    else if (pmtIndexList[playItemNum - 1].size() > 0)
        writer.putBits(32, (unsigned)pmtIndexList[playItemNum].begin()->first / 2);
    else
        writer.putBits(32, IN_time);

    // writer.flushBits();
    *lengthPos = my_htons(writer.getBitsCount() / 8 - beforeCount);
}

int MPLSParser::composePip_metadata(uint8_t* buffer, int bufferSize, std::vector<PMTIndex>& pmtIndexList)
{
    // The ID1 value and the ID2 value of the ExtensionData() shall be set to 0x0001 and 0x0001
    BitStreamWriter writer{};
    writer.setBuffer(buffer, buffer + bufferSize);
    uint32_t* lengthPos = (uint32_t*)buffer;
    writer.putBits(32, 0);  // length

    vector<MPLSStreamInfo> pipStreams;
    int mainVSize = 0;
    int mainHSize = 0;
    for (auto& i : m_streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (isVideoStreamType(stream_coding_type))
        {
            if (i.isSecondary)
            {
                pipStreams.push_back(i);
            }
            else
            {
                mainHSize = i.width;
                mainVSize = i.height;
            }
        }
    }

    auto pipStreamsSize = (unsigned)pipStreams.size();
    auto pmtIndexListSize = (unsigned)pmtIndexList.size();

    writer.putBits(16, pipStreamsSize * pmtIndexListSize);
    vector<uint32_t*> blockDataAddressPos;
    for (unsigned i = 0; i < pmtIndexListSize; ++i)
    {
        for (unsigned k = 0; k < pipStreamsSize; k++)
        {
            PIPParams pipParams = pipStreams[k].pipParams;
            // metadata_block_header[k]() {
            writer.putBits(16, i);  // ref_to_PlayItem_id
            writer.putBits(8, k);   // ref_to_secondary_video_stream_id
            writer.putBits(8, 0);   // reserved_for_future_use
            // pip_timeline_type == 1. Synchronous type of Picture-in-Picture
            writer.putBits(4, pipParams.lumma >= 0 ? 1 : 0);
            writer.putBit(1);       // is_luma_key = 0
            writer.putBit(1);       // trick_playing_flag. keep PIP windows when tricking
            writer.putBits(10, 0);  // reserved_for_word_align
            writer.putBits(8, 0);   // reserved_for_future_use
            if (pipParams.lumma >= 0)
            {                                        // is_luma_key==1b
                writer.putBits(8, pipParams.lumma);  // transparent Y pixels
            }
            else
            {
                writer.putBits(8, 0);  // reserved_for_future_use
            }
            writer.putBits(16, 0);  // reserved_for_future_use
            blockDataAddressPos.push_back((uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8));
            writer.putBits(32, 0);  // metadata_block_data_start_address
        }
    }
    while (writer.getBitsCount() % 16 != 0) writer.putBit(0);
    for (size_t i = 0; i < pmtIndexList.size(); ++i)
    {
        for (size_t k = 0; k < pipStreams.size(); ++k)
        {
            PIPParams pipParams = pipStreams[k].pipParams;

            *(blockDataAddressPos[i * pipStreams.size() + k]) = my_htonl(writer.getBitsCount() / 8);

            writer.putBits(16, 1);  // number_of_pip_metadata_entries
            {
                if (i == 0)
                    writer.putBits(32, IN_time);
                else if (pmtIndexList[i - 1].size() > 0)
                    writer.putBits(32, (unsigned)(pmtIndexList[i].begin()->first / 2));
                else
                    writer.putBits(32, IN_time);

                int hPos = 0;
                int vPos = 0;

                if (!pipParams.isFullScreen())
                {
                    hPos = pipParams.hOffset;
                    vPos = pipParams.vOffset;

                    int pipWidth = (int)(pipStreams[k].width * pipParams.getScaleCoeff());
                    int pipHeight = (int)(pipStreams[k].height * pipParams.getScaleCoeff());

                    if (pipParams.corner == PIPParams::PipCorner::TopRight ||
                        pipParams.corner == PIPParams::PipCorner::BottomRight)
                        hPos = mainHSize - pipWidth - pipParams.hOffset;
                    if (pipParams.corner == PIPParams::PipCorner::BottomRight ||
                        pipParams.corner == PIPParams::PipCorner::BottomLeft)
                        vPos = mainVSize - pipHeight - pipParams.vOffset;
                }

                writer.putBits(12, hPos);
                writer.putBits(12, vPos);

                writer.putBits(4, pipParams.scaleIndex);  // pip_scale[i]. 1 == no_scale
                writer.putBits(4, 0);                     // reserved_for_future_use
            }
            while (writer.getBitsCount() % 16 != 0) writer.putBit(0);
        }
    }

    writer.flushBits();
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - 4);
    return writer.getBitsCount() / 8;
}

void MPLSParser::parseStnTableSS(uint8_t* data, int dataLength)
{
    try
    {
        BitStreamReader reader{};
        reader.setBuffer(data, data + dataLength);
        reader.skipBits(32);  // len, fixedOffsetDuringPopup, reserved

        for (int i = 0; i < number_of_primary_video_stream_entries; ++i)
        {
            MPLSStreamInfo streamInfo;

            streamInfo.parseStreamEntry(reader);

            reader.skipBits(32);  // attrSSLen, coding type 0x20, format, frameRate
            reader.skipBits(32);  // reserved, number_of_offset_sequences
        }

        for (int i = 0; i < number_of_PG_textST_stream_entries; ++i)
        {
            int PG_textST_offset_sequence_id = reader.getBits(8);
            int idx = pgIndexToFullIndex(i);
            if (idx != -1)
                m_streamInfo[idx].offsetId = PG_textST_offset_sequence_id;

            reader.skipBits(5);  // reserved, dialog_region_offset_valid
            m_streamInfo[idx].isSSPG = reader.getBit();
            bool isTopAS = reader.getBit();
            bool isBottomAS = reader.getBit();
            if (m_streamInfo[idx].isSSPG)
            {
                m_streamInfo[idx].leftEye = new MPLSStreamInfo();
                m_streamInfo[idx].leftEye->parseStreamEntry(reader);  // left eye
                m_streamInfo[idx].rightEye = new MPLSStreamInfo();
                m_streamInfo[idx].rightEye->parseStreamEntry(reader);  // right eye
                reader.skipBits(8);                                    // reserved
                m_streamInfo[idx].SS_PG_offset_sequence_id = reader.getBits(8);
            }
            if (isTopAS)
            {
                MPLSStreamInfo streamInfo;
                streamInfo.parseStreamEntry(reader);  // left eye
                reader.skipBits(16);                  // reserved, top_AS_offset_sequence_id
            }
            if (isBottomAS)
            {
                MPLSStreamInfo streamInfo;
                streamInfo.parseStreamEntry(reader);  // left eye
                reader.skipBits(16);                  // reserved, bottom_AS_offset_sequence_id
            }
        }
    }
    catch (...)
    {
    }
}

void MPLSParser::parseSubPathEntryExtension(uint8_t* data, int dataLen)
{
    BitStreamReader reader{};
    reader.setBuffer(data, data + dataLen);
    try
    {
        reader.skipBits(32);  // length
        uint16_t size = reader.getBits(16);
        // for (int i = 0; i < size; ++i)
        if (size > 0)
        {
            // subpath extension
            reader.skipBits(32);  // length
            reader.skipBits(8);   // reserved field
            int type = reader.getBits(8);
            if (type == 8 || type == 9)
            {
                reader.skipBits(24);
                int subPlayItems = reader.getBits(8);
                for (int i = 0; i < subPlayItems; ++i)
                {
                    reader.skipBits(16);
                    char clip_Information_file_name[6];
                    CLPIStreamInfo::readString(clip_Information_file_name, reader, 5);
                    m_mvcFiles.push_back(clip_Information_file_name);
                    reader.skipBits(32);  // clip codec identifier
                    reader.skipBits(31);  // reserved, condition
                    bool isMulticlip = reader.getBit();
                    reader.skipBits(8);   // ref to stc id
                    reader.skipBits(32);  // in time
                    reader.skipBits(32);  // out time
                    reader.skipBits(16);  // sync play item id
                    reader.skipBits(32);  // sync start
                    if (isMulticlip)
                    {
                        int numberOfClipEntries = reader.getBits(8);
                        reader.skipBits(8);  // reserved
                        for (int j = 1; j < numberOfClipEntries; ++j)
                        {
                            CLPIStreamInfo::readString(clip_Information_file_name, reader, 5);
                            m_mvcFiles.push_back(clip_Information_file_name);
                            reader.skipBits(32);  // clip codec identifier
                            reader.skipBits(8);   // ref to stc id
                        }
                    }
                }
            }
        }
    }
    catch (...)
    {
    }
}

void MPLSParser::parseExtensionData(uint8_t* data, uint8_t* dataEnd)
{
    try
    {
        BitStreamReader reader{};
        reader.setBuffer(data, dataEnd);
        if (reader.getBits(32) == 0)  // length
            return;

        reader.skipBits(32);  // data_block_start_address
        reader.skipBits(24);
        int entries = reader.getBits(8);
        for (int i = 0; i < entries; ++i)
        {
            uint32_t dataID = reader.getBits(32);
            uint32_t dataAddress = reader.getBits(32);
            uint32_t dataLength = reader.getBits(32);

            if (dataAddress + dataLength > (uint32_t)(dataEnd - data))
            {
                LTRACE(LT_WARN, 2, "Invalid playlist extension entry skipped.");
                continue;
            }

            if (dataAddress + dataLength > (uint32_t)(dataEnd - data))
                continue;  // invalid entry

            switch (dataID)
            {
            case 0x00020001:
                // stn table ss
                isDependStreamExist = true;
                parseStnTableSS(data + dataAddress, dataLength);
                break;
            case 0x00020002:
                // stn table ss
                isDependStreamExist = true;
                parseSubPathEntryExtension(data + dataAddress, dataLength);
            }
        }
    }
    catch (...)
    {
    }
}

void MPLSParser::composeExtensionData(BitStreamWriter& writer, vector<ExtDataBlockInfo>& extDataBlockInfo)
{
    vector<uint32_t*> extDataStartAddrPos;
    extDataStartAddrPos.resize(extDataBlockInfo.size());
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int initPos = writer.getBitsCount() / 8;
    if (extDataBlockInfo.size() > 0)
    {
        writer.putBits(32, 0);  // data_block_start_address
        writer.putBits(24, 0);  // reserved_for_word_align
        auto extDataBlockInfoSize = (unsigned)extDataBlockInfo.size();
        writer.putBits(8, extDataBlockInfoSize);
        for (unsigned i = 0; i < extDataBlockInfoSize; ++i)
        {
            writer.putBits(16, extDataBlockInfo[i].id1);
            writer.putBits(16, extDataBlockInfo[i].id2);
            extDataStartAddrPos[i] = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
            writer.putBits(32, 0);                                          // ext_data_start_address
            writer.putBits(32, (unsigned)extDataBlockInfo[i].data.size());  // ext_data_length
        }
        while ((writer.getBitsCount() / 8 - initPos) % 4 != 0) writer.putBits(16, 0);
        *(lengthPos + 1) = my_htonl(writer.getBitsCount() / 8 - initPos + 4);  // data_block_start_address
        for (unsigned i = 0; i < extDataBlockInfoSize; ++i)
        {
            *(extDataStartAddrPos[i]) = my_htonl(writer.getBitsCount() / 8 - initPos + 4);
            for (auto& j : extDataBlockInfo[i].data) writer.putBits(8, j);
        }
    }

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - initPos);
}

void MPLSParser::parsePlayItem(BitStreamReader& reader, int PlayItem_id)
{
    MPLSPlayItem newItem;
    reader.skipBits(16);  // length
    char clip_Information_file_name[6];
    char clip_codec_identifier[5];
    CLPIStreamInfo::readString(clip_Information_file_name, reader, 5);
    newItem.fileName = clip_Information_file_name;
    CLPIStreamInfo::readString(clip_codec_identifier, reader, 4);
    reader.skipBits(11);  // reserved_for_future_use
    is_multi_angle = reader.getBit();
    newItem.connection_condition = reader.getBits(4);
    ref_to_STC_id = reader.getBits(8);

    newItem.IN_time = reader.getBits(32);
    newItem.OUT_time = reader.getBits(32);
    m_playItems.push_back(newItem);

    UO_mask_table(reader);
    PlayItem_random_access_flag = reader.getBit();
    reader.skipBits(31);  // reserved_for_future_use, still_time

    if (is_multi_angle == 1)
    {
        number_of_angles = reader.getBits(8);
        reader.skipBits(6);  // reserved_for_future_use
        is_different_audios = reader.getBit();
        is_seamless_angle_change = reader.getBit();
        for (int angle_id = 1;  // angles except angle_id=0
             angle_id < number_of_angles; angle_id++)
        {
            CLPIStreamInfo::readString(clip_Information_file_name, reader, 5);
            CLPIStreamInfo::readString(clip_codec_identifier, reader, 4);
            ref_to_STC_id = reader.getBits(8);
        }
    }
    STN_table(reader, PlayItem_id);
}

void MPLSParser::composePlayItem(BitStreamWriter& writer, size_t playItemNum, std::vector<PMTIndex>& pmtIndexList)
{
    uint16_t* lengthPos = (uint16_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(16, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;
    int fileNum = (int)playItemNum;
    if (isDependStreamExist)
        fileNum *= 2;
    std::string clip_Information_file_name = strPadLeft(int32ToStr(fileNum + m_m2tsOffset), 5u, '0');
    CLPIStreamInfo::writeString(clip_Information_file_name.c_str(), writer, 5);
    char clip_codec_identifier[] = "M2TS";
    CLPIStreamInfo::writeString(clip_codec_identifier, writer, 4);
    writer.putBits(11, 0);  // reserved_for_future_use
    writer.putBit(0);       // is_multi_angle
    int connection_condition = playItemNum == 0 ? 1 : 6;
    writer.putBits(4, connection_condition);
    writer.putBits(8, ref_to_STC_id);
    if (playItemNum == 0)
        writer.putBits(32, IN_time);
    else if (pmtIndexList[playItemNum - 1].size() > 0)
        writer.putBits(32, (unsigned)pmtIndexList[playItemNum].begin()->first / 2);
    else
        writer.putBits(32, IN_time);

    if (playItemNum == pmtIndexList.size() - 1)
        writer.putBits(32, OUT_time);
    else if (pmtIndexList[playItemNum + 1].size() > 0)
        writer.putBits(32, (unsigned)(pmtIndexList[playItemNum + 1].begin()->first / 2));
    else
        writer.putBits(32, OUT_time);

    writer.putBits(28, 0);               // UO_mask_table;
    writer.putBits(4, isV3() ? 15 : 0);  // UO_mask_table;
    writer.putBit(0);                    // reserved
    writer.putBit(isV3() ? 1 : 0);       // UO_mask_table: SecondaryPGStreamNumberChange
    writer.putBits(30, 0);               // UO_mask_table cont;

    writer.putBit(PlayItem_random_access_flag);
    writer.putBits(7, 0);   // reserved_for_future_use
    writer.putBits(8, 0);   // still_mode
    writer.putBits(16, 0);  // reserved after stillMode != 0x01
    composeSTN_table(writer, playItemNum, false);
    *lengthPos = my_htons(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::parsePlayListMark(uint8_t* buffer, int len)
{
    BitStreamReader reader{};
    reader.setBuffer(buffer, buffer + len);
    reader.skipBits(32);  // length
    int number_of_PlayList_marks = reader.getBits(16);
    for (int PL_mark_id = 0; PL_mark_id < number_of_PlayList_marks; PL_mark_id++)
    {
        reader.skipBits(8);  // reserved_for_future_use
        int mark_type = reader.getBits(8);
        int ref_to_PlayItem_id = reader.getBits(16);
        uint32_t mark_time_stamp = reader.getBits(32);
        reader.skipBits(16);  // entry_ES_PID
        reader.skipBits(32);  // duration
        if (mark_type == 1)   // mark_type 0x01 = Chapter search
            m_marks.push_back(PlayListMark(ref_to_PlayItem_id, mark_time_stamp));
    }
}

int MPLSParser::calcPlayItemID(MPLSStreamInfo& streamInfo, uint32_t pts)
{
    for (size_t i = 0; i < streamInfo.m_index.size(); i++)
    {
        if (streamInfo.m_index[i].size() > 0)
        {
            if (streamInfo.m_index[i].begin()->first > pts)
                return FFMAX((int)i, 1) - 1;
        }
    }
    return (int)streamInfo.m_index.size() - 1;
}

void MPLSParser::composePlayListMark(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;
    MPLSStreamInfo& streamInfo = MPLSParser::getMainStream();
    if (m_marks.size() == 0)
    {
        if (m_chapterLen == 0)
            m_marks.push_back(PlayListMark(-1, IN_time));
        else
        {
            for (uint32_t i = IN_time; i < OUT_time; i += m_chapterLen * 45000) m_marks.push_back(PlayListMark(-1, i));
        }
    }
    writer.putBits(16, (unsigned)m_marks.size());
    for (auto& i : m_marks)
    {
        writer.putBits(8, 0);  // reserved_for_future_use
        writer.putBits(8, 1);  // mark_type 0x01 = Chapter search
        if (i.m_playItemID >= 0)
            writer.putBits(16, i.m_playItemID);  // play item ID
        else
            writer.putBits(16, calcPlayItemID(streamInfo, i.m_markTime * 2));  // play item ID
        writer.putBits(32, i.m_markTime);
        writer.putBits(16, 0xffff);  // entry_ES_PID always 0xffff for mark_type 1
        writer.putBits(32, 0);       // duration always 0 for mark_type 1
    }
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::composeSTN_table(BitStreamWriter& writer, size_t PlayItem_id, bool isSSEx)
{
    uint16_t* lengthPos = (uint16_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(16, 0);  // length
    int beforeCount = writer.getBitsCount() / 8;
    writer.putBit(0);       // Fixed_offset_during_PopUp_flag for ext mode
    writer.putBits(15, 0);  // reserved_for_future_use

    number_of_primary_video_stream_entries = 0;
    number_of_secondary_video_stream_entries = 0;
    number_of_primary_audio_stream_entries = 0;
    number_of_secondary_audio_stream_entries = 0;
    number_of_PG_textST_stream_entries = 0;
    number_of_DolbyVision_video_stream_entries = 0;

    std::vector<MPLSStreamInfo>& streamInfo = isSSEx ? m_streamInfoMVC : m_streamInfo;

    for (auto& i : streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (isVideoStreamType(stream_coding_type))
        {
            if (i.isSecondary)
                number_of_secondary_video_stream_entries++;
            else if (i.HDR == 4)
                number_of_DolbyVision_video_stream_entries++;
            else
                number_of_primary_video_stream_entries++;
        }
        else if (isAudioStreamType(stream_coding_type))
        {
            if (!i.isSecondary)
                number_of_primary_audio_stream_entries++;
            else
                number_of_secondary_audio_stream_entries++;
        }
        else if (stream_coding_type == StreamType::SUB_PGS)
            number_of_PG_textST_stream_entries++;
        else if (stream_coding_type == StreamType::AUDIO_EAC3_ATSC)
        {
            LTRACE(LT_ERROR, 2, "Pure E-AC3 is not supported by AVCHD/Blu-ray. Aborting...");
            THROW(ERR_COMMON, "");
        }
        else
        {
            LTRACE(LT_ERROR, 2,
                   "Unsupported media type " << (int)stream_coding_type << " for AVCHD/Blu-ray muxing. Aborting...");
            THROW(ERR_COMMON, "");
        }
    }

    if (!isSSEx)
    {
        writer.putBits(8, number_of_primary_video_stream_entries);  // test subpath
        writer.putBits(8, number_of_primary_audio_stream_entries);
        writer.putBits(8, number_of_PG_textST_stream_entries);
        writer.putBits(8, 0);  // int number_of_IG_stream_entries = reader.getBits(8);
        writer.putBits(8, number_of_secondary_audio_stream_entries);
        writer.putBits(8,
                       number_of_secondary_video_stream_entries);  // int number_of_secondary_video_stream_entries.
                                                                   // Now subpath used for second video stream
        writer.putBits(8, 0);                                      // number_of_PiP_PG_textST_stream_entries_plus
        writer.putBits(8, number_of_DolbyVision_video_stream_entries);
        writer.putBits(32, 0);  // reserved_for_future_use
    }

    // video
    for (auto& i : streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (isVideoStreamType(stream_coding_type) && !i.isSecondary && i.HDR != 4)
        {
            i.composeStreamEntry(writer, PlayItem_id);
            i.composeStreamAttributes(writer);
            if (stream_coding_type == StreamType::VIDEO_MVC)
            {
                writer.putBits(10, 0);  // reserved_for_future_use
                writer.putBits(6, FFMAX(1, i.number_of_offset_sequences));
            }
        }
    }

    // primary audio
    for (auto& i : streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (isAudioStreamType(stream_coding_type) && !i.isSecondary)
        {
            i.composeStreamEntry(writer, PlayItem_id);
            i.composeStreamAttributes(writer);
        }
    }

    // PG
    for (auto& i : m_streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (stream_coding_type == StreamType::SUB_PGS)
        {
            if (isSSEx)
            {
                i.composePGS_SS_StreamEntry(writer, PlayItem_id);
            }
            else
            {
                i.composeStreamEntry(writer, PlayItem_id);
                i.composeStreamAttributes(writer);
            }
        }
    }

    // secondary audio
    for (auto& i : streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (isAudioStreamType(stream_coding_type) && i.isSecondary)
        {
            i.composeStreamEntry(writer, PlayItem_id);
            i.composeStreamAttributes(writer);
            if (number_of_secondary_video_stream_entries == 0)
            {
                writer.putBits(8, number_of_primary_audio_stream_entries);  // number_of_primary_audio_ref_entries
                                                                            // allowed to join with this stream
                writer.putBits(8, 0);                                       // word align
                uint8_t primaryAudioNum = 0;
                for (auto& j : streamInfo)
                {
                    StreamType stream_coding_type = j.stream_coding_type;
                    if (isAudioStreamType(stream_coding_type) && !j.isSecondary)
                        writer.putBits(8, primaryAudioNum++);
                }
                if (number_of_primary_audio_stream_entries % 2 == 1)
                    writer.putBits(8, 0);  // word align
            }
            else
            {
                writer.putBits(16, 0);
            }
        }
    }

    // secondary video
    int secondaryVNum = 0;
    for (auto& i : streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (i.isSecondary && isVideoStreamType(stream_coding_type))
        {
            i.type = 3;
            i.composeStreamEntry(writer, PlayItem_id, secondaryVNum);
            i.composeStreamAttributes(writer);

            int useSecondaryAudio = number_of_secondary_audio_stream_entries > secondaryVNum ? 1 : 0;
            writer.putBits(8, useSecondaryAudio);
            writer.putBits(8, 0);  // reserved_for_word_align
            if (useSecondaryAudio)
            {
                writer.putBits(8, secondaryVNum);
                writer.putBits(8, 0);  // word align
            }

            writer.putBits(8, 0);  // number_of_PiP_PG_textST_ref_entries
            writer.putBits(8, 0);  // reserved_for_word_align
            secondaryVNum++;
        }
    }

    // DV video
    for (auto& i : streamInfo)
    {
        StreamType stream_coding_type = i.stream_coding_type;
        if (isVideoStreamType(stream_coding_type) && i.HDR == 4)
        {
            i.type = 4;
            i.composeStreamEntry(writer, PlayItem_id);
            i.composeStreamAttributes(writer);
        }
    }

    if (isSSEx && writer.getBitsCount() % 32 != 0)
        writer.putBits(16, 0);

    *lengthPos = my_htons(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::STN_table(BitStreamReader& reader, int PlayItem_id)
{
    reader.skipBits(32);  // length, reserved_for_future_use
    number_of_primary_video_stream_entries = reader.getBits(8);
    number_of_primary_audio_stream_entries = reader.getBits(8);
    number_of_PG_textST_stream_entries = reader.getBits(8);
    number_of_IG_stream_entries = reader.getBits(8);
    number_of_secondary_audio_stream_entries = reader.getBits(8);
    number_of_secondary_video_stream_entries = reader.getBits(8);
    number_of_PiP_PG_textST_stream_entries_plus = reader.getBits(8);
    number_of_DolbyVision_video_stream_entries = reader.getBits(8);
    reader.skipBits(32);  // reserved_for_future_use

    for (int primary_video_stream_id = 0; primary_video_stream_id < number_of_primary_video_stream_entries;
         primary_video_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        if (PlayItem_id == 0)
            m_streamInfo.push_back(streamInfo);
    }
    for (int primary_audio_stream_id = 0; primary_audio_stream_id < number_of_primary_audio_stream_entries;
         primary_audio_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        if (PlayItem_id == 0)
            m_streamInfo.push_back(streamInfo);
    }

    for (int PG_textST_stream_id = 0;
         PG_textST_stream_id < number_of_PG_textST_stream_entries + number_of_PiP_PG_textST_stream_entries_plus;
         PG_textST_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        if (PlayItem_id == 0)
            m_streamInfo.push_back(streamInfo);
    }

    for (int IG_stream_id = 0; IG_stream_id < number_of_IG_stream_entries; IG_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        if (PlayItem_id == 0)
            m_streamInfo.push_back(streamInfo);
    }

    for (int secondary_audio_stream_id = 0; secondary_audio_stream_id < number_of_secondary_audio_stream_entries;
         secondary_audio_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.isSecondary = true;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        if (PlayItem_id == 0)
            m_streamInfo.push_back(streamInfo);

        int number_of_primary_audio_ref_entries = reader.getBits(8);
        reader.skipBits(8);  // reserved_for_word_align

        for (int i = 0; i < number_of_primary_audio_ref_entries; i++)
            reader.skipBits(8);  // primary_audio_stream_id_ref

        if (number_of_primary_audio_ref_entries & 1)
            reader.skipBits(8);  // reserved_for_word_align
    }

    for (int secondary_video_stream_id = 0; secondary_video_stream_id < number_of_secondary_video_stream_entries;
         secondary_video_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.isSecondary = true;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        if (PlayItem_id == 0)
            m_streamInfo.push_back(streamInfo);

        int number_of_secondary_audio_ref_entries = reader.getBits(8);
        reader.skipBits(8);  // reserved_for_word_align

        for (int i = 0; i < number_of_secondary_audio_ref_entries; i++)
            reader.skipBits(8);  // secondary_audio_stream_id_ref

        if (number_of_secondary_audio_ref_entries & 1)
            reader.skipBits(8);  // reserved_for_word_align

        int number_of_PiP_PG_textST_ref_entries = reader.getBits(8);
        reader.skipBits(8);  // reserved_for_word_align

        for (int i = 0; i < number_of_PiP_PG_textST_ref_entries; i++)
            reader.skipBits(8);  // PiP_PG_textST_stream_id_ref

        if (number_of_PiP_PG_textST_ref_entries & 1)
            reader.skipBits(8);  // reserved_for_word_align
    }

    for (int DV_video_stream_id = 0; DV_video_stream_id < number_of_DolbyVision_video_stream_entries;
         DV_video_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        if (PlayItem_id == 0)
            m_streamInfo.push_back(streamInfo);
    }
}

// ------------- M2TSStreamInfo -----------------------

void M2TSStreamInfo::blurayStreamParams(double fps, bool interlaced, int width, int height, int ar, int* video_format,
                                        int* frame_rate_index, int* aspect_ratio_index)
{
    *video_format = 0;
    *frame_rate_index = 0;
    *aspect_ratio_index = 3;  // 16:9; 2 = 4:3

    bool isNtsc = width <= 854 && height <= 480 && (fabs(25 - fps) >= 0.5 && fabs(50 - fps) >= 0.5);
    bool isPal = width <= 1024 && height <= 576 && (fabs(25 - fps) < 0.5 || fabs(50 - fps) < 0.5);
    if (isNtsc)
        *video_format = interlaced ? 1 : 3;
    else if (isPal)
        *video_format = interlaced ? 2 : 7;
    else if (width >= 2600)
        *video_format = 8;
    else if (width >= 1300)
        *video_format = interlaced ? 4 : 6;  // as 1920x1080
    else
        *video_format = 5;  // as 1280x720

    if (width < 1080 && isV3())
        LTRACE(LT_WARN, 2, "Warning: video height < 1080 is not standard for V3 Blu-ray.");
    if (interlaced && isV3())
        LTRACE(LT_WARN, 2, "Warning: interlaced video is not standard for V3 Blu-ray.");

    if (fabs(fps - 23.976) < 1e-4)
        *frame_rate_index = 1;
    else if (fabs(fps - 24.0) < 1e-4)
        *frame_rate_index = 2;
    else if (fabs(fps - 25.0) < 1e-4)
        *frame_rate_index = 3;
    else if (fabs(fps - 29.97) < 1e-4)
        *frame_rate_index = 4;
    else if (fabs(fps - 50.0) < 1e-4)
        *frame_rate_index = 6;
    else if (fabs(fps - 59.94) < 1e-4)
        *frame_rate_index = 7;

    if (ar == (int)VideoAspectRatio::AR_3_4 || ar == (int)VideoAspectRatio::AR_VGA)
        *aspect_ratio_index = 2;  // 4x3
}

M2TSStreamInfo::M2TSStreamInfo(const PMTStreamInfo& pmtStreamInfo)
{
    streamPID = pmtStreamInfo.m_pid;
    stream_coding_type = pmtStreamInfo.m_streamType;
    m_index = pmtStreamInfo.m_index;
    isSecondary = pmtStreamInfo.isSecondary;
    memset(&language_code, 0, 4);
    memcpy(language_code, pmtStreamInfo.m_lang, 3);

    character_code = 0;
    video_format = 0;
    frame_rate_index = 0;
    number_of_offset_sequences = 0;
    audio_presentation_type = 0;
    sampling_frequency_index = 0;
    aspect_ratio_index = 3;  // 16:9; 2 = 4:3

    if (pmtStreamInfo.m_codecReader != 0)
    {
        MPEGStreamReader* vStream = dynamic_cast<MPEGStreamReader*>(pmtStreamInfo.m_codecReader);
        if (vStream)
        {
            width = vStream->getStreamWidth();
            height = vStream->getStreamHeight();
            HDR = vStream->getStreamHDR();
            VideoAspectRatio ar = vStream->getStreamAR();
            blurayStreamParams(vStream->getFPS(), vStream->getInterlaced(), width, height, (int)ar, &video_format,
                               &frame_rate_index, &aspect_ratio_index);
            if (ar == VideoAspectRatio::AR_3_4)
                width = height * 4 / 3;
            else if (ar == VideoAspectRatio::AR_16_9)
                width = height * 16 / 9;
            else if (ar == VideoAspectRatio::AR_221_100)
                width = height * 221 / 100;
        }
        H264StreamReader* h264Stream = dynamic_cast<H264StreamReader*>(pmtStreamInfo.m_codecReader);
        if (h264Stream)
            number_of_offset_sequences = h264Stream->getOffsetSeqCnt();

        SimplePacketizerReader* aStream = dynamic_cast<SimplePacketizerReader*>(pmtStreamInfo.m_codecReader);
        if (aStream)
        {
            audio_presentation_type = aStream->getChannels();
            if (audio_presentation_type == 2)
                audio_presentation_type = 3;
            else if (audio_presentation_type > 3)
                audio_presentation_type = 6;
            int freq = aStream->getFreq();
            // todo: add index 12 and 14. 12: 48Khz core, 192Khz mpl, 14: 48Khz core, 96Khz mlp
            switch (freq)
            {
            case 48000:
                if (aStream->getAltFreq() == 96000)
                    sampling_frequency_index = 14;
                else if (aStream->getAltFreq() == 192000)
                    sampling_frequency_index = 12;
                else
                    sampling_frequency_index = 1;
                break;
            case 96000:
                if (aStream->getAltFreq() == 192000)
                    sampling_frequency_index = 12;
                else
                    sampling_frequency_index = 4;
                break;
            case 192000:
                sampling_frequency_index = 5;
                break;
            }
        }
    }
}

M2TSStreamInfo::M2TSStreamInfo(const M2TSStreamInfo& other)
{
    streamPID = other.streamPID;
    stream_coding_type = other.stream_coding_type;
    video_format = other.video_format;
    frame_rate_index = other.frame_rate_index;
    number_of_offset_sequences = other.number_of_offset_sequences;
    width = other.width;
    height = other.height;
    HDR = other.HDR;
    aspect_ratio_index = other.aspect_ratio_index;
    audio_presentation_type = other.audio_presentation_type;
    sampling_frequency_index = other.sampling_frequency_index;
    character_code = other.character_code;
    memcpy(language_code, other.language_code, sizeof(language_code));
    isSecondary = other.isSecondary;
    m_index = other.m_index;
}

MPLSStreamInfo::MPLSStreamInfo(const MPLSStreamInfo& other) : M2TSStreamInfo(other)
{
    type = other.type;
    character_code = other.character_code;
    offsetId = other.offsetId;
    pipParams = other.pipParams;
    isSSPG = other.isSSPG;
    SS_PG_offset_sequence_id = other.SS_PG_offset_sequence_id;
    leftEye = 0;
    rightEye = 0;
    if (other.leftEye)
        leftEye = new MPLSStreamInfo(*other.leftEye);
    if (other.rightEye)
        rightEye = new MPLSStreamInfo(*other.rightEye);
}

// -------------- MPLSStreamInfo -----------------------

MPLSStreamInfo::MPLSStreamInfo()
    : M2TSStreamInfo(),
      type(0),
      character_code(0),
      offsetId(0xff),
      isSSPG(false),
      SS_PG_offset_sequence_id(0xff),
      leftEye(0),
      rightEye(0)
{
}

MPLSStreamInfo::MPLSStreamInfo(const PMTStreamInfo& pmtStreamInfo)
    : M2TSStreamInfo(pmtStreamInfo),
      type(1),
      character_code(0),
      offsetId(0xff),
      isSSPG(false),
      SS_PG_offset_sequence_id(0xff),
      leftEye(0),
      rightEye(0)
{
    pipParams = pmtStreamInfo.m_codecReader->getPipParams();
}

MPLSStreamInfo::~MPLSStreamInfo()
{
    delete leftEye;
    delete rightEye;
}

void MPLSStreamInfo::parseStreamEntry(BitStreamReader& reader)
{
    reader.skipBits(8);  // length
    type = reader.getBits(8);
    if (type == 1)
    {
        streamPID = reader.getBits(16);
        reader.skipBits(32);
        reader.skipBits(16);
    }
    else if (type == 2)
    {
        reader.skipBits(16);  // ref_to_SubPath_id, ref_to_subClip_entry_id
        streamPID = reader.getBits(32);
        reader.skipBits(16);
    }
    else if (type == 3)
    {
        reader.getBits(8);  // ref_to_SubPath_id
        streamPID = reader.getBits(8);
        reader.skipBits(32);
        reader.skipBits(16);
    }
    else if (type == 4)
    {
        reader.skipBits(8);
        streamPID = reader.getBits(16);
        reader.skipBits(32);
        reader.skipBits(8);
    }
}

void MPLSStreamInfo::composePGS_SS_StreamEntry(BitStreamWriter& writer, size_t entryNum)
{
    writer.putBits(8, offsetId);
    writer.putBits(4, 0);   // reserved
    writer.putBit(0);       // dialog region offset valid
    writer.putBit(isSSPG);  // is SS PG
    writer.putBit(0);       // top AS PG
    writer.putBit(0);       // bottom AS PG
    if (isSSPG)
    {
        leftEye->composeStreamEntry(writer, entryNum);
        rightEye->composeStreamEntry(writer, entryNum);
        writer.putBits(8, 0);  // reserved
        writer.putBits(8, SS_PG_offset_sequence_id);
    }
}

void MPLSStreamInfo::composeStreamEntry(BitStreamWriter& writer, size_t entryNum, int subPathID)
{
    uint8_t* lengthPos = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);  // length
    int initPos = writer.getBitsCount() / 8;
    writer.putBits(8, type);
    if (type == 1)
    {
        writer.putBits(16, streamPID);
        writer.putBits(32, 0);
        writer.putBits(16, 0);
    }
    else if (type == 2)
    {
        writer.putBits(8, 0);  // ref_to_SubPath_id
        writer.putBits(8, 0);  // entryNum - ref_to_subClip_entry_id
        writer.putBits(16, streamPID);
        writer.putBits(32, 0);
    }
    else if (type == 3)
    {
        writer.putBits(8, subPathID);  // ref_to_SubPath_id. constant subPathID == 0
        writer.putBits(16, streamPID);
        writer.putBits(32, 0);
        writer.putBits(8, 0);
    }
    else if (type == 4)
    {
        writer.putBits(8, 0);
        writer.putBits(16, streamPID);
        writer.putBits(32, 0);
        writer.putBits(8, 0);
    }
    else
        THROW(ERR_COMMON, "Unsupported media type for AVCHD/Blu-ray muxing");
    *lengthPos = writer.getBitsCount() / 8 - initPos;
}

void MPLSStreamInfo::parseStreamAttributes(BitStreamReader& reader)
{
    reader.skipBits(8);  // length
    stream_coding_type = (StreamType)reader.getBits(8);
    if (isVideoStreamType(stream_coding_type))
    {
        video_format = reader.getBits(4);
        frame_rate_index = reader.getBits(4);
        reader.skipBits(24);  // reserved_for_future_use
    }
    else if (isAudioStreamType(stream_coding_type))
    {
        audio_presentation_type = reader.getBits(4);
        reader.skipBits(4);  // sampling_frequency_index
        CLPIStreamInfo::readString(language_code, reader, 3);
    }
    else if (stream_coding_type == StreamType::SUB_PGS)
    {
        // Presentation Graphics stream
        CLPIStreamInfo::readString(language_code, reader, 3);
        reader.skipBits(8);  // reserved_for_future_use
    }
    else if (stream_coding_type == StreamType::SUB_IGS)
    {
        // Interactive Graphics stream
        CLPIStreamInfo::readString(language_code, reader, 3);
        reader.skipBits(8);  // reserved_for_future_use
    }
    else if (stream_coding_type == StreamType::SUB_TGS)
    {
        // Text subtitle stream
        character_code = reader.getBits(8);
        CLPIStreamInfo::readString(language_code, reader, 3);
    }
}

void MPLSStreamInfo::composeStreamAttributes(BitStreamWriter& writer)
{
    uint8_t* lengthPos = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);  // length
    int initPos = writer.getBitsCount() / 8;

    writer.putBits(8, (int)stream_coding_type);
    if (isVideoStreamType(stream_coding_type))
    {
        writer.putBits(4, video_format);
        writer.putBits(4, frame_rate_index);
        if (HDR & 18)
            writer.putBits(8, 0x12);  // HDR10 or HDR10plus
        else if (HDR == 4)
            writer.putBits(8, 0x22);  // DV
        else
            writer.putBits(8, 0);
        if (HDR == 16)
            writer.putBits(8, 0x40);  // HDR10plus
        else
            writer.putBits(8, 0);
        writer.putBits(8, 0);  // reserved_for_future_use
    }
    else if (isAudioStreamType(stream_coding_type))
    {
        writer.putBits(4, audio_presentation_type);
        writer.putBits(4, sampling_frequency_index);
        CLPIStreamInfo::writeString(language_code, writer, 3);
    }
    else if (stream_coding_type == StreamType::SUB_PGS)
    {
        // Presentation Graphics stream
        CLPIStreamInfo::writeString(language_code, writer, 3);
        writer.putBits(8, 0);  // reserved_for_future_use
    }
    else
        THROW(ERR_COMMON, "Unsupported media type for AVCHD/Blu-ray muxing");
    *lengthPos = writer.getBitsCount() / 8 - initPos;
}
