
#ifndef _WIN32
#endif
#include "tsPacket.h"

#include <fs/file.h>
#include <fs/systemlog.h>

#include <string>

#include "bitStream.h"
#include "crc32.h"
#include "h264StreamReader.h"
#include "hevc.h"
#include "math.h"
#include "mpegStreamReader.h"
#include "simplePacketizerReader.h"
#include "tsMuxer.h"
#include "vodCoreException.h"

using namespace std;

enum SubPathType
{
    SUBPATH_PIP = 7
};

bool isVideoStreamType(int stream_coding_type)
{
    return stream_coding_type == STREAM_TYPE_VIDEO_MPEG2 || stream_coding_type == STREAM_TYPE_VIDEO_H264 ||
           stream_coding_type == STREAM_TYPE_VIDEO_VC1 || stream_coding_type == STREAM_TYPE_VIDEO_MVC ||
           stream_coding_type == STREAM_TYPE_VIDEO_H265;
}

bool isAudioStreamType(int stream_coding_type)
{
    return stream_coding_type == 0x80 || stream_coding_type == 0x81 || stream_coding_type == 0x82 ||
           stream_coding_type == 0x83 || stream_coding_type == 0x84 || stream_coding_type == 0x85 ||
           stream_coding_type == 0x86 || stream_coding_type == 0xA1 || stream_coding_type == 0xA2 ||

           stream_coding_type == STREAM_TYPE_AUDIO_AAC || stream_coding_type == STREAM_TYPE_AUDIO_AAC_RAW ||
           stream_coding_type == STREAM_TYPE_AUDIO_MPEG1 || stream_coding_type == STREAM_TYPE_AUDIO_MPEG2;
}

// ------------ PS PACK -------------------
bool PS_stream_pack::deserialize(uint8_t* buffer, int buf_size)
{
    m_pts = 0;
    BitStreamReader bitReader;
    bitReader.setBuffer(buffer, buffer + buf_size);
    if (bitReader.getBits(2) != 1)
        return false;  // 0b01 required
    m_pts = (bitReader.getBits(3) << 30);
    if (bitReader.getBit() != 1)
        return false;
    m_pts += (bitReader.getBits(15) << 15);
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
    BitStreamReader bitReader;
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
        int crcBit = bitReader.getBitsCount() + (section_length - 4) * 8;

        transport_stream_id = bitReader.getBits(16);
        bitReader.skipBits(2);  // reserved
        bitReader.getBits(5);   // int version_number
        bitReader.getBits(1);   // int current_next_indicator

        bitReader.getBits(8);  // int section_number
        bitReader.getBits(8);  // int last_section_number

        pmtPids.clear();
        // while(get_bits_count(&bitContext) < crcBit)
        while (bitReader.getBitsCount() < crcBit)
        {
            int program_number = bitReader.getBits(16);
            bitReader.skipBits(3);  // reserved
            int program_pid = bitReader.getBits(13);
            if (program_number != 0)  // not a network pid
                pmtPids.insert(std::make_pair(program_pid, program_number));
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
    BitStreamWriter bitWriter;

    // init_bitWriter.putBits(buffer, buf_size*8);
    bitWriter.setBuffer(buffer, buffer + buf_size);

    bitWriter.putBits(8, 0);
    bitWriter.putBits(2, 2);  // indicator
    bitWriter.putBits(2, 3);  // reserved

    unsigned section_length = 9 + pmtPids.size() * 4;
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
    // uint32_t crc = av_crc(tmpAvCrc, 0xffffffff, buffer, bitWriter.getBitsCount()/8);
    bitWriter.flushBits();
    uint32_t crc = calculateCRC32(buffer, bitWriter.getBitsCount() / 8);
    // bitWriter.putBits(32, my_htonl(crc));
    uint32_t* crcPtr = (uint32_t*)(buffer + bitWriter.getBitsCount() / 8);
    *crcPtr = my_htonl(crc);
    // flush_put_bits(&bitContext);
    // return put_bits_count(&bitContext)/8 + 1;
    return bitWriter.getBitsCount() / 8 + 5;
}

// ------------- PMT ------------------------

TS_program_map_section::TS_program_map_section()
    : video_pid(0), audio_pid(0), sub_pid(0), pcr_pid(0), program_number(0), casID(0), casPID(0)
{
    // av_crc_init(tmpAvCrc, 0, 32, 0x04C11DB7L, sizeof(AVCRC)*257);

    video_type = -1;
    audio_type = -1;
}

bool TS_program_map_section::isFullBuff(uint8_t* buffer, int buf_size)
{
    uint8_t pointerField = *buffer;
    uint8_t* bufEnd = buffer + buf_size;
    BitStreamReader bitReader;
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
        uint8_t tag = *curPos;
        uint8_t len = curPos[1];
        curPos += 2;
        if (tag == TS_CAS_DESCRIPTOR_TAG && len >= 4)
        {
            casID = (curPos[0] << 8) + curPos[1];
            casPID = ((curPos[2] & 0x0f) << 8) + curPos[3];
        }
        else if (tag == TS_COPY_CONTROL_DESCRIPTOR_TAG && len >= 2)
        {
            uint16_t casSystemId = (curPos[0] << 8) + curPos[1];
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
    BitStreamReader bitReader;
    try
    {
        bitReader.setBuffer(buffer + 1 + pointerField, buffer + buf_size);

        // bitReader.skipBits(8); // skip zero byte?

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
        bitReader.skipBits(2);                         // reserved
        bitReader.getBits(5);                          // int version_number
        int nextIndicator = bitReader.getBits(1);      // int current_next_indicator
        int sectionNumber = bitReader.getBits(8);      // int section_number
        int lastSectionNumber = bitReader.getBits(8);  // int last_section_number
        bitReader.skipBits(3);                         // reserved
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
            int stream_type = *curPos++;
            int elementary_pid = get16(&curPos) & 0x1fff;
            switch (stream_type)
            {
            case STREAM_TYPE_VIDEO_MPEG1:
            case STREAM_TYPE_VIDEO_MPEG2:
            case STREAM_TYPE_VIDEO_MPEG4:
            case STREAM_TYPE_VIDEO_H264:
            case STREAM_TYPE_VIDEO_H265:
            case STREAM_TYPE_VIDEO_MVC:
            case STREAM_TYPE_VIDEO_VC1:
                video_pid = elementary_pid;
                video_type = stream_type;
                break;

            case STREAM_TYPE_AUDIO_MPEG1:
            case STREAM_TYPE_AUDIO_MPEG2:
            case STREAM_TYPE_AUDIO_AAC:
            case STREAM_TYPE_AUDIO_AC3:
            case STREAM_TYPE_AUDIO_EAC3:
            case STREAM_TYPE_AUDIO_EAC3_ATSC:
            case STREAM_TYPE_AUDIO_DTS:
                audio_pid = elementary_pid;
                audio_type = stream_type;
                break;
            case STREAM_TYPE_SUBTITLE_DVB:
                sub_pid = elementary_pid;
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
            pidList.insert(std::make_pair(elementary_pid, pmtStreamInfo));
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
        uint8_t tag = *curPos;
        uint8_t len = curPos[1];
        curPos += 2;
        uint8_t* descrBuf = curPos;
        if (tag == TS_REGISTRATION_DESCRIPTOR_TAG)
        {
            uint32_t format_identifier = *((uint32_t*)descrBuf);
            descrBuf += 4;
        }
        else if (tag == TS_LANG_DESCRIPTOR_TAG)
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
    BitStreamWriter bitWriter;
    bitWriter.setBuffer(buffer, buffer + max_buf_size);
    bitWriter.putBits(8, 2);  // table id

    uint16_t* LengthPos1 = (uint16_t*)(bitWriter.getBuffer() + bitWriter.getBitsCount() / 8);
    bitWriter.putBits(2, 2);   // indicator
    bitWriter.putBits(2, 3);   // reserved
    bitWriter.putBits(12, 0);  // skip lengthField
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
        bitWriter.putBits(8, 0x05);
        bitWriter.putBits(8, 0x04);
        bitWriter.putBits(32, 0x48444d56);

        // put DTCP descriptor
        bitWriter.putBits(8, 0x88);
        bitWriter.putBits(8, 0x04);
        bitWriter.putBits(32, 0x0ffffcfc);
    }

    if (casPID)
    {
        // put CAS descriptor
        bitWriter.putBits(8, TS_CAS_DESCRIPTOR_TAG);
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
        // bitWriter.putBits( 8, 0x04);
        bitWriter.putBits(8, audio_type);
        bitWriter.putBits(3, 7);  // reserved
        bitWriter.putBits(13, audio_pid);
        bitWriter.putBits(4, 15);  // reserved
        bitWriter.putBits(12, 0);  // es_info_len
    }

    if (sub_pid)
    {
        bitWriter.putBits(8, STREAM_TYPE_SUBTITLE_DVB);
        bitWriter.putBits(3, 7);  // reserved
        bitWriter.putBits(13, sub_pid);
        bitWriter.putBits(4, 15);  // reserved
        bitWriter.putBits(12, 0);  // es_info_len
    }

    for (PIDListMap::const_iterator itr = pidList.begin(); itr != pidList.end(); ++itr)
    {
        if (itr->second.m_streamType == 0x90 && !hdmvDescriptors)
            LTRACE(LT_WARN, 2, "Warning: PGS might not work without HDMV descriptors.");

        bitWriter.putBits(8, itr->second.m_streamType);
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
            bitWriter.putBits(8, TS_LANG_DESCRIPTOR_TAG);                             // lang descriptor ID
            bitWriter.putBits(8, 4);                                                  // lang descriptor len
            for (int k = 0; k < 3; k++) bitWriter.putBits(8, itr->second.m_lang[k]);  // lang code[i]
            bitWriter.putBits(8, 0);
        }
        *esInfoLen = my_htons(0xf000 + bitWriter.getBitsCount() / 8 - beforeCount);
    }
    *LengthPos1 = my_htons(0xb000 + bitWriter.getBitsCount() / 8 - beforeCount1 + 4);
    bitWriter.flushBits();

    // uint32_t crc = av_crc(tmpAvCrc, 0xffffffff, buffer, bitWriter.getBitsCount()/8);
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
    /*
    writeString(country_code, writer, 2);
    writeString(copyright_holder, writer, 3);
    writeString(recording_year, writer, 2);
    writeString(recording_number, writer, 5);*/
    writeString("\x30\x30", writer, 2);
    writeString("\x30\x30\x30", writer, 3);
    writeString("\x30\x30", writer, 2);
    writeString("\x30\x30\x30\x30\x30", writer, 5);
}

void CLPIStreamInfo::parseStreamCodingInfo(BitStreamReader& reader)
{
    int length = reader.getBits(8);
    stream_coding_type = reader.getBits(8);

    if (isVideoStreamType(stream_coding_type))
    {
        video_format = reader.getBits(4);
        frame_rate_index = reader.getBits(4);
        aspect_ratio_index = reader.getBits(4);
        reader.skipBits(2);  // reserved_for_future_use 2 bslbf
        bool cc_flag = reader.getBit();
        reader.skipBits(17);  // reserved_for_future_use 17 bslbf
        ISRC(reader);
        reader.skipBits(32);  // reserved_for_future_use 32 bslbf
    }
    else if (isAudioStreamType(stream_coding_type))
    {
        audio_presentation_type = reader.getBits(4);
        sampling_frequency_index = reader.getBits(4);
        readString(language_code, reader, 3);
        ISRC(reader);
        reader.skipBits(32);
    }
    else if (stream_coding_type == 0x90)
    {
        // Presentation Graphics stream
        readString(language_code, reader, 3);
        reader.skipBits(8);  // reserved_for_future_use 8 bslbf
        ISRC(reader);
        reader.skipBits(32);  // reserved_for_future_use 32 bslbf
    }
    else if (stream_coding_type == 0x91)
    {
        // Interactive Graphics stream
        readString(language_code, reader, 3);
        reader.skipBits(8);  // reserved_for_future_use 8 bslbf
        ISRC(reader);
        reader.skipBits(32);  // reserved_for_future_use 32 bslbf
    }
    else if (stream_coding_type == 0x92)
    {
        // Text subtitle stream
        character_code = reader.getBits(8);
        readString(language_code, reader, 3);
        ISRC(reader);
        reader.skipBits(32);  // reserved_for_future_use 32 bslbf
    }
}

void CLPIStreamInfo::composeStreamCodingInfo(BitStreamWriter& writer) const
{
    uint8_t* lengthPos = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, stream_coding_type);

    if (isVideoStreamType(stream_coding_type))
    {
        writer.putBits(4, video_format);
        writer.putBits(4, frame_rate_index);
        writer.putBits(4, aspect_ratio_index);
        writer.putBits(2, 0);  // reserved_for_future_use 2 bslbf
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
        writer.putBits(32, 0);  // reserved_for_future_use 32 bslbf
    }
    else if (isAudioStreamType(stream_coding_type))
    {
        writer.putBits(4, audio_presentation_type);
        writer.putBits(4, sampling_frequency_index);
        writeString(language_code, writer, 3);
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use 32 bslbf
    }
    else if (stream_coding_type == 0x90)
    {
        // Presentation Graphics stream
        writeString(language_code, writer, 3);
        writer.putBits(8, 0);  // reserved_for_future_use 8 bslbf
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use 32 bslbf
    }
    else if (stream_coding_type == 0x91)
    {
        // Interactive Graphics stream
        writeString(language_code, writer, 3);
        writer.putBits(8, 0);  // reserved_for_future_use 8 bslbf
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use 32 bslbf
    }
    else if (stream_coding_type == 0x92)
    {
        // Text subtitle stream
        writer.putBits(8, character_code);
        writeString(language_code, writer, 3);
        composeISRC(writer);
        writer.putBits(32, 0);  // reserved_for_future_use 32 bslbf
    }
    *lengthPos = writer.getBitsCount() / 8 - beforeCount;
}

void CLPIParser::parseProgramInfo(uint8_t* buffer, uint8_t* end, std::vector<CLPIProgramInfo>& programInfoMap,
                                  std::map<int, CLPIStreamInfo>& streamInfoMap)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    uint32_t length = reader.getBits(32);
    reader.skipBits(8);  // reserved_for_word_align
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
            streamInfoMap.insert(std::make_pair(pid, streamInfo));
        }
    }
}

