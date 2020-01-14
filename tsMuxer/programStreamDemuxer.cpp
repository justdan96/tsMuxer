#include "programStreamDemuxer.h"

#include "mpegVideo.h"
#include "pesPacket.h"
#include "tsPacket.h"
#include "wave.h"

using namespace std;

static const uint32_t LPCM_FREQS[4] = {48000, 96000, 44100, 32000};

//#define min(a,b) a<=b?a:b

ProgramStreamDemuxer::ProgramStreamDemuxer(const BufferedReaderManager& readManager) : m_readManager(readManager)
{
    memset(m_psm_es_type, 0, sizeof(m_psm_es_type));
    memset(m_lpcpHeaderAdded, 0, sizeof(m_lpcpHeaderAdded));
    m_bufferedReader = (const_cast<BufferedReaderManager&>(m_readManager)).getReader(m_streamName.c_str());
    m_readerID = m_bufferedReader->createReader(MAX_PES_HEADER_SIZE);
    m_lastReadRez = 0;
    m_lastPesLen = 0;
    m_lastPID = 0;
    m_tmpBufferLen = 0;
    m_firstPTS = -1;
    m_firstVideoPTS = -1;
}

void ProgramStreamDemuxer::setFileIterator(FileNameIterator* itr)
{
    BufferedReader* br = dynamic_cast<BufferedReader*>(m_bufferedReader);
    if (br)
        br->setFileIterator(itr, m_readerID);
    else if (itr != 0)
        THROW(ERR_COMMON, "Can not set file iterator. Reader does not support bufferedReader interface.");
}

ProgramStreamDemuxer::~ProgramStreamDemuxer() { m_bufferedReader->deleteReader(m_readerID); }

void ProgramStreamDemuxer::readClose() {}

uint64_t ProgramStreamDemuxer::getDemuxedSize() { return m_dataProcessed; }

void ProgramStreamDemuxer::openFile(const std::string& streamName)
{
    m_streamName = streamName;
    readClose();
    // BufferedFileReader* fileReader = dynamic_cast <BufferedFileReader*> (m_bufferedReader);
    if (!m_bufferedReader->openStream(m_readerID, m_streamName.c_str()))
        THROW(ERR_FILE_NOT_FOUND, "Can't open stream " << m_streamName);
    m_readCnt = 0;
    m_dataProcessed = 0;
    m_notificated = false;
}

int ProgramStreamDemuxer::mpegps_psm_parse(uint8_t* buff, uint8_t* end)
{
    if (end - buff < 7)
        return -1;
    uint8_t* curBuff = buff + 4;
    // int map_stream_id = *curBuff++;
    int psm_length = (*curBuff << 8) + curBuff[1];
    if (psm_length > MAX_PES_HEADER_SIZE)
    {
        THROW(ERR_COMMON,
              "Can't parse Program Stream Map. Too large size " << psm_length << ". Max allowed size 1018 bytes.");
    }
    if (end - buff < psm_length + 7)
        return -1;

    curBuff += 4;
    int ps_info_length = (*curBuff << 8) + curBuff[1];
    curBuff += ps_info_length + 2;
    int es_map_length = (*curBuff << 8) + curBuff[1];
    curBuff += 2;
    /* at least one es available? */
    while (es_map_length >= 4)
    {
        unsigned char type = *curBuff++;
        unsigned char es_id = *curBuff++;
        /* remember mapping from stream id to stream type */
        m_psm_es_type[es_id] = type;
        /* skip program_stream_info */
        uint16_t es_info_length = (*curBuff << 8) + curBuff[1];
        curBuff += 2;
        curBuff += es_info_length;
        es_map_length -= 4 + es_info_length;
    }
    return 6 + psm_length;
}