void CLPIParser::composeProgramInfo(BitStreamWriter& writer, bool isSsExt)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField

    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);  // reserved
    writer.putBits(8, 1);  // number_of_program_sequences
    // for (int i=0; i < number_of_program_sequences; i++)
    {
        // m_programInfo.push_back(CLPIProgramInfo());
        writer.putBits(32, 0);                // m_programInfo[i].SPN_program_sequence_start
        writer.putBits(16, DEFAULT_PMT_PID);  // m_programInfo[i].program_map_PID =

        int streams = 0;
        for (std::map<int, CLPIStreamInfo>::const_iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
        {
            const CLPIStreamInfo& si = itr->second;
            bool streamOK = isSsExt && si.stream_coding_type == 0x20 || !isSsExt && si.stream_coding_type != 0x20;
            if (streamOK)
                streams++;
        }

        writer.putBits(8, streams);  // m_programInfo[i].number_of_streams_in_ps
        writer.putBits(8, 0);        // reserved_for_future_use 8 bslbf
        // for (int i=0; i < m_streamInfo.size(); i++)
        for (std::map<int, CLPIStreamInfo>::const_iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
        {
            const CLPIStreamInfo& si = itr->second;
            bool streamOK = isSsExt && si.stream_coding_type == 0x20 || !isSsExt && si.stream_coding_type != 0x20;
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
    uint16_t length = reader.getBits(16);
    uint8_t Validity_flags = reader.getBits(8);                // 1000 0000b is tipical value
    CLPIStreamInfo::readString(format_identifier, reader, 4);  // HDMV
    // Network_information 8*9 bslbf zero
    for (int i = 0; i < 9; i++) reader.skipBits(8);
    // Stream_format_name 8*16 bslbf zero
    for (int i = 0; i < 16; i++) reader.skipBits(8);
}

void CLPIParser::composeTS_type_info_block(BitStreamWriter& writer)
{
    // uint16_t length = reader.getBits(16);
    uint16_t* lengthPos = (uint16_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(16, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0x80);  // Validity_flags
    CLPIStreamInfo::writeString("HDMV", writer, 4);
    // Network_information 8*9 bslbf zero
    for (int i = 0; i < 9; i++) writer.putBits(8, 0);
    // Stream_format_name 8*16 bslbf zero
    for (int i = 0; i < 16; i++) writer.putBits(8, 0);
    *lengthPos = my_ntohs(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::parseClipInfo(BitStreamReader& reader)
{
    uint32_t length = reader.getBits(32);
    reader.skipBits(16);                            // reserved_for_future_use 16 bslbf
    clip_stream_type = reader.getBits(8);           // 1 - AV stream
    application_type = reader.getBits(8);           // 1 - Main TS for a main-path of Movie
    reader.skipBits(31);                            // reserved_for_future_use 31 bslbf
    is_ATC_delta = reader.getBit();                 // 1 bslbf
    TS_recording_rate = reader.getBits(32);         // kbps in bytes/sec
    number_of_source_packets = reader.getBits(32);  // number of TS packets?
    for (int i = 0; i < 32; i++) reader.skipBits(32);
    TS_type_info_block(reader);
    /*
    if (is_ATC_delta==1b) {
            reserved_for_future_use 8 bslbf
            number_of_ATC_delta_entries 8 uimsbf
            for (i=0; i<number_of_ATC_delta_entries; i++) {
                    ATC_delta[i] 32 uimsbf
                    following_Clip_Information_file_name[i] 8*5 bslbf
                    Clip_codec_identifier 8*4 bslbf
                    reserved_for_future_use 8 bslbf
            }
    }
    if (application_type==6){
            reserved_for_future_use 8 bslbf
            number_of_font_files 8 uimsbf
            for (font_id=0; font_id<number_of_font_files; font_id++) {
                    font_file_name[font_id] 8*5 bslbf
                    reserved_for_future_use 8 bslbf
            }
    }
    */
}

void CLPIParser::composeClipInfo(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(16, 0);                               // reserved_for_future_use 16 bslbf
    writer.putBits(8, clip_stream_type);                 // 1 - AV stream
    writer.putBits(8, application_type);                 // 1 - Main TS for a main-path of Movie
    writer.putBits(31, 0);                               // reserved_for_future_use 31 bslbf
    writer.putBit(is_ATC_delta);                         // 1 bslbf
    writer.putBits(32, TS_recording_rate);               // kbps in bytes/sec
    writer.putBits(32, number_of_source_packets);        // number of TS packets?
    for (int i = 0; i < 32; i++) writer.putBits(32, 0);  // reserved
    composeTS_type_info_block(writer);
    if (is_ATC_delta)
        THROW(ERR_COMMON, "CLPI is_ATC_delta is not implemented now.");
    if (application_type == 6)
        THROW(ERR_COMMON, "CLPI application_type==6 is not implemented now.");
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::parseSequenceInfo(uint8_t* buffer, uint8_t* end)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    uint32_t length = reader.getBits(32);
    reader.skipBits(8);                                   // reserved_for_word_align 8 bslbf
    uint8_t number_of_ATC_sequences = reader.getBits(8);  // 1 is tipical value
    for (int atc_id = 0; atc_id < number_of_ATC_sequences; atc_id++)
    {
        uint32_t SPN_ATC_start = reader.getBits(32);  // 0 is tipical value
        uint8_t number_of_STC_sequences = reader.getBits(8);
        int offset_STC_id = reader.getBits(8);
        for (int stc_id = offset_STC_id; stc_id < number_of_STC_sequences + offset_STC_id; stc_id++)
        {
            int PCR_PID = reader.getBits(16);
            uint32_t SPN_STC_start = reader.getBits(32);
            presentation_start_time = reader.getBits(32);
            presentation_end_time = reader.getBits(32);
        }
    }
}

void CLPIParser::composeSequenceInfo(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);  // reserved_for_word_align 8 bslbf
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
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    uint32_t length = reader.getBits(32);
    if (length != 0)
    {
        // reserved_for_word_align 12 bslbf
        reader.skipBits(12);
        int CPI_type = reader.getBits(4);  // 1 is tipical value
        CPI_type = CPI_type;
        // EP_map(reader);
    }
}

void CLPIParser::EP_map(BitStreamReader& reader) {}

void CLPIParser::composeCPI(BitStreamWriter& writer, bool isCPIExt)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField

    if (isDependStream && !isCPIExt || !isDependStream && isCPIExt)
        return;  // CPI_SS for MVC depend stream only and vice versa: standard CPI for standard video stream

    int beforeCount = writer.getBitsCount() / 8;
    // if (length != 0)
    {
        // reserved_for_word_align 12 bslbf
        writer.putBits(12, 0);
        writer.putBits(4, 1);  // CPI_type
        composeEP_map(writer, isCPIExt);
    }

    if (isCPIExt && writer.getBitsCount() % 32 != 0)
        writer.putBits(16, 0);

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::composeEP_map(BitStreamWriter& writer, bool isSSExt)
{
    uint32_t beforeCount = writer.getBitsCount() / 8;
    std::vector<CLPIStreamInfo> processStream;
    int EP_stream_type = 1;  //[k] 4 bslbf
    for (std::map<int, CLPIStreamInfo>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
    {
        int coding_type = itr->second.stream_coding_type;
        if (isSSExt)
        {
            if (coding_type == 0x20)
                processStream.push_back(itr->second);
        }
        else
        {
            if (coding_type != 0x20 && isVideoStreamType(coding_type))
                processStream.push_back(itr->second);
        }
    }
    if (processStream.size() == 0)
        for (std::map<int, CLPIStreamInfo>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
        {
            int coding_type = itr->second.stream_coding_type;
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
    writer.putBits(8, 0);                     // reserved_for_word_align 8 bslbf
    writer.putBits(8, processStream.size());  // number_of_stream_PID_entries 8 uimsbf
    std::vector<uint32_t*> epStartAddrPos;

    for (int i = 0; i < processStream.size(); i++)
    {
        writer.putBits(16, processStream[i].streamPID);  // stream_PID[k] 16 bslbf
        writer.putBits(10, 0);                           // reserved_for_word_align 10 bslbf
        writer.putBits(4, EP_stream_type);
        std::vector<BluRayCoarseInfo> coarseInfo = buildCoarseInfo(processStream[i]);
        writer.putBits(16, coarseInfo.size());  // number_of_EP_coarse_entries[k] 16 uimsbf
        if (processStream[i].m_index.size() > 0)
            writer.putBits(18, processStream[i].m_index[m_clpiNum].size());  // number_of_EP_fine_entries[k] 18 uimsbf
        else
            writer.putBits(18, 0);
        epStartAddrPos.push_back((uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8));
        writer.putBits(32, 0);  // EP_map_for_one_stream_PID_start_address[k] 32 uimsbf
    }
    while (writer.getBitsCount() % 16 != 0) writer.putBits(8, 0);  // padding_word 16 bslbf

    for (int i = 0; i < processStream.size(); i++)
    {
        *epStartAddrPos[i] = my_htonl(writer.getBitsCount() / 8 - beforeCount);
        composeEP_map_for_one_stream_PID(writer, processStream[i]);
        while (writer.getBitsCount() % 16 != 0) writer.putBits(8, 0);  // padding_word 16 bslbf
    }
}

std::vector<BluRayCoarseInfo> CLPIParser::buildCoarseInfo(M2TSStreamInfo& streamInfo)
{
    std::vector<BluRayCoarseInfo> rez;
    if (streamInfo.m_index.size() == 0)
        return rez;
    uint32_t cnt = 0;
    uint32_t lastPktCnt = 0;
    uint32_t lastCoarsePts = 0;
    PMTIndex& curIndex = streamInfo.m_index[m_clpiNum];
    for (PMTIndex::const_iterator itr = curIndex.begin(); itr != curIndex.end(); ++itr)
    {
        const PMTIndexData& indexData = itr->second;
        uint32_t newCoarsePts = itr->first >> 19;
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
    writer.putBits(32, 0);  // EP_fine_table_start_address 32 uimsbf
    std::vector<BluRayCoarseInfo> coarseInfo = buildCoarseInfo(streamInfo);
    for (int i = 0; i < coarseInfo.size(); i++)
    {
        writer.putBits(18, coarseInfo[i].m_fineRefID);  // ref_to_EP_fine_id[i] 18 uimsbf
        writer.putBits(14, coarseInfo[i].m_coarsePts);  // PTS_EP_coarse[i] 14 uimsbf
        writer.putBits(32, coarseInfo[i].m_pktCnt);     // SPN_EP_coarse[i] 32 uimsbf
    }
    while (writer.getBitsCount() % 16 != 0) writer.putBits(8, 0);  // padding_word 16 bslbf
    *epFineStartAddr = my_htonl(writer.getBitsCount() / 8 - beforePos);
    if (streamInfo.m_index.size() > 0)
    {
        const PMTIndex& curIndex = streamInfo.m_index[m_clpiNum];
        for (PMTIndex::const_iterator itr = curIndex.begin(); itr != curIndex.end(); ++itr)
        {
            const PMTIndexData& indexData = itr->second;
            writer.putBit(0);  // is_angle_change_point[EP_fine_id] 1 bslbf
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
            writer.putBits(3, endCode);                            // I_end_position_offset[EP_fine_id] 3 bslbf
            writer.putBits(11, (itr->first >> 9) % 2048);          // PTS_EP_fine[EP_fine_id] 11 uimsbf
            writer.putBits(17, indexData.m_pktCnt % (65536 * 2));  // SPN_EP_fine[EP_fine_id] 17 uimsbf
        }
    }
}

void CLPIParser::parseClipMark(uint8_t* buffer, uint8_t* end)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    uint32_t length = reader.getBits(32);
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
    if (!file.read(buffer, fileSize))
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
    BitStreamReader reader;
    reader.setBuffer(buffer, buffer + dataLength);
    uint32_t length = reader.getBits(32);

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
    writer.putBits(32, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(16, 0);  // reserved
    writer.putBits(16, interleaveInfo.size());

    uint32_t sum = 0;
    for (int i = 0; i < interleaveInfo.size(); ++i)
    {
        sum += interleaveInfo[i];
        writer.putBits(32, sum);
    }

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void CLPIParser::composeExtentInfo(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(32, 0);  // skip lengthField

    if (interleaveInfo.empty())
        return;

    writer.putBits(32, 0);  // skip data block start address
    writer.putBits(24, 0);  // reserved for world align. not used

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

    while (writer.getBitsCount() % 32) writer.putBits(16, 0);

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
    BitStreamReader reader;
    reader.setBuffer(buffer, end);
    uint32_t length = reader.getBits(32);
    if (length == 0)
        return;

    int data_block_start_address = reader.getBits(32);
    reader.skipBits(24);
    int entries = reader.getBits(8);
    for (int i = 0; i < entries; ++i)
    {
        uint32_t dataID = reader.getBits(32);
        uint32_t dataAddress = reader.getBits(32);
        uint32_t dataLength = reader.getBits(32);

        if (dataAddress + dataLength > end - buffer)
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

void CLPIParser::parse(uint8_t* buffer, int len)
{
    BitStreamReader reader;
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
        for (int i = 0; i < 3; i++) reader.skipBits(32);  // reserved_for_future_use 96 bslbf
        parseClipInfo(reader);
        // for (int i=0; i<N1; i++) padding_word 16 bslbf
        parseSequenceInfo(buffer + sequenceInfo_start_address, buffer + len);
        // for (i=0; i<N2; i++) { padding_word 16 bslbf
        parseProgramInfo(buffer + programInfo_start_address, buffer + len, m_programInfo, m_streamInfo);
        // for (i=0; i<N3; i++) padding_word 16 bslbf
        parseCPI(buffer + CPI_start_address, buffer + len);
        // for (i=0; i<N4; i++) padding_word 16 bslbf
        parseClipMark(buffer + clipMark_start_address, buffer + len);
        // for (i=0; i<N5; i++) padding_word 16 bslbf
        if (extensionData_start_address)
            parseExtensionData(buffer + extensionData_start_address, buffer + len);
        // for (i=0; i<N6; i++) padding_word 16 bslbf
    }
    catch (BitStreamException&)
    {
        THROW(ERR_COMMON, "Can't parse clip info file: unexpected end of data");
    }
}

int CLPIParser::compose(uint8_t* buffer, int bufferSize)
{
    BitStreamWriter writer;
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
    for (int i = 0; i < 3; i++) writer.putBits(32, 0);  // reserved_for_future_use 32 bslbf

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
    : m_chapterLen(0),
      m_playItem(0),
      number_of_SubPaths(0),
      m_m2tsOffset(0),
      isDependStreamExist(false),
      mvc_base_view_r(false)
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
    if (!file.read(buffer, fileSize))
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

void MPLSParser::parse(uint8_t* buffer, int len)
{
    BitStreamReader reader;
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
        for (int i = 0; i < 5; i++) reader.skipBits(32);  // reserved_for_future_use 160 bslbf
        AppInfoPlayList(reader);
        parsePlayList(buffer + playList_start_address, len - playList_start_address);
        /*
        for (int i=0; i<N1; i++) {
                padding_word 16 bslbf
        }
        PlayList();
        for (int i=0; i<N2; i++) {
                padding_word 16 bslbf
        }
        */
        parsePlayListMark(buffer + playListMark_start_address, len - playListMark_start_address);

        if (extensionData_start_address)
        {
            parseExtensionData(buffer + extensionData_start_address, buffer + len);
        }

        /*
        for (i=0; i<N3; i++) {
                padding_word 16 bslbf
        }
        ExtensionData();
        for (i=0; i<N4; i++) {
                padding_word 16 bslbf
        }
        */
    }
    catch (BitStreamException&)
    {
        THROW(ERR_COMMON, "Can't parse media playlist file: unexpected end of data");
    }
}

void MPLSParser::SubPath_extension(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);   // reserved
    writer.putBits(8, 8);   // SubPath_type = 8
    writer.putBits(15, 0);  // reserved
    writer.putBit(0);       // is_repeat_SubPath
    writer.putBits(8, 0);   // reserved

    std::vector<PMTIndex> pmtIndexList = getMVCDependStream().m_index;
    writer.putBits(8, pmtIndexList.size());  // number_of_SubPlayItems
    for (int i = 0; i < pmtIndexList.size(); i++) composeSubPlayItem(writer, i, 0, pmtIndexList);

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

int MPLSParser::composeSubPathEntryExtension(uint8_t* buffer, int bufferSize)
{
    BitStreamWriter writer;
    writer.setBuffer(buffer, buffer + bufferSize);
    try
    {
        uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
        writer.putBits(32, 0);  // skip lengthField
        int beforeCount = writer.getBitsCount() / 8;

        // composeSubPath(writer, 0, dependStreamInfo.m_index, 5); // MVC dep stream type
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
    BitStreamWriter writer;
    writer.setBuffer(buffer, buffer + bufferSize);
    try
    {
        MPLSStreamInfo streamInfoMVC = getMVCDependStream();
        for (int PlayItem_id = 0; PlayItem_id < streamInfoMVC.m_index.size(); PlayItem_id++)
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
    BitStreamWriter writer;
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
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].stream_coding_type;
        if (isVideoStreamType(stream_coding_type))
        {
            if (m_streamInfo[i].isSecondary)
            {
                number_of_SubPaths++;
                subPath_type = 7;  // PIP not fully implemented yet
            }
            else if (m_streamInfo[i].HDR & 4)
            {
                number_of_SubPaths++;
                subPath_type = 10;
            }
        }
    }

    BitStreamWriter writer;
    writer.setBuffer(buffer, buffer + bufferSize);

    std::string type_indicator = "MPLS";
    std::string version_number;
    if (dt == DT_BLURAY)
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
    for (int i = 0; i < 5; i++) writer.putBits(32, 0);  // reserved_for_future_use 160 bslbf
    composeAppInfoPlayList(writer);
    while (writer.getBitsCount() % 16 != 0) writer.putBits(8, 0);
    *playList_bit_pos = my_htonl(writer.getBitsCount() / 8);
    composePlayList(writer);

    while (writer.getBitsCount() % 16 != 0) writer.putBits(8, 0);
    *playListMark_bit_pos = my_htonl(writer.getBitsCount() / 8);
    composePlayListMark(writer);

    while (writer.getBitsCount() % 16 != 0) writer.putBits(8, 0);

    if (number_of_SubPaths > 0 || isDependStreamExist || isV3())
    {
        *extDataStartAddr = my_htonl(writer.getBitsCount() / 8);
        uint8_t buffer[1024 * 4];
        MPLSStreamInfo& mainStreamInfo = getMainStream();
        vector<ExtDataBlockInfo> blockVector;

        // for (int i = 0; i < number_of_SubPaths; ++i)
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
        while (writer.getBitsCount() % 16 != 0) writer.putBits(8, 0);
    }

    writer.flushBits();
    return writer.getBitsCount() / 8;
}

void MPLSParser::AppInfoPlayList(BitStreamReader& reader)
{
    uint32_t length = reader.getBits(32);
    reader.skipBits(8);                          // reserved_for_future_use 8 bslbf
    PlayList_playback_type = reader.getBits(8);  // 8 bslbf
    if (PlayList_playback_type == 2 || PlayList_playback_type == 3)
    {                                         // 1 == Sequential playback of PlayItems
        playback_count = reader.getBits(16);  // 16 uimsbf
    }
    else
    {
        reader.skipBits(16);  // reserved_for_future_use 16 bslbf
    }
    UO_mask_table(reader);
    bool PlayList_random_access_flag = reader.getBit();  // 1 bslbf
    bool audio_mix_app_flag = reader.getBits(1);
    bool lossless_may_bypass_mixer_flag = reader.getBit();  // 1 bslbf
    mvc_base_view_r = reader.getBit();
    reader.skipBits(12);  // reserved_for_future_use 13 bslbf
}

void MPLSParser::composeAppInfoPlayList(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);                       // reserved_for_future_use 8 bslbf
    writer.putBits(8, PlayList_playback_type);  // 8 bslbf
    if (PlayList_playback_type == 2 || PlayList_playback_type == 3)
    {                                        // 1 == Sequential playback of PlayItems
        writer.putBits(16, playback_count);  // 16 uimsbf
    }
    else
    {
        writer.putBits(16, 0);  // reserved_for_future_use 16 bslbf
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
    writer.putBits(12, 0);  // reserved_for_future_use 13 bslbf
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::UO_mask_table(BitStreamReader& reader)
{
    reader.skipBit();                           // reserved_for_future_use // reserved for menu call mask 1 bslbf
    reader.skipBit();                           // reserved_for_future_use // reserved for title search mask 1 bslbf
    chapter_search_mask = reader.getBit();      // 1 bslbf
    time_search_mask = reader.getBit();         // 1 bslbf
    skip_to_next_point_mask = reader.getBit();  // 1 bslbf
    skip_back_to_previous_point_mask = reader.getBit();  // 1 bslbf
    reader.skipBit();                      // reserved_for_future_use // reserved for play FirstPlay mask 1 bslbf
    stop_mask = reader.getBit();           // 1 bslbf
    pause_on_mask = reader.getBit();       // 1 bslbf
    reader.skipBit();                      // reserved_for_future_use // reserved for pause off mask 1 bslbf
    still_off_mask = reader.getBit();      // 1 bslbf
    forward_play_mask = reader.getBit();   // 1 bslbf
    backward_play_mask = reader.getBit();  // 1 bslbf
    resume_mask = reader.getBit();         // 1 bslbf
    move_up_selected_button_mask = reader.getBit();               // 1 bslbf
    move_down_selected_button_mask = reader.getBit();             // 1 bslbf
    move_left_selected_button_mask = reader.getBit();             // 1 bslbf
    move_right_selected_button_mask = reader.getBit();            // 1 bslbf
    select_button_mask = reader.getBit();                         // 1 bslbf
    activate_button_mask = reader.getBit();                       // 1 bslbf
    select_button_and_activate_mask = reader.getBit();            // 1 bslbf
    primary_audio_stream_number_change_mask = reader.getBit();    // 1 bslbf
    reader.skipBit();                                             // reserved_for_future_use 1 bslbf
    angle_number_change_mask = reader.getBit();                   // 1 bslbf
    popup_on_mask = reader.getBit();                              // 1 bslbf
    popup_off_mask = reader.getBit();                             // 1 bslbf
    PG_textST_enable_disable_mask = reader.getBit();              // 1 bslbf
    PG_textST_stream_number_change_mask = reader.getBit();        // 1 bslbf
    secondary_video_enable_disable_mask = reader.getBit();        // 1 bslbf
    secondary_video_stream_number_change_mask = reader.getBit();  // 1 bslbf
    secondary_audio_enable_disable_mask = reader.getBit();        // 1 bslbf
    secondary_audio_stream_number_change_mask = reader.getBit();  //
    reader.skipBit();                                             // reserved_for_future_use 1 bslbf
    PiP_PG_textST_stream_number_change_mask = reader.getBit();    // 1 bslbf
    reader.skipBits(30);                                          // reserved_for_future_use 30 bslbf
}

void MPLSParser::parsePlayList(uint8_t* buffer, int len)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, buffer + len);
    uint32_t length = reader.getBits(32);
    reader.skipBits(16);                           // reserved_for_future_use 16 bslbf
    int number_of_PlayItems = reader.getBits(16);  // 16 uimsbf
    number_of_SubPaths = reader.getBits(16);       // 16 uimsbf
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
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int coding_type = m_streamInfo[i].stream_coding_type;
        if (isVideoStreamType(coding_type))
            return m_streamInfo[i];
    }
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int coding_type = m_streamInfo[i].stream_coding_type;
        if (isAudioStreamType(coding_type))
        {
            return m_streamInfo[i];
        }
    }
    THROW(ERR_COMMON, "Can't find stream index. One audio or video stream is needed.");
}

int MPLSParser::pgIndexToFullIndex(int value)
{
    int cnt = 0;
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        if (m_streamInfo[i].stream_coding_type == 0x90)
        {
            if (cnt++ == value)
                return i;
        }
    }
    return -1;
}

MPLSStreamInfo MPLSParser::getStreamByPID(int pid) const
{
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        if (m_streamInfo[i].streamPID == pid)
            return m_streamInfo[i];
    }
    return MPLSStreamInfo();
}

std::vector<MPLSStreamInfo> MPLSParser::getPgStreams() const
{
    std::vector<MPLSStreamInfo> pgStreams;
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int coding_type = m_streamInfo[i].stream_coding_type;
        if (coding_type == 0x90)
            pgStreams.push_back(m_streamInfo[i]);
    }
    return pgStreams;
}

MPLSStreamInfo& MPLSParser::getMVCDependStream()
{
    for (int i = 0; i < m_streamInfoMVC.size(); i++)
    {
        int coding_type = m_streamInfoMVC[i].stream_coding_type;
        if (coding_type == 0x20)
            return m_streamInfoMVC[i];
    }
    THROW(ERR_COMMON, "Can't find stream index. One audio or video stream is needed.");
}

void MPLSParser::composePlayList(BitStreamWriter& writer)
{
    // uint32_t length = reader.getBits(32);
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;
    writer.putBits(16, 0);  // reserved_for_future_use 16 bslbf
    MPLSStreamInfo& mainStreamInfo = getMainStream();
    writer.putBits(16, mainStreamInfo.m_index.size());  // 16 uimsbf number_of_PlayItems
    writer.putBits(16, number_of_SubPaths);             // number_of_SubPaths
    // connection_condition = 1;
    for (int PlayItem_id = 0; PlayItem_id < mainStreamInfo.m_index.size(); PlayItem_id++)
    {
        composePlayItem(writer, PlayItem_id, mainStreamInfo.m_index);
        // connection_condition = 6;
    }

    MPLSStreamInfo& dependStreamInfo = mainStreamInfo;

    for (int SubPath_id = 0; SubPath_id < number_of_SubPaths * dependStreamInfo.m_index.size(); SubPath_id++)
    {
        composeSubPath(writer, SubPath_id, dependStreamInfo.m_index, subPath_type);  // pip
    }

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::composeSubPath(BitStreamWriter& writer, int subPathNum, std::vector<PMTIndex>& pmtIndexList, int type)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    writer.putBits(8, 0);  // reserved_for_future_use
    writer.putBits(
        8, type);  // SubPath_type = 7 (In-mux and Synchronous type of Picture-in-Picture), 5 - MVC depend stream
    writer.putBits(15, 0);  // reserved_for_future_use
    writer.putBits(1, 0);   // is_repeat_SubPath = false
    writer.putBits(8, 0);   // reserved_for_future_use

    /*
    vector<StreamInfo*> secondary;
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
            int stream_coding_type = m_streamInfo[i].stream_coding_type;
            if (m_streamInfo[i].isSecondary && (stream_coding_type==0x02 || stream_coding_type==0x1B ||
    stream_coding_type==0xEA)) secondary.push_back(m_streamInfo[i]);
    }
    */
    writer.putBits(8, pmtIndexList.size());  // number_of_SubPlayItems
    for (int i = 0; i < pmtIndexList.size(); i++)
    {
        composeSubPlayItem(writer, i, subPathNum, pmtIndexList);
    }
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::composeSubPlayItem(BitStreamWriter& writer, int playItemNum, int subPathNum,
                                    std::vector<PMTIndex>& pmtIndexList)
{
    uint16_t* lengthPos = (uint16_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(16, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;

    int fileNum = playItemNum;
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
    writer.putBits(27, 0);                    // reserved_for_future_use
    writer.putBits(4, connection_condition);  // 4 bslbf
    writer.putBit(0);                         // is_multi_Clip_entries
    writer.putBits(8, ref_to_STC_id);         // 8 uimsbf

    if (playItemNum == 0)
        writer.putBits(32, IN_time);  // 32 uimsbf
    else if (pmtIndexList[playItemNum - 1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum].begin()->first / 2);
    else
        writer.putBits(32, IN_time);  // 32 uimsbf

    if (playItemNum == pmtIndexList.size() - 1)
        writer.putBits(32, OUT_time);  // 32 uimsbf
    else if (pmtIndexList[playItemNum + 1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum + 1].begin()->first / 2);  // 32 uimsbf
    else
        writer.putBits(32, OUT_time);  // 32 uimsbf

    writer.putBits(16, playItemNum);  // sync_PlayItem_id. reference to play_item id.
    // sync_start_PTS_of_PlayItem
    if (playItemNum == 0)
        writer.putBits(32, IN_time);  // 32 uimsbf
    else if (pmtIndexList[playItemNum - 1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum].begin()->first / 2);
    else
        writer.putBits(32, IN_time);  // 32 uimsbf

    // writer.flushBits();
    *lengthPos = my_htons(writer.getBitsCount() / 8 - beforeCount);
}

int MPLSParser::composePip_metadata(uint8_t* buffer, int bufferSize, std::vector<PMTIndex>& pmtIndexList)
{
    // The ID1 value and the ID2 value of the ExtensionData() shall be set to 0x0001 and 0x0001
    BitStreamWriter writer;
    writer.setBuffer(buffer, buffer + bufferSize);
    uint32_t* lengthPos = (uint32_t*)buffer;
    writer.putBits(32, 0);  // int length = reader.getBits(8); //8 uimsbf

    vector<MPLSStreamInfo> pipStreams;
    int mainVSize = 0;
    int mainHSize = 0;
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].stream_coding_type;
        if (isVideoStreamType(stream_coding_type))
        {
            if (m_streamInfo[i].isSecondary)
            {
                pipStreams.push_back(m_streamInfo[i]);
            }
            else
            {
                mainHSize = m_streamInfo[i].width;
                mainVSize = m_streamInfo[i].height;
            }
        }
    }

    writer.putBits(16, pipStreams.size() * pmtIndexList.size());
    vector<uint32_t*> blockDataAddressPos;
    for (int i = 0; i < pmtIndexList.size(); ++i)
    {
        for (int k = 0; k < pipStreams.size(); k++)
        {
            PIPParams pipParams = pipStreams[k].pipParams;
            // metadata_block_header[k]() {
            writer.putBits(16, i);  // ref_to_PlayItem_id
            writer.putBits(8, k);   // ref_to_secondary_video_stream_id
            writer.putBits(8, 0);   // reserved_for_future_use
            writer.putBits(
                4, pipParams.lumma >= 0 ? 1 : 0);  // pip_timeline_type == 1. Synchronous type of Picture-in-Picture
            writer.putBit(1);                      // is_luma_key = 0
            writer.putBit(1);                      // trick_playing_flag. keep PIP windows when tricking
            writer.putBits(10, 0);                 // reserved_for_word_align
            writer.putBits(8, 0);                  // reserved_for_future_use 8 bslbf
            if (pipParams.lumma >= 0)
            {                                        // is_luma_key==1b
                writer.putBits(8, pipParams.lumma);  // transparent Y pixels
            }
            else
            {
                writer.putBits(8, 0);  // reserved_for_future_use 16 bslbf
            }
            writer.putBits(16, 0);  // reserved_for_future_use 16 bslbf
            blockDataAddressPos.push_back((uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8));
            writer.putBits(32, 0);  // metadata_block_data_start_address
        }
    }
    while (writer.getBitsCount() % 16 != 0) writer.putBit(0);
    for (int i = 0; i < pmtIndexList.size(); ++i)
    {
        for (int k = 0; k < pipStreams.size(); k++)
        {
            PIPParams pipParams = pipStreams[k].pipParams;

            *(blockDataAddressPos[i * pipStreams.size() + k]) = my_htonl(writer.getBitsCount() / 8);

            writer.putBits(16, 1);  // number_of_pip_metadata_entries
            {
                if (i == 0)
                    writer.putBits(32, IN_time);  // 32 uimsbf
                else if (pmtIndexList[i - 1].size() > 0)
                    writer.putBits(32, pmtIndexList[i].begin()->first / 2);
                else
                    writer.putBits(32, IN_time);  // 32 uimsbf

                int hPos = 0;
                int vPos = 0;

                if (!pipParams.isFullScreen())
                {
                    hPos = pipParams.hOffset;
                    vPos = pipParams.vOffset;

                    int pipWidth = pipStreams[k].width * pipParams.getScaleCoeff();
                    int pipHeight = pipStreams[k].height * pipParams.getScaleCoeff();

                    if (pipParams.corner == PIPParams::TopRight || pipParams.corner == PIPParams::BottomRight)
                        hPos = mainHSize - pipWidth - pipParams.hOffset;
                    if (pipParams.corner == PIPParams::BottomRight || pipParams.corner == PIPParams::BottomLeft)
                        vPos = mainVSize - pipHeight - pipParams.vOffset;
                }

                writer.putBits(12, hPos);
                writer.putBits(12, vPos);

                writer.putBits(4, pipParams.scaleIndex);  // pip_scale[i] 4 uimsbf. 1 == no_scale
                writer.putBits(4, 0);                     // reserved_for_future_use 4 bslbf
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
        BitStreamReader reader;
        reader.setBuffer(data, data + dataLength);
        int len = reader.getBits(16);
        bool fixedOffsetDuringPopup = reader.getBit();
        reader.skipBits(15);  // reserved

        int firstPgIndex = number_of_primary_video_stream_entries + number_of_primary_audio_stream_entries;

        for (int i = 0; i < number_of_primary_video_stream_entries; ++i)
        {
            MPLSStreamInfo streamInfo;
            // m_streamInfoMVC.push_back(streamInfo);

            streamInfo.parseStreamEntry(reader);

            int attrSSLen = reader.getBits(8);
            reader.skipBits(8);  // coding type 0x20
            int format = reader.getBits(4);
            int frameRate = reader.getBits(4);
            reader.skipBits(24);  // reserved

            reader.skipBits(10);  // reserved
            int number_of_offset_sequences = reader.getBits(6);
        }

        for (int i = 0; i < number_of_PG_textST_stream_entries; ++i)
        {
            int PG_textST_offset_sequence_id = reader.getBits(8);
            int idx = pgIndexToFullIndex(i);
            if (idx != -1)
                m_streamInfo[idx].offsetId = PG_textST_offset_sequence_id;

            reader.skipBits(4);  // reserved
            bool dialog_region_offset_valid = reader.getBit();
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
                reader.skipBits(8);                   // reserved
                int top_AS_offset_sequence_id = reader.getBits(8);
            }
            if (isBottomAS)
            {
                MPLSStreamInfo streamInfo;
                streamInfo.parseStreamEntry(reader);  // left eye
                reader.skipBits(8);                   // reserved
                int bottom_AS_offset_sequence_id = reader.getBits(8);
            }
        }
    }
    catch (...)
    {
    }
}

void MPLSParser::parseSubPathEntryExtension(uint8_t* data, int dataLen)
{
    BitStreamReader reader;
    reader.setBuffer(data, data + dataLen);
    try
    {
        uint32_t length = reader.getBits(32);
        uint16_t size = reader.getBits(16);
        // for (int i = 0; i < size; ++i)
        if (size > 0)
        {
            // subpath extension
            uint32_t len2 = reader.getBits(32);
            reader.skipBits(8);  // reserved field
            int type = reader.getBits(8);
            if (type == 8 || type == 9)
            {
                reader.skipBits(24);
                int subPlayItems = reader.getBits(8);
                for (int i = 0; i < subPlayItems; ++i)
                {
                    int len3 = reader.getBits(16);
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
                            char clip_Information_file_name[6];
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
        BitStreamReader reader;
        reader.setBuffer(data, dataEnd);
        uint32_t length = reader.getBits(32);
        if (length == 0)
            return;

        int data_block_start_address = reader.getBits(32);
        reader.skipBits(24);
        int entries = reader.getBits(8);
        for (int i = 0; i < entries; ++i)
        {
            uint32_t dataID = reader.getBits(32);
            uint32_t dataAddress = reader.getBits(32);
            uint32_t dataLength = reader.getBits(32);

            if (dataAddress + dataLength > dataEnd - data)
            {
                LTRACE(LT_WARN, 2, "Invalid playlist extension entry skipped.");
                continue;
            }

            if (dataAddress + dataLength > dataEnd - data)
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
    writer.putBits(32, 0);  // int length = reader.getBits(8); //8 uimsbf
    int initPos = writer.getBitsCount() / 8;
    if (extDataBlockInfo.size() > 0)
    {
        writer.putBits(32, 0);  // data_block_start_address
        writer.putBits(24, 0);  // reserved_for_word_align
        writer.putBits(8, extDataBlockInfo.size());
        for (int i = 0; i < extDataBlockInfo.size(); i++)
        {
            writer.putBits(16, extDataBlockInfo[i].id1);
            writer.putBits(16, extDataBlockInfo[i].id2);
            extDataStartAddrPos[i] = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
            writer.putBits(32, 0);                                // ext_data_start_address
            writer.putBits(32, extDataBlockInfo[i].data.size());  // ext_data_length
        }
        while ((writer.getBitsCount() / 8 - initPos) % 4 != 0) writer.putBits(16, 0);
        *(lengthPos + 1) = my_htonl(writer.getBitsCount() / 8 - initPos + 4);  // data_block_start_address
        for (int i = 0; i < extDataBlockInfo.size(); i++)
        {
            *(extDataStartAddrPos[i]) = my_htonl(writer.getBitsCount() / 8 - initPos + 4);
            for (int j = 0; j < extDataBlockInfo[i].data.size(); ++j) writer.putBits(8, extDataBlockInfo[i].data[j]);
        }
    }

    *lengthPos = my_htonl(writer.getBitsCount() / 8 - initPos);
}

void MPLSParser::parsePlayItem(BitStreamReader& reader, int PlayItem_id)
{
    MPLSPlayItem newItem;
    int length = reader.getBits(16);
    char clip_Information_file_name[6];
    char clip_codec_identifier[5];
    CLPIStreamInfo::readString(clip_Information_file_name, reader, 5);
    newItem.fileName = clip_Information_file_name;
    CLPIStreamInfo::readString(clip_codec_identifier, reader, 4);
    reader.skipBits(11);  // reserved_for_future_use 11 bslbf
    is_multi_angle = reader.getBit();
    newItem.connection_condition = reader.getBits(4);  // 4 bslbf
    ref_to_STC_id = reader.getBits(8);                 // 8 uimsbf

    newItem.IN_time = reader.getBits(32);   // 32 uimsbf
    newItem.OUT_time = reader.getBits(32);  // 32 uimsbf
    m_playItems.push_back(newItem);

    UO_mask_table(reader);
    PlayItem_random_access_flag = reader.getBit();  // 1 bslbf
    reader.skipBits(7);                             // reserved_for_future_use 7 bslbf
    uint8_t still_mode = reader.getBits(8);         // 8 bslbf
    if (still_mode == 0x01)
    {
        int still_time = reader.getBits(16);  // 16 uimsbf
    }
    else
    {
        reader.skipBits(16);  // reserved_for_future_use 16 bslbf
    }
    if (is_multi_angle == 1)
    {
        number_of_angles = reader.getBits(8);        // uimsbf
        reader.skipBits(6);                          // reserved_for_future_use 6 bslbf
        is_different_audios = reader.getBit();       // 1 bslbf
        is_seamless_angle_change = reader.getBit();  // 1 bslbf
        for (int angle_id = 1;                       // angles except angle_id=0
             angle_id < number_of_angles; angle_id++)
        {
            CLPIStreamInfo::readString(clip_Information_file_name, reader, 5);  // 8*5 bslbf
            CLPIStreamInfo::readString(clip_codec_identifier, reader, 4);       // 8*4 bslbf
            ref_to_STC_id = reader.getBits(8);                                  // 8 uimsbf
        }
    }
    STN_table(reader, PlayItem_id);
}

void MPLSParser::composePlayItem(BitStreamWriter& writer, int playItemNum, std::vector<PMTIndex>& pmtIndexList)
{
    uint16_t* lengthPos = (uint16_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(16, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;
    int fileNum = playItemNum;
    if (isDependStreamExist)
        fileNum *= 2;
    std::string clip_Information_file_name = strPadLeft(int32ToStr(fileNum + m_m2tsOffset), 5, '0');
    CLPIStreamInfo::writeString(clip_Information_file_name.c_str(), writer, 5);
    char clip_codec_identifier[] = "M2TS";
    CLPIStreamInfo::writeString(clip_codec_identifier, writer, 4);
    writer.putBits(11, 0);  // reserved_for_future_use 11 bslbf
    writer.putBit(0);       // is_multi_angle
    int connection_condition = playItemNum == 0 ? 1 : 6;
    writer.putBits(4, connection_condition);  // 4 bslbf
    writer.putBits(8, ref_to_STC_id);         // 8 uimsbf
    if (playItemNum == 0)
        writer.putBits(32, IN_time);  // 32 uimsbf
    else if (pmtIndexList[playItemNum - 1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum].begin()->first / 2);
    else
        writer.putBits(32, IN_time);  // 32 uimsbf

    if (playItemNum == pmtIndexList.size() - 1)
        writer.putBits(32, OUT_time);  // 32 uimsbf
    else if (pmtIndexList[playItemNum + 1].size() > 0)
        writer.putBits(32, pmtIndexList[playItemNum + 1].begin()->first / 2);  // 32 uimsbf
    else
        writer.putBits(32, OUT_time);  // 32 uimsbf

    writer.putBits(28, 0);               // UO_mask_table;
    writer.putBits(4, isV3() ? 15 : 0);  // UO_mask_table;
    writer.putBit(0);                    // reserved
    writer.putBit(isV3() ? 1 : 0);       // UO_mask_table: SecondaryPGStreamNumberChange
    writer.putBits(30, 0);               // UO_mask_table cont;

    writer.putBit(PlayItem_random_access_flag);  // 1 bslbf
    writer.putBits(7, 0);                        // reserved_for_future_use 7 bslbf
    writer.putBits(8, 0);                        // still_mode
    writer.putBits(16, 0);                       // reserved after stillMode != 0x01
    composeSTN_table(writer, playItemNum, false);
    *lengthPos = my_htons(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::parsePlayListMark(uint8_t* buffer, int len)
{
    BitStreamReader reader;
    reader.setBuffer(buffer, buffer + len);
    uint32_t length = reader.getBits(32);               //
    int number_of_PlayList_marks = reader.getBits(16);  // 16 uimsbf
    for (int PL_mark_id = 0; PL_mark_id < number_of_PlayList_marks; PL_mark_id++)
    {
        reader.skipBits(8);                 // reserved_for_future_use 8 bslbf
        int mark_type = reader.getBits(8);  // 8 bslbf
        int ref_to_PlayItem_id = reader.getBits(16);
        uint32_t mark_time_stamp = reader.getBits(32);  // 32 uimsbf
        int entry_ES_PID = reader.getBits(16);          // 16 uimsbf
        uint32_t duration = reader.getBits(32);         // 32 uimsbf
        if (mark_type == 1)                             // mark_type 0x01 = Chapter search
            m_marks.push_back(PlayListMark(ref_to_PlayItem_id, mark_time_stamp));
    }
}

int MPLSParser::calcPlayItemID(MPLSStreamInfo& streamInfo, uint32_t pts)
{
    for (int i = 0; i < streamInfo.m_index.size(); i++)
    {
        if (streamInfo.m_index[i].size() > 0)
        {
            if (streamInfo.m_index[i].begin()->first > pts)
                return FFMAX(i - 1, 0);
        }
    }
    return streamInfo.m_index.size() - 1;
}

void MPLSParser::composePlayListMark(BitStreamWriter& writer)
{
    uint32_t* lengthPos = (uint32_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(32, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;
    MPLSStreamInfo& streamInfo = MPLSParser::getMainStream();
    if (m_marks.size() == 0)
    {
        if (m_chapterLen == 0)
            m_marks.push_back(PlayListMark(-1, IN_time));
        else
        {
            for (int i = IN_time; i < OUT_time; i += m_chapterLen * 45000) m_marks.push_back(PlayListMark(-1, i));
        }
    }
    writer.putBits(16, m_marks.size());  // 16 uimsbf
    for (int i = 0; i < m_marks.size(); i++)
    {
        writer.putBits(8, 0);  // reserved_for_future_use 8 bslbf
        writer.putBits(8, 1);  // mark_type 0x01 = Chapter search
        if (m_marks[i].m_playItemID >= 0)
            writer.putBits(16, m_marks[i].m_playItemID);  // play item ID
        else
            writer.putBits(16, calcPlayItemID(streamInfo, m_marks[i].m_markTime * 2));  // play item ID
        writer.putBits(32, m_marks[i].m_markTime);                                      // 32 uimsbf
        writer.putBits(16, 0xffff);  // entry_ES_PID always 0xffff for mark_type 1
        writer.putBits(32, 0);       // duration always 0 for mark_type 1
    }
    *lengthPos = my_htonl(writer.getBitsCount() / 8 - beforeCount);
}

/*
void MPLSParser::composePlayListMark(BitStreamWriter& writer)
{
        uint32_t* lengthPos = (uint32_t*) (writer.getBuffer() + writer.getBitsCount()/8);
        writer.putBits(32,0); // skip lengthField
        int beforeCount = writer.getBitsCount()/8;

        writer.putBits(16, 1); // number_of_PlayList_marks
        //for(int PL_mark_id=0; PL_mark_id<number_of_PlayList_marks; PL_mark_id++)
        //{
                writer.putBits(8, 0 ); //reserved_for_future_use 8 bslbf
                writer.putBits(8, 1); // mark_type

                writer.putBits(16, 0); // ref_to_PlayItem_id
                writer.putBits(32, IN_time); // mark_time_stamp
                writer.putBits(16, 0xffff); // entry_ES_PID
                writer.putBits(32, 0); // duration

        //}
        *lengthPos = my_ntohl(writer.getBitsCount()/8 - beforeCount);
}
*/

void MPLSParser::composeSTN_table(BitStreamWriter& writer, int PlayItem_id, bool isSSEx)
{
    uint16_t* lengthPos = (uint16_t*)(writer.getBuffer() + writer.getBitsCount() / 8);
    writer.putBits(16, 0);  // skip lengthField
    int beforeCount = writer.getBitsCount() / 8;
    writer.putBit(0);       // Fixed_offset_during_PopUp_flag for ext mode
    writer.putBits(15, 0);  // reserved_for_future_use 16 bslbf

    number_of_primary_video_stream_entries = 0;
    number_of_secondary_video_stream_entries = 0;
    number_of_primary_audio_stream_entries = 0;
    number_of_secondary_audio_stream_entries = 0;
    number_of_PG_textST_stream_entries = 0;
    number_of_DolbyVision_video_stream_entries = 0;

    std::vector<MPLSStreamInfo>& streamInfo = isSSEx ? m_streamInfoMVC : m_streamInfo;

    for (int i = 0; i < streamInfo.size(); i++)
    {
        int stream_coding_type = streamInfo[i].stream_coding_type;
        if (isVideoStreamType(stream_coding_type))
        {
            if (streamInfo[i].isSecondary)
                number_of_secondary_video_stream_entries++;
            else if (streamInfo[i].HDR == 4)
                number_of_DolbyVision_video_stream_entries++;
            else
                number_of_primary_video_stream_entries++;
        }
        else if (isAudioStreamType(stream_coding_type))
        {
            if (!streamInfo[i].isSecondary)
                number_of_primary_audio_stream_entries++;
            else
                number_of_secondary_audio_stream_entries++;
        }
        else if (stream_coding_type == 0x90)
            number_of_PG_textST_stream_entries++;
        else
            THROW(ERR_COMMON, "Unsupported media type " << stream_coding_type << " for AVCHD/Blu-ray muxing");
    }

    if (!isSSEx)
    {
        writer.putBits(8, number_of_primary_video_stream_entries);  // 8 uimsbf // test subpath
        writer.putBits(8, number_of_primary_audio_stream_entries);
        writer.putBits(8, number_of_PG_textST_stream_entries);
        writer.putBits(8, 0);  // int number_of_IG_stream_entries = reader.getBits(8); //8 uimsbf
        writer.putBits(8, number_of_secondary_audio_stream_entries);
        writer.putBits(8, number_of_secondary_video_stream_entries);  // int number_of_secondary_video_stream_entries.
                                                                      // Now subpath used for second video stream
        writer.putBits(8, 0);                                         // number_of_PiP_PG_textST_stream_entries_plus
        writer.putBits(8, number_of_DolbyVision_video_stream_entries);
        writer.putBits(32, 0);  // reserved_for_future_use 32 bslbf
    }

    // if (number_of_SubPaths == 0)

    // video
    for (int i = 0; i < streamInfo.size(); i++)
    {
        int stream_coding_type = streamInfo[i].stream_coding_type;
        if (isVideoStreamType(stream_coding_type) && !streamInfo[i].isSecondary && streamInfo[i].HDR != 4)
        {
            streamInfo[i].composeStreamEntry(writer, PlayItem_id);
            streamInfo[i].composeStreamAttributes(writer);
            if (stream_coding_type == 0x20)  // MVC
            {
                writer.putBits(10, 0);  // reserved_for_future_use
                writer.putBits(6, FFMAX(1, streamInfo[i].number_of_offset_sequences));
            }
        }
    }

    // primary audio
    for (int i = 0; i < streamInfo.size(); i++)
    {
        int stream_coding_type = streamInfo[i].stream_coding_type;
        if (isAudioStreamType(stream_coding_type) && !streamInfo[i].isSecondary)
        {
            streamInfo[i].composeStreamEntry(writer, PlayItem_id);
            streamInfo[i].composeStreamAttributes(writer);
        }
    }

    // PG
    for (int i = 0; i < m_streamInfo.size(); i++)
    {
        int stream_coding_type = m_streamInfo[i].stream_coding_type;
        if (stream_coding_type == 0x90)
        {
            if (isSSEx)
            {
                m_streamInfo[i].composePGS_SS_StreamEntry(writer, PlayItem_id);
            }
            else
            {
                m_streamInfo[i].composeStreamEntry(writer, PlayItem_id);
                m_streamInfo[i].composeStreamAttributes(writer);
            }
        }
    }

    // secondary audio
    for (int i = 0; i < streamInfo.size(); i++)
    {
        int stream_coding_type = streamInfo[i].stream_coding_type;
        if (isAudioStreamType(stream_coding_type) && streamInfo[i].isSecondary)
        {
            streamInfo[i].composeStreamEntry(writer, PlayItem_id);
            streamInfo[i].composeStreamAttributes(writer);
            if (number_of_secondary_video_stream_entries == 0)
            {
                writer.putBits(8, number_of_primary_audio_stream_entries);  // number_of_primary_audio_ref_entries
                                                                            // allowed to join with this stream
                writer.putBits(8, 0);                                       // word align
                uint8_t primaryAudioNum = 0;
                for (int j = 0; j < streamInfo.size(); j++)
                {
                    int stream_coding_type = streamInfo[j].stream_coding_type;
                    if (isAudioStreamType(stream_coding_type) && !streamInfo[j].isSecondary)
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
    for (int i = 0; i < streamInfo.size(); i++)
    {
        int stream_coding_type = streamInfo[i].stream_coding_type;
        if (streamInfo[i].isSecondary && isVideoStreamType(stream_coding_type))
        {
            streamInfo[i].type = 3;
            streamInfo[i].composeStreamEntry(writer, PlayItem_id, secondaryVNum);
            streamInfo[i].composeStreamAttributes(writer);
            // comb_info_Secondary_video_Secondary_audio() {
            int useSecondaryAudio = number_of_secondary_audio_stream_entries > secondaryVNum ? 1 : 0;
            writer.putBits(8, useSecondaryAudio);
            writer.putBits(8, 0);  // reserved_for_word_align
            if (useSecondaryAudio)
            {
                writer.putBits(8, secondaryVNum);
                writer.putBits(8, 0);  // word align
            }
            //}

            // comb_info_Secondary_video_PiP_PG_textST(){
            writer.putBits(8, 0);  // number_of_PiP_PG_textST_ref_entries
            writer.putBits(8, 0);  // reserved_for_word_align
                                   //}
            secondaryVNum++;
        }
    }

    // DV video
    for (int i = 0; i < streamInfo.size(); i++)
    {
        int stream_coding_type = streamInfo[i].stream_coding_type;
        if (isVideoStreamType(stream_coding_type) && streamInfo[i].HDR == 4)
        {
            streamInfo[i].type = 4;
            streamInfo[i].composeStreamEntry(writer, PlayItem_id);
            streamInfo[i].composeStreamAttributes(writer);
        }
    }

    if (isSSEx && writer.getBitsCount() % 32 != 0)
        writer.putBits(16, 0);

    *lengthPos = my_htons(writer.getBitsCount() / 8 - beforeCount);
}

void MPLSParser::STN_table(BitStreamReader& reader, int PlayItem_id)
{
    int length = reader.getBits(16);                                  // 16 uimsbf
    reader.skipBits(16);                                              // reserved_for_future_use 16 bslbf
    number_of_primary_video_stream_entries = reader.getBits(8);       // 8 uimsbf
    number_of_primary_audio_stream_entries = reader.getBits(8);       // 8 uimsbf
    number_of_PG_textST_stream_entries = reader.getBits(8);           // 8 uimsbf
    number_of_IG_stream_entries = reader.getBits(8);                  // 8 uimsbf
    number_of_secondary_audio_stream_entries = reader.getBits(8);     // 8 uimsbf
    number_of_secondary_video_stream_entries = reader.getBits(8);     // 8 uimsbf
    number_of_PiP_PG_textST_stream_entries_plus = reader.getBits(8);  // 8 uimsbf
    number_of_DolbyVision_video_stream_entries = reader.getBits(8);   // 8 uimsbf
    reader.skipBits(32);                                              // reserved_for_future_use 32 bslbf

    std::vector<MPLSStreamInfo> streamInfos;

    for (int primary_video_stream_id = 0; primary_video_stream_id < number_of_primary_video_stream_entries;
         primary_video_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        streamInfos.push_back(streamInfo);
    }
    for (int primary_audio_stream_id = 0; primary_audio_stream_id < number_of_primary_audio_stream_entries;
         primary_audio_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        streamInfos.push_back(streamInfo);
    }

    for (int PG_textST_stream_id = 0;
         PG_textST_stream_id < number_of_PG_textST_stream_entries + number_of_PiP_PG_textST_stream_entries_plus;
         PG_textST_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        streamInfos.push_back(streamInfo);
    }

    for (int IG_stream_id = 0; IG_stream_id < number_of_IG_stream_entries; IG_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        streamInfos.push_back(streamInfo);
    }

    for (int secondary_audio_stream_id = 0; secondary_audio_stream_id < number_of_secondary_audio_stream_entries;
         secondary_audio_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.isSecondary = true;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        streamInfos.push_back(streamInfo);

        // comb_info_Secondary_audio_Primary_audio(){
        int number_of_primary_audio_ref_entries = reader.getBits(8);  // 8 uimsbf
        reader.skipBits(8);                                           // reserved_for_word_align  8 bslbf
        for (int i = 0; i < number_of_primary_audio_ref_entries; i++)
        {
            int primary_audio_stream_id_ref = reader.getBits(8);  // 8 uimsbf
        }
        if (number_of_primary_audio_ref_entries % 2 == 1)
        {
            reader.skipBits(8);  // reserved_for_word_align
        }
        //}
    }
    for (int secondary_video_stream_id = 0; secondary_video_stream_id < number_of_secondary_video_stream_entries;
         secondary_video_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.isSecondary = true;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        streamInfos.push_back(streamInfo);

        // comb_info_Secondary_video_Secondary_audio(){
        int number_of_secondary_audio_ref_entries = reader.getBits(8);  // 8 uimsbf
        reader.skipBits(8);                                             // reserved_for_word_align 8 bslbf
        for (int i = 0; i < number_of_secondary_audio_ref_entries; i++)
        {
            int secondary_audio_stream_id_ref = reader.getBits(8);  // 8 uimsbf
        }
        if (number_of_secondary_audio_ref_entries % 2 == 1)
        {
            reader.skipBits(8);  // reserved_for_word_align 8 bslbf
        }
        //}
        // comb_info_Secondary_video_PiP_PG_textST(){
        int number_of_PiP_PG_textST_ref_entries = reader.getBits(8);  // 8 uimsbf
        reader.skipBits(8);                                           // reserved_for_word_align 8 bslbf
        for (int i = 0; i < number_of_PiP_PG_textST_ref_entries; i++)
        {
            int PiP_PG_textST_stream_id_ref = reader.getBits(8);  // 8 uimsbf
        }
        if (number_of_PiP_PG_textST_ref_entries % 2 == 1)
        {
            reader.skipBits(8);  // reserved_for_word_align 8 bslbf
        }
        //}
    }
    for (int DV_video_stream_id = 0; DV_video_stream_id < number_of_DolbyVision_video_stream_entries;
         DV_video_stream_id++)
    {
        MPLSStreamInfo streamInfo;
        streamInfo.parseStreamEntry(reader);
        streamInfo.parseStreamAttributes(reader);
        streamInfos.push_back(streamInfo);
    }

    if (streamInfos.size() > m_streamInfo.size())
    {
        m_streamInfo = std::move(streamInfos);
        m_playItem = PlayItem_id;
    }
}

// ------------- M2TSStreamInfo -----------------------

void M2TSStreamInfo::blurayStreamParams(double fps, bool interlaced, int width, int height, float ar, int* video_format,
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

    if (ar == AR_3_4 || ar == AR_VGA)
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
    // memcpy(language_code, "eng", 3); // todo: delete this line
    if (pmtStreamInfo.m_codecReader != 0)
    {
        MPEGStreamReader* vStream = dynamic_cast<MPEGStreamReader*>(pmtStreamInfo.m_codecReader);
        if (vStream)
        {
            width = vStream->getStreamWidth();
            height = vStream->getStreamHeight();
            HDR = vStream->getStreamHDR();
            VideoAspectRatio ar = vStream->getStreamAR();
            blurayStreamParams(vStream->getFPS(), vStream->getInterlaced(), width, height, ar, &video_format,
                               &frame_rate_index, &aspect_ratio_index);
            if (ar == AR_3_4)
                width = height * 4 / 3;
            else if (ar == AR_16_9)
                width = height * 16 / 9;
            else if (ar == AR_221_100)
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
    : M2TSStreamInfo(), type(0), offsetId(0xff), isSSPG(false), SS_PG_offset_sequence_id(0xff), leftEye(0), rightEye(0)
{
}

MPLSStreamInfo::MPLSStreamInfo(const PMTStreamInfo& pmtStreamInfo)
    : M2TSStreamInfo(pmtStreamInfo),
      type(1),
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
    int length = reader.getBits(8);  // 8 uimsbf
    type = reader.getBits(8);        // 8 bslbf
    if (type == 1)
    {
        streamPID = reader.getBits(16);  // 16 uimsbf
        reader.skipBits(32);             // reserved_for_future_use 48 bslbf
        reader.skipBits(16);
    }
    else if (type == 2)
    {
        int ref_to_SubPath_id = reader.getBits(8);        // 8 uimsbf
        int ref_to_subClip_entry_id = reader.getBits(8);  // 8 uimsbf
        streamPID = reader.getBits(16);                   // 16 uimsbf
        reader.skipBits(32);                              // reserved_for_future_use 32 bslbf
    }
    else if (type == 3)
    {
        int ref_to_SubPath_id = reader.getBits(8);  // 8 uimsbf
        streamPID = reader.getBits(16);             // 16 uimsbf
        reader.skipBits(32);                        // reserved_for_future_use 40 bslbf
        reader.skipBits(8);
    }
    else if (type == 4)
    {
        reader.skipBits(8);
        streamPID = reader.getBits(16);  // 16 uimsbf
        reader.skipBits(32);             // reserved_for_future_use 40 bslbf
        reader.skipBits(8);
    }
}

void MPLSStreamInfo::composePGS_SS_StreamEntry(BitStreamWriter& writer, int entryNum)
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

void MPLSStreamInfo::composeStreamEntry(BitStreamWriter& writer, int entryNum, int subPathID)
{
    uint8_t* lengthPos = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);  // int length = reader.getBits(8); //8 uimsbf
    int initPos = writer.getBitsCount() / 8;
    writer.putBits(8, type);  // 8 bslbf
    if (type == 1)
    {
        writer.putBits(16, streamPID);  // 16 uimsbf
        writer.putBits(32, 0);          // reserved_for_future_use 48 bslbf
        writer.putBits(16, 0);
    }
    else if (type == 2)
    {
        writer.putBits(8, 0);  // ref_to_SubPath_id
        writer.putBits(8, 0);  // entryNum - ref_to_subClip_entry_id
        writer.putBits(16, streamPID);
        writer.putBits(32, 0);  // reserved
    }
    else if (type == 3)
    {
        writer.putBits(8, subPathID);   // ref_to_SubPath_id. constant subPathID == 0
        writer.putBits(16, streamPID);  // 16 uimsbf
        writer.putBits(32, 0);          // reserved_for_future_use 40 bslbf
        writer.putBits(8, 0);
    }
    else if (type == 4)
    {
        writer.putBits(8, 0);
        writer.putBits(16, streamPID);  // 16 uimsbf
        writer.putBits(32, 0);          // reserved_for_future_use 40 bslbf
        writer.putBits(8, 0);
    }
    else
        THROW(ERR_COMMON, "Unsupported media type for AVCHD/Blu-ray muxing");
    *lengthPos = writer.getBitsCount() / 8 - initPos;
}

void MPLSStreamInfo::parseStreamAttributes(BitStreamReader& reader)
{
    int length = reader.getBits(8);          // 8 uimsbf
    stream_coding_type = reader.getBits(8);  // 8 bslbf
    if (isVideoStreamType(stream_coding_type))
    {
        video_format = reader.getBits(4);      // 4 bslbf
        frame_rate_index = reader.getBits(4);  // 4 bslbf
        reader.skipBits(24);                   // reserved_for_future_use 24 bslbf
    }
    else if (isAudioStreamType(stream_coding_type))
    {
        audio_presentation_type = reader.getBits(4);       // 4 bslbf
        int sampling_frequency_index = reader.getBits(4);  // 4 bslbf
        CLPIStreamInfo::readString(language_code, reader, 3);
    }
    else if (stream_coding_type == 0x90)
    {
        // Presentation Graphics stream
        CLPIStreamInfo::readString(language_code, reader, 3);
        reader.skipBits(8);  // reserved_for_future_use 8 bslbf
    }
    else if (stream_coding_type == 0x91)
    {
        // Interactive Graphics stream
        CLPIStreamInfo::readString(language_code, reader, 3);
        reader.skipBits(8);  // reserved_for_future_use 8 bslbf
    }
    else if (stream_coding_type == 0x92)
    {
        // Text subtitle stream
        character_code = reader.getBits(8);  // 8 bslbf
        CLPIStreamInfo::readString(language_code, reader, 3);
    }
}

void MPLSStreamInfo::composeStreamAttributes(BitStreamWriter& writer)
{
    uint8_t* lengthPos = writer.getBuffer() + writer.getBitsCount() / 8;
    writer.putBits(8, 0);  // int length = reader.getBits(8); //8 uimsbf
    int initPos = writer.getBitsCount() / 8;

    writer.putBits(8, stream_coding_type);  // 8 bslbf
    if (isVideoStreamType(stream_coding_type))
    {
        writer.putBits(4, video_format);      // 4 bslbf
        writer.putBits(4, frame_rate_index);  // 4 bslbf
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
        writer.putBits(8, 0);  // reserved_for_future_use 8 bslbf
    }
    else if (isAudioStreamType(stream_coding_type))
    {
        writer.putBits(4, audio_presentation_type);   // 4 bslbf
        writer.putBits(4, sampling_frequency_index);  // 4 bslbf
        CLPIStreamInfo::writeString(language_code, writer, 3);
    }
    else if (stream_coding_type == 0x90)
    {
        // Presentation Graphics stream
        CLPIStreamInfo::writeString(language_code, writer, 3);
        writer.putBits(8, 0);  // reserved_for_future_use 8 bslbf
    }
    else
        THROW(ERR_COMMON, "Unsupported media type for AVCHD/Blu-ray muxing");
    *lengthPos = writer.getBitsCount() / 8 - initPos;
}