long ProgramStreamDemuxer::processPES(uint8_t* buff, uint8_t* end, int& afterPesHeader)
{
    afterPesHeader = 0;
    uint32_t pstart_code = 0;
    long startcode = 0;
    // c, flags, header_len;
    // int pes_ext, ext2_len, id_ext, skip;
    // int64_t pts, dts;

    PESPacket* pesPacket = (PESPacket*)buff;
    startcode = buff[3];

    if (startcode == PACK_START_CODE)
        return 0;
    if (startcode == SYSTEM_HEADER_START_CODE)
        return 0;
    if (startcode == PADDING_STREAM || startcode == PES_PRIVATE_DATA2)
    {
        // skip them
        // len = get_be16(&s->pb);
        // url_fskip(&s->pb, len);
        return 0;
    }
    if (startcode == PES_PROGRAM_STREAM_MAP)
    {
        // mpegps_psm_parse(m, &s->pb);
        return 0;
    }

    /* find matching stream */
    if (!((startcode >= 0xc0 && startcode <= 0xdf) || (startcode >= 0xe0 && startcode <= 0xef) || (startcode == 0xbd) ||
          (startcode == 0xfd)))
        return 0;

    uint8_t* curBuf = buff + 9;
    if ((pesPacket->flagsLo & 0xc0) == 0xc0)
        curBuf += 10;
    else if ((pesPacket->flagsLo & 0xc0) == 0x80)
        curBuf += 5;
    if ((pesPacket->flagsLo & 0x20))  // ESCR_flag
        curBuf += 6;
    if ((pesPacket->flagsLo & 0x10))  // ES_rate_flag
        curBuf += 3;
    if ((pesPacket->flagsLo & 0x08))  // DSM_trick_mode_flag
        curBuf++;
    if ((pesPacket->flagsLo & 0x04))  // additional_copy_info_flag
        curBuf++;
    if (pesPacket->flagsLo & 0x02)  // PES_CRC_flag 1 bslbf
        curBuf += 2;
    if (pesPacket->flagsLo & 0x01)
    {  // PES_extension_flag 1 bslbf
        uint8_t extFlag = *curBuf++;
        if (extFlag & 0x80)  // PES_private_data_flag
            curBuf += 128;
        if (extFlag & 0x40)
        {  // pack_header_field_flag
            int pack_field_length = *curBuf++;
            curBuf += pack_field_length;
        }
        if (extFlag & 0x20)  // program_packet_sequence_counter_flag
            curBuf += 2;
        if (extFlag & 0x10)  // P-STD_buffer_flag
            curBuf += 2;
        if (extFlag & 0x01)
        {  // PES_extension_flag_2
            int ext2_len = *curBuf++ & 0x7f;
            if (ext2_len > 0)
                startcode = (startcode << 8) + *curBuf;
            curBuf += ext2_len;
        }
    }
    curBuf = buff + pesPacket->getHeaderLength();
    if (startcode == PES_PRIVATE_DATA1 && m_psm_es_type[startcode] == 0)
    {
        afterPesHeader++;
        startcode = *curBuf++;
        if (startcode >= 0xb0 && startcode <= 0xbf)
        {  // MLP/TrueHD audio has a 4-byte header, keep header
        }
        else if (startcode >= 0xa0 && startcode <= 0xaf)
        {  // PCM

            MemoryBlock& waveHeader = m_lpcmWaveHeader[startcode - 0xa0];

            // header[0] emphasis (1), muse(1), reserved(1), frame number(5)
            // header[1] quant (2), freq(2), reserved(1), channels(3)
            // header[2] dynamic range control (0x80 = off)
            int bitdepth = 16 + (curBuf[4] >> 6 & 3) * 4;

            if (waveHeader.isEmpty())
            {
                int samplerate = LPCM_FREQS[curBuf[4] >> 4 & 3];
                int channels = 1 + (curBuf[4] & 7);
                wave_format::buildWaveHeader(waveHeader, samplerate, channels, channels >= 6, bitdepth);
            }

            // uint16_t dataLen = pesPacket->getPacketLength() - pesPacket->getHeaderLength() - 7;
            // AV_WB16(curBuf+1, dataLen);
            // afterPesHeader++;
            afterPesHeader += 6;

            uint8_t* payloadData = curBuf + afterPesHeader - 1;
            uint32_t pesPayloadLen = pesPacket->getPacketLength() - pesPacket->getHeaderLength() - afterPesHeader;
            wave_format::toLittleEndian(payloadData, payloadData, pesPayloadLen, bitdepth);
        }
        else if (startcode >= 0x80 && startcode <= 0xcf)
        {
            // audio: skip header
            afterPesHeader += 3;
            /*
if (startcode >= 0xb0 && startcode <= 0xbf) {
                    // MLP/TrueHD audio has a 4-byte header
                    afterPesHeader++;
}
            */
        }
    }

    return startcode;
}

void ProgramStreamDemuxer::getTrackList(std::map<uint32_t, TrackInfo>& trackList)
{
    for (int i = 0x20; i < 0xff; i++)
    {
        if (i >= 0xa0 && i <= 0xaf)
            trackList.insert(std::make_pair(i, TrackInfo(STREAM_TYPE_AUDIO_LPCM, "", 0)));  // set track hint
        else
            trackList.insert(std::make_pair(i, TrackInfo(0, "", 0)));  // autodetect
    }
    return;
}

bool ProgramStreamDemuxer::isVideoPID(uint32_t pid)
{
    return pid >= 0x55 && pid <= 0x5f ||  // vc1
           pid >= 0xe0 && pid <= 0xef ||  // mpeg video
           m_psm_es_type[pid & 0xff] == STREAM_TYPE_VIDEO_H264 || m_psm_es_type[pid & 0xff] == STREAM_TYPE_VIDEO_MVC;
}

// static uint64_t prevDts = 0;
// static int ggCnt = 0;

int ProgramStreamDemuxer::simpleDemuxBlock(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, int64_t& discardSize)
{
    discardSize = 0;
    uint32_t readedBytes;
    int readRez = 0;
    uint8_t* data = m_bufferedReader->readBlock(m_readerID, readedBytes, readRez);  // blocked read mode
    if (readRez == BufferedFileReader::DATA_NOT_READY)
    {
        m_lastReadRez = readRez;
        return BufferedFileReader::DATA_NOT_READY;
    }
    if (readedBytes + m_tmpBufferLen == 0 || readedBytes == 0 && m_lastReadRez == BufferedReader::DATA_EOF)
    {
        m_lastReadRez = readRez;
        return BufferedReader::DATA_EOF;
    }
    if (readedBytes > 0)
        m_bufferedReader->notify(m_readerID, readedBytes);

    m_lastReadRez = readRez;
    data += MAX_PES_HEADER_SIZE;
    assert(m_tmpBufferLen <= MAX_PES_HEADER_SIZE);
    if (m_tmpBufferLen > 0)
    {
        memcpy(data - m_tmpBufferLen, m_tmpBuffer, m_tmpBufferLen);
        data -= m_tmpBufferLen;
        readedBytes += m_tmpBufferLen;
        m_tmpBufferLen = 0;
    }

    uint8_t* end = data + readedBytes;
    uint8_t* curBuf = data;

    if (m_lastPesLen > 0)
    {
        int copyLen = FFMIN(readedBytes, m_lastPesLen);
        if (m_lastPID > 0)
        {
            StreamData& vect = demuxedData[m_lastPID];
            vect.append(curBuf, copyLen);
            m_dataProcessed += copyLen;
        }
        else
            discardSize += copyLen;
        curBuf += copyLen;
        m_lastPesLen -= copyLen;
        if (m_lastPesLen > 0)
        {
            if (readedBytes > 0)
                return 0;
            else
                return BufferedReader::DATA_EOF;
        }
    }

    uint8_t* prevBuf = curBuf;
    curBuf = MPEGHeader::findNextMarker(curBuf, end);
    discardSize += curBuf - prevBuf;

    // while (curBuf <= end - TS_FRAME_SIZE)
    while (curBuf <= end - 9)
    {
        PESPacket* pesPacket = (PESPacket*)curBuf;
        uint8_t startcode = curBuf[3];
        if ((startcode >= 0xc0 && startcode <= 0xef) || (startcode == PES_PRIVATE_DATA1) || (startcode == PES_VC1_ID) ||
            (startcode == PES_PRIVATE_DATA2))
        {
            if (curBuf + pesPacket->getHeaderLength() + 4 > end)
                break;
            int afterPesHeader = 0;
            startcode = processPES(curBuf, end, afterPesHeader);
            if (acceptedPIDs.find(startcode) != acceptedPIDs.end())
            {
                if ((pesPacket->flagsLo & 0x80) == 0x80)
                {
                    int64_t curPts = pesPacket->getPts();
                    if (m_firstPTS == -1 || curPts < m_firstPTS)
                        m_firstPTS = curPts;
                    if (isVideoPID(startcode) && (m_firstVideoPTS == -1 || curPts < m_firstVideoPTS))
                        m_firstVideoPTS = curPts;
                    if (m_firstPtsTime.find(startcode) == m_firstPtsTime.end())
                        m_firstPtsTime[startcode] = curPts;
                    else if (curPts < m_firstPtsTime[startcode])
                        m_firstPtsTime[startcode] = curPts;

                    /*
                    if (startcode == 85)
                    {
                            uint64_t tmpDts = ((pesPacket->flagsLo & 0xc0) == 0xc0) ? pesPacket->getDts() :
                                    pesPacket->getPts();
                            int32_t dtsDif = (int64_t)tmpDts - prevDts;
                            uint8_t* afterPesData = curBuf + pesPacket->getHeaderLength();
                            LTRACE(LT_INFO, 2, "Cnt=" << ++ggCnt << "  PTS: " << pesPacket->getPts() <<
                                    " DTS:" << tmpDts << "  dtsDif:" << dtsDif <<
                                    " after pes 0x" << int32ToStr(afterPesData[0],16) << int32ToStr(afterPesData[1],16)
                    << int32ToStr(afterPesData[2],16) << int32ToStr(afterPesData[3],16)); prevDts = tmpDts;
                    }
                    */
                }

                StreamData& vect = demuxedData[startcode];

                int idx = startcode - 0xa0;
                if (idx >= 0 && idx <= 15 && !m_lpcpHeaderAdded[idx])
                {
                    vect.append(m_lpcmWaveHeader[idx].data(), m_lpcmWaveHeader[idx].size());
                    m_lpcpHeaderAdded[idx] = true;
                }

                uint8_t* payloadData = curBuf + pesPacket->getHeaderLength() + afterPesHeader;
                uint32_t pesPayloadLen = pesPacket->getPacketLength() - pesPacket->getHeaderLength() - afterPesHeader;
                uint32_t copyLen = FFMIN(pesPayloadLen, end - payloadData);
                vect.append(payloadData, copyLen);
                m_dataProcessed += copyLen;
                discardSize += payloadData - curBuf;
                if (copyLen < pesPayloadLen)
                {
                    m_lastPesLen = pesPayloadLen - copyLen;
                    m_lastPID = startcode;
                    return 0;
                }
                curBuf += pesPacket->getPacketLength();
            }
            else
            {
                uint32_t tmpLen = pesPacket->getPacketLength();
                if (tmpLen > end - curBuf)
                {
                    discardSize += end - curBuf;
                    m_lastPID = 0;
                    m_lastPesLen = tmpLen - (end - curBuf);
                    return 0;
                }
                curBuf += tmpLen;
                discardSize += tmpLen;
            }
        }
        else if (startcode == PES_PROGRAM_STREAM_MAP)
        {
            int psmLen = mpegps_psm_parse(curBuf, end);
            if (psmLen == -1)
                break;
            else if (psmLen > end - curBuf)
            {
                discardSize += end - curBuf;
                m_lastPID = 0;
                m_lastPesLen = psmLen - (end - curBuf);
                return 0;
            }
            discardSize += psmLen;
            curBuf += psmLen;
        }
        else
        {
            uint32_t rest = FFMIN(end - curBuf, 4);
            curBuf += rest;
            discardSize += rest;
        }
        prevBuf = curBuf;
        curBuf = MPEGHeader::findNextMarker(curBuf, end);
        discardSize += curBuf - prevBuf;
    }
    m_tmpBufferLen = end - curBuf;
    if (m_tmpBufferLen > 0)
        memcpy(m_tmpBuffer, curBuf, end - curBuf);
    return 0;
}

int64_t getLastPCR(File& file, int bufferSize, int64_t fileSize)
{
    file.seek(FFMAX(0, fileSize - bufferSize), File::smBegin);
    uint8_t* tmpBuffer = new uint8_t[bufferSize];
    int len = file.read(tmpBuffer, bufferSize);
    if (len < 1)
        return -2;
    uint8_t* curPtr = tmpBuffer;
    uint8_t* bufEnd = tmpBuffer + len;
    int64_t lastPcrVal = -1;

    curPtr = MPEGHeader::findNextMarker(curPtr, bufEnd);
    while (curPtr <= bufEnd - 9)
    {
        PESPacket* pesPacket = (PESPacket*)curPtr;
        uint8_t startcode = curPtr[3];
        if ((startcode >= 0xc0 && startcode <= 0xef) || (startcode == PES_PRIVATE_DATA1) || (startcode == PES_VC1_ID) ||
            (startcode == PES_PRIVATE_DATA2))
        {
            if ((pesPacket->flagsLo & 0x80) == 0x80)
                lastPcrVal = pesPacket->getPts();
        }
        curPtr = MPEGHeader::findNextMarker(curPtr + 4, bufEnd);
    }
    delete[] tmpBuffer;
    return lastPcrVal;
}

int64_t getPSDuration(const char* fileName)
{
    int BUF_SIZE = 1024 * 256;

    try
    {
        uint64_t fileSize;
        File file(fileName, File::ofRead);
        if (!file.size(&fileSize))
            return 0;

        uint8_t* tmpBuffer = new uint8_t[BUF_SIZE];

        // pcr from start of file
        int len = file.read(tmpBuffer, BUF_SIZE);
        if (len < 1)
        {
            delete[] tmpBuffer;
            return 0;
        }
        uint8_t* curPtr = tmpBuffer;
        int64_t firstPcrVal = 0;
        uint8_t* bufEnd = tmpBuffer + len;
        curPtr = MPEGHeader::findNextMarker(curPtr, bufEnd);
        while (curPtr <= bufEnd - 9)
        {
            PESPacket* pesPacket = (PESPacket*)curPtr;
            uint8_t startcode = curPtr[3];
            if ((startcode >= 0xc0 && startcode <= 0xef) || (startcode == PES_PRIVATE_DATA1) ||
                (startcode == PES_VC1_ID) || (startcode == PES_PRIVATE_DATA2))
            {
                if ((pesPacket->flagsLo & 0x80) == 0x80)
                {
                    firstPcrVal = pesPacket->getPts();
                    break;
                }
            }
            curPtr = MPEGHeader::findNextMarker(curPtr + 4, bufEnd);
        }
        delete[] tmpBuffer;

        // pcr from end of file

        int64_t lastPcrVal = -1;
        int bufferSize = BUF_SIZE;
        do
        {
            lastPcrVal = getLastPCR(file, bufferSize, fileSize);
            bufferSize *= 4;
        } while (lastPcrVal == -1 && bufferSize <= 1024 * 1024);

        file.close();

        return lastPcrVal >= 0 ? lastPcrVal - firstPcrVal : 0;
    }
    catch (...)
    {
        return 0;
    }
}

int64_t ProgramStreamDemuxer::getFileDurationNano() const
{
    return getPSDuration(m_streamName.c_str()) * 1000000000ll / 90000ll;
}
