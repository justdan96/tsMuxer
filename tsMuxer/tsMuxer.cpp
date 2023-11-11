#include "tsMuxer.h"

#include <cmath>

#include <fs/systemlog.h>
#include <fs/textfile.h>

#include "ac3StreamReader.h"
#include "dtsStreamReader.h"
#include "h264StreamReader.h"
#include "mpegAudioStreamReader.h"
#include "mpegStreamReader.h"
#include "muxerManager.h"
#include "pesPacket.h"
#include "tsPacket.h"
#include "vodCoreException.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace std;

int V3_flags = 0;
unsigned HDR10_metadata[6] = {0, 0, 0, 0, 0, 0};
bool isV3() { return V3_flags & HDMV_V3; }
bool is4K() { return V3_flags & FOUR_K; }

static constexpr int64_t M_PCR_DELTA = 7000;
static constexpr int64_t SIT_INTERVAL = 76900;
static constexpr int64_t M_CBR_PCR_DELTA = 7000;

static constexpr int64_t DEFAULT_VBV_BUFFER_LEN = 500;  // default 500 ms vbv buffer

static constexpr int PAT_PID = 0;
static constexpr int SIT_PID = 0x1f;
static constexpr int NULL_PID = 8191;

uint8_t DefaultSitTableOne[] = {
    0x47, 0x40, 0x1f, 0x10, 0x00, 0x7f, 0xf0, 0x19, 0xff, 0xff, 0xc1, 0x00, 0x00, 0xf0, 0x0a, 0x63, 0x08, 0xc1, 0xd4,
    0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x80, 0x00, 0x03, 0x00, 0x38, 0x6d, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// for v3 Blu-ray, change "peak_rate" and CRC to 128 mbps
uint8_t SitTableHEVC[] = {0xc4, 0xe1, 0x06, 0xff, 0xff, 0xff, 0xff, 0xff,
                          0x00, 0x01, 0x80, 0x00, 0x24, 0xc4, 0xba, 0xf0};

TSMuxer::TSMuxer(MuxerManager* owner) : AbstractMuxer(owner)
{
    m_m2tsMode = false;
    m_vbvLen = DEFAULT_VBV_BUFFER_LEN * 90;
    m_lastPESDTS = -1;
    m_cbrBitrate = -1;  // mux CBR if bitrate specifed
    m_minBitrate = -1;
    m_pcrOnVideo = true;
    m_endStreamDTS = 0;
    m_prevM2TSPCROffset = 0;
    m_pesPID = 0;
    m_lastTSIndex = -1;
    m_lastPesLen = -1;
    m_lastMuxedDts = -1;
    m_lastPCR = -1;
    m_lastPMTPCR = -1;
    m_lastSITPCR = -1;
    m_nullCnt = 0;
    m_pmtCnt = 0;
    m_patCnt = 0;
    m_sitCnt = 0;
    m_needTruncate = false;
    m_videoTrackCnt = 0;
    m_DVvideoTrackCnt = 0;
    m_videoSecondTrackCnt = 0;
    m_audioTrackCnt = 0;
    m_secondaryAudioTrackCnt = 0;
    m_pgsTrackCnt = 0;
    m_beforePCRDataWrited = false;
    m_useNewStyleAudioPES = false;
    m_minDts = -1;
    m_pcrBits = 0;
    m_pcr_delta = -1;
    m_patPmtDelta = -1;
    m_firstPts.push_back(-1);
    m_lastPts.push_back(-1);
    m_muxedPacketCnt.push_back(0);
    m_interleaveInfo.emplace_back();
    m_pesIFrame = false;
    m_pesSpsPps = false;
    m_computeMuxStats = false;
    m_pmtFrames = 0;
    m_curFileStartPts = 0;  // FIXED_PTS_OFFSET;
    m_splitSize = 0;
    m_splitDuration = 0;
    m_curFileNum = 0;
    m_bluRayMode = false;
    m_hdmvDescriptors = true;
    m_lastGopNullCnt = 0;
    m_outBufLen = 0;
    m_pesData.reserve(1024 * 128);
    m_mainStreamIndex = -1;
    m_muxFile = nullptr;
    m_isExternalFile = false;
    m_writeBlockSize = 0;
    m_frameSize = 188;

    m_processedBlockSize = 0;
    m_sublingMuxer = nullptr;
    m_outBuf = nullptr;
    m_masterMode = false;
    m_subMode = false;
    setPtsOffset(0);
    m_canSwithBlock = true;
    m_additionCLPISize = 0;
#ifdef _DEBUG
    m_lastProcessedDts = -1000000000;
    m_lastStreamIndex = -1;
#endif
}

void TSMuxer::setPtsOffset(const int64_t value)
{
    m_timeOffset = value;
    m_fixed_pcr_offset = m_timeOffset - DEFAULT_VBV_BUFFER_LEN * 90;
    m_prevM2TSPCR = m_fixed_pcr_offset * 300;
}

void TSMuxer::internalReset() { m_outBufLen = 0; }

TSMuxer::~TSMuxer()
{
    delete[] m_outBuf;
    if (!m_isExternalFile)
        delete m_muxFile;
}

void TSMuxer::setVBVBufferLen(const int value)
{
    m_vbvLen = static_cast<int64_t>(value) * 90;
    m_fixed_pcr_offset = m_timeOffset - m_vbvLen;
    if (m_fixed_pcr_offset < 0)
        m_fixed_pcr_offset = 0;
    m_prevM2TSPCR = m_fixed_pcr_offset * 300;
}

void TSMuxer::intAddStream(const std::string& streamName, const std::string& codecName, const int streamIndex,
                           const map<string, string>& params, AbstractStreamReader* codecReader)
{
    int descriptorLen = 0;
    uint8_t descrBuffer[1024];
    if (codecReader != nullptr)
        descriptorLen = codecReader->getTSDescriptor(descrBuffer, m_bluRayMode, m_hdmvDescriptors);

    if (codecName[0] == 'V' || (codecName[0] == 'A' && m_mainStreamIndex == -1))
        m_mainStreamIndex = streamIndex;

    string lang;
    auto itr = params.find("lang");
    if (itr != params.end())
        lang = itr->second;

    int tsStreamIndex = streamIndex + 16;
    const bool isSecondary = codecReader != nullptr ? codecReader->isSecondary() : false;

    if (codecName[0] == 'V')
    {
        if (!isSecondary)
        {
            const uint16_t doubleMux = (m_subMode || m_masterMode) ? 2 : 1;
            if (codecReader != nullptr && codecReader->getStreamHDR() == 4)
            {
                tsStreamIndex = 0x1015 + m_DVvideoTrackCnt * doubleMux;
                m_DVvideoTrackCnt++;
            }
            else
            {
                tsStreamIndex = 0x1011 + m_videoTrackCnt * doubleMux;
                m_videoTrackCnt++;
                V3_flags |= BL_TRACK;
            }
            if (m_subMode)
                tsStreamIndex++;
        }
        else
        {
            tsStreamIndex = 0x1B00 + m_videoSecondTrackCnt;
            m_videoSecondTrackCnt++;
        }
    }
    else if (codecName[0] == 'A')
    {
        if (isSecondary)
        {
            tsStreamIndex = 0x1A00 + m_secondaryAudioTrackCnt;
            m_secondaryAudioTrackCnt++;
        }
        else
        {
            tsStreamIndex = 0x1100 + m_audioTrackCnt;
            m_audioTrackCnt++;
        }
    }
    else if (codecName == "S_HDMV/PGS" || codecName == "S_TEXT/UTF8")
    {
        tsStreamIndex = (V3_flags & 0x1e ? 0x12A0 : 0x1200) + m_pgsTrackCnt;
        m_pgsTrackCnt++;
    }
    m_extIndexToTSIndex[streamIndex] = tsStreamIndex;

    m_pmt.program_number = 1;
    if (m_pcrOnVideo)
    {
        if (codecName[0] == 'V')
            m_pmt.pcr_pid = tsStreamIndex;
    }
    else
        m_pmt.pcr_pid = DEFAULT_PCR_PID;

    if (codecName[0] == 'V')
    {
        if (codecName == "V_MS/VFW/WVC1")
        {
            m_pesType[tsStreamIndex] = PES_VC1_ID;
        }
        else if (codecName == "V_MPEGH/ISO/HEVC")
        {
            m_pesType[tsStreamIndex] = PES_HEVC_ID;
        }
        else if (codecName == "V_MPEGI/ISO/VVC")
        {
            m_pesType[tsStreamIndex] = PES_VVC_ID;
        }
        else
        {
            m_pesType[tsStreamIndex] = PES_VIDEO_ID;
        }
    }
    else if (codecName[0] == 'A')
    {
        if (codecName == "A_AC3")
            m_pesType[tsStreamIndex] = PES_INT_AC3_ID;
        else if (codecName == "A_DTS")
            m_pesType[tsStreamIndex] = PES_INT_DTS_ID;
        else if (codecName == "A_MP3")
            m_pesType[tsStreamIndex] = PES_AUDIO_ID;
        else
            m_pesType[tsStreamIndex] = PES_PRIVATE_DATA1;
    }
    else if (codecName[0] == 'S')
    {
        if (codecName == "S_SUP" || codecName == "S_HDMV/PGS" || codecName == "S_TEXT/UTF8")
            m_pesType[tsStreamIndex] = PES_PRIVATE_DATA1;
        else
            m_pesType[tsStreamIndex] = PES_INT_SUB_ID;
    }

    if (codecName[0] == 'V')
    {
        itr = params.find("fps");
        if (itr != params.end())
        {
            double fps = strToDouble(itr->second.c_str());
            LTRACE(LT_DEBUG, 0, "Muxing fps: " << fps);
        }
    }

    if (codecName == "V_MPEG4/ISO/AVC")
    {
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::VIDEO_H264, tsStreamIndex, descrBuffer, descriptorLen,
                                                     codecReader, lang, isSecondary);
    }
    else if (codecName == "V_MPEG4/ISO/MVC")
    {
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::VIDEO_MVC, tsStreamIndex, descrBuffer, descriptorLen,
                                                     codecReader, lang, isSecondary);
    }
    else if (codecName == "V_MPEGH/ISO/HEVC")
    {
        StreamType stream_type = StreamType::VIDEO_H265;
        // Change "peak_rate" to 109 mbps + change descriptor CRC32
        memcpy(&DefaultSitTableOne[17], SitTableHEVC, 16);
        // For non-bluray, second Dolby Vision track must be stream_type 06 = private data
        if (!m_bluRayMode && tsStreamIndex == 0x1015 && (V3_flags & BL_TRACK))
            stream_type = StreamType::PRIVATE_DATA;
        // Dolby Vision profile 5 is not compatible with SDR or HDR and must be stream_type 06 = private data
        // else if (V3_flags & BL_NOTCOMPAT)
        //     stream_type = StreamType::PRIVATE_DATA;

        m_pmt.pidList[tsStreamIndex] =
            PMTStreamInfo(stream_type, tsStreamIndex, descrBuffer, descriptorLen, codecReader, lang, isSecondary);
    }
    else if (codecName == "V_MPEGI/ISO/VVC")
    {
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::VIDEO_H266, tsStreamIndex, descrBuffer, descriptorLen,
                                                     codecReader, lang, isSecondary);
    }
    else if (codecName == "V_MS/VFW/WVC1")
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::VIDEO_VC1, tsStreamIndex, descrBuffer, descriptorLen,
                                                     codecReader, lang, isSecondary);
    else if (codecName == "V_MPEG-2")
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::VIDEO_MPEG2, tsStreamIndex, descrBuffer, descriptorLen,
                                                     codecReader, lang, isSecondary);
    else if (codecName == "A_AAC")
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::AUDIO_AAC, tsStreamIndex, descrBuffer, descriptorLen,
                                                     codecReader, lang, isSecondary);
    else if (codecName == "S_SUP" || codecName == "S_HDMV/PGS" || codecName == "S_TEXT/UTF8")
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::SUB_PGS, tsStreamIndex, descrBuffer, descriptorLen,
                                                     codecReader, lang, isSecondary);
    else if (codecName == "A_AC3")
    {
        const auto ac3Reader = dynamic_cast<AC3StreamReader*>(codecReader);
        ac3Reader->setNewStyleAudioPES(m_useNewStyleAudioPES);

        StreamType streamType;

        if (ac3Reader->isTrueHD() && !ac3Reader->getDownconvertToAC3())
            streamType = StreamType::AUDIO_TRUE_HD;
        else if (ac3Reader->isEAC3() && !ac3Reader->getDownconvertToAC3())
        {
            if (m_bluRayMode || m_m2tsMode)
            {
                if (ac3Reader->isSecondary())
                    streamType = StreamType::AUDIO_EAC3_SECONDARY;
                else
                    streamType = StreamType::AUDIO_EAC3;
            }
            else
            {
                streamType = StreamType::AUDIO_EAC3_ATSC;
            }
        }
        else
            streamType = StreamType::AUDIO_AC3;

        m_pmt.pidList[tsStreamIndex] =
            PMTStreamInfo(streamType, tsStreamIndex, descrBuffer, descriptorLen, codecReader, lang, isSecondary);
    }
    else if (codecName == "A_MP3")
    {
        const auto mp3Reader = dynamic_cast<MpegAudioStreamReader*>(codecReader);
        if (mp3Reader->getLayer() >= 2)
            m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::AUDIO_MPEG2, tsStreamIndex, descrBuffer,
                                                         descriptorLen, codecReader, lang, isSecondary);
        else
            m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::AUDIO_MPEG1, tsStreamIndex, descrBuffer,
                                                         descriptorLen, codecReader, lang, isSecondary);
    }
    else if (codecName == "A_LPCM")
    {
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::AUDIO_LPCM, tsStreamIndex, descrBuffer, descriptorLen,
                                                     codecReader, lang, isSecondary);
    }
    else if (codecName == "A_MLP")
    {
        m_pmt.pidList[tsStreamIndex] = PMTStreamInfo(StreamType::AUDIO_TRUE_HD, tsStreamIndex, descrBuffer,
                                                     descriptorLen, codecReader, lang, isSecondary);
    }
    else if (codecName == "A_DTS")
    {
        const auto dtsReader = dynamic_cast<DTSStreamReader*>(codecReader);
        dtsReader->setNewStyleAudioPES(m_useNewStyleAudioPES);
        StreamType audioType;
        if (dtsReader->getDTSHDMode() != DTSStreamReader::DTSHD_SUBTYPE::DTS_SUBTYPE_UNINITIALIZED &&
            !dtsReader->getDownconvertToDTS())
        {
            // if (dtsReader->getDTSHDMode() == DTSStreamReader::DTS_SUBTYPE_EXPRESS)
            if (dtsReader->isSecondary())
                audioType = StreamType::AUDIO_DTS_HD_SECONDARY;
            else if (dtsReader->getDTSHDMode() == DTSStreamReader::DTSHD_SUBTYPE::DTS_SUBTYPE_MASTER_AUDIO)
                audioType = StreamType::AUDIO_DTS_HD_MA;
            else
                audioType = StreamType::AUDIO_DTS_HD;
        }
        else
        {
            audioType = StreamType::AUDIO_DTS;
        }
        m_pmt.pidList[tsStreamIndex] =
            PMTStreamInfo(audioType, tsStreamIndex, descrBuffer, descriptorLen, codecReader, lang, isSecondary);
    }
    else
        THROW(ERR_UNKNOWN_CODEC, "Unsupported codec " << codecName << " for TS/M2TS muxing.")

    m_streamInfo[DEFAULT_PCR_PID].m_tsCnt = 0;
    buildNULL();
    buildPAT();
    buildPMT();
}

bool TSMuxer::doFlush()
{
    if (!m_lastPts.empty())
        m_lastPts[m_lastPts.size() - 1] += m_additionCLPISize;

    int64_t newPCR = 0;
    if (m_m2tsMode)
    {
        newPCR = (m_endStreamDTS - m_minDts) / INT_FREQ_TO_TS_FREQ + m_fixed_pcr_offset;
        if (m_cbrBitrate != -1 && m_lastPCR != -1)
        {
            const auto cbrPCR = llround(static_cast<double>(m_lastPCR + m_pcrBits) * 90000.0 / m_cbrBitrate);
            newPCR = FFMAX(newPCR, cbrPCR);
        }
    }
    return doFlush(newPCR, 0);
}

bool TSMuxer::doFlush(const int64_t newPCR, const int64_t pcrGAP)
{
    flushTSFrame();

    if (m_sectorSize > 0)
    {
        // round file to sector size. It's also can be applied to any file in a m2ts mode
        int nullCnt = 0;
        if (m_processedBlockSize % m_sectorSize)
            nullCnt = m_sectorSize - static_cast<int>(m_processedBlockSize % m_sectorSize);
        writeNullPackets(nullCnt / m_frameSize);
    }

    if (m_m2tsMode)
        processM2TSPCR(newPCR, pcrGAP);

    const int lastBlockSize = m_outBufLen & (MuxerManager::PHYSICAL_SECTOR_SIZE - 1);  // last 64K of data
    const int roundBufLen = m_outBufLen & ~(MuxerManager::PHYSICAL_SECTOR_SIZE - 1);
    if (m_owner->isAsyncMode())
    {
        if (lastBlockSize > 0)
        {
            assert(m_sectorSize == 0);  // we should not be here in interleaved mode!
            const auto newBuff = new uint8_t[lastBlockSize];
            memcpy(newBuff, m_outBuf + roundBufLen, lastBlockSize);
            m_owner->asyncWriteBuffer(this, m_outBuf, roundBufLen, m_muxFile);
            m_outBuf = newBuff;
        }
        else
        {
            m_owner->asyncWriteBuffer(this, m_outBuf, roundBufLen, m_muxFile);
            m_outBuf = nullptr;
        }
    }
    else
    {
        m_owner->syncWriteBuffer(this, m_outBuf, roundBufLen, m_muxFile);
        memmove(m_outBuf, m_outBuf + roundBufLen, lastBlockSize);
    }
    m_outBufLen = lastBlockSize;
    assert(!m_sectorSize || m_outBufLen == 0);
    return true;
}

int TSMuxer::writeOutFile(const uint8_t* buffer, const int len) const
{
    const int rez = m_muxFile->write(buffer, len);
    return rez;
}

bool TSMuxer::close()
{
    if (m_isExternalFile)
        return true;

    if (m_sectorSize)
    {
        assert(m_outBufLen == 0 && m_muxFile->size() % m_sectorSize == 0);
    }

    if (!m_muxFile->close())
        return false;

    if (m_sectorSize)
        return true;

    if (m_outBufLen > 0)
    {
        if (!m_muxFile->open(m_outFileName.c_str(), AbstractOutputStream::ofWrite + AbstractOutputStream::ofAppend))
            return false;
        if (!writeOutFile(m_outBuf, m_outBufLen))
            return false;
    }
    const bool bRes = m_muxFile->close();
    return bRes;
}

int TSMuxer::calcM2tsFrameCnt() const
{
    int32_t byteCnt = 0;
    for (const auto& i : m_m2tsDelayBlocks) byteCnt += i.second;
    byteCnt -= m_prevM2TSPCROffset;
    byteCnt += m_outBufLen;
    assert(byteCnt % 192 == 0);
    return byteCnt / 192;
}

void TSMuxer::processM2TSPCR(const int64_t pcrVal, const int64_t pcrGAP)
{
    const int m2tsFrameCnt = calcM2tsFrameCnt();
    const int64_t hiResPCR = pcrVal * 300 - pcrGAP;
    const int64_t pcrValDif = hiResPCR - m_prevM2TSPCR;  // m2ts pcr clock based on full 27Mhz counter
    const double pcrIncPerFrame = static_cast<double>(pcrValDif) / m2tsFrameCnt;

    auto curM2TSPCR = static_cast<double>(m_prevM2TSPCR);
    uint8_t* curPos;
    if (!m_m2tsDelayBlocks.empty())
    {
        int offset = m_prevM2TSPCROffset;
        for (const auto& i : m_m2tsDelayBlocks)
        {
            curPos = i.first + offset;
            int j = offset;
            for (; j < i.second; j += 192)
            {
                curM2TSPCR += pcrIncPerFrame;
                writeM2TSHeader(curPos, llround(curM2TSPCR));
                curPos += 192;
            }
            if (m_owner->isAsyncMode())
                m_owner->asyncWriteBuffer(this, i.first, i.second, m_muxFile);
            else
            {
                m_owner->syncWriteBuffer(this, i.first, i.second, m_muxFile);
                delete[] i.first;
            }
            offset = j - i.second;
        }
        m_prevM2TSPCROffset = offset;
        m_m2tsDelayBlocks.clear();
    }
    curPos = m_outBuf + m_prevM2TSPCROffset;
    const uint8_t* end = m_outBuf + m_outBufLen;
    for (; curPos < end; curPos += 192)
    {
        curM2TSPCR += pcrIncPerFrame;
        writeM2TSHeader(curPos, llround(curM2TSPCR));
    }
    assert(curPos == end);
    m_prevM2TSPCROffset = m_outBufLen;
    // assert((int64_t) curM2TSPCR == hiResPCR);
    m_prevM2TSPCR = hiResPCR;
}

void TSMuxer::writeEmptyPacketWithPCRTest(const int64_t pcrVal)
{
    if (m_m2tsMode)
    {
        m_outBufLen += 4;
        m_processedBlockSize += 4;
        m_pcrBits += 4 * 8;
    }
    uint8_t* curBuf = m_outBuf + m_outBufLen;
    const auto tsPacket = reinterpret_cast<TSPacket*>(curBuf);
    const auto initTS = reinterpret_cast<uint32_t*>(curBuf);
    *initTS = TSPacket::TS_FRAME_SYNC_BYTE;
    initTS[1] = 0x07 + TSPacket::PCR_BIT_VAL;
    tsPacket->setPID(m_pmt.pcr_pid);
    tsPacket->afExists = 1;
    tsPacket->adaptiveField.setPCR33(pcrVal);
    tsPacket->counter = m_streamInfo[m_pmt.pcr_pid].m_tsCnt;  // do not increment counter because data_exists == 0

    memset(curBuf + tsPacket->getHeaderSize(), 0xff, TS_FRAME_SIZE - tsPacket->getHeaderSize());
    tsPacket->adaptiveField.length += TS_FRAME_SIZE - tsPacket->getHeaderSize();
    m_outBufLen += TS_FRAME_SIZE;
    m_processedBlockSize += TS_FRAME_SIZE;
    m_pcrBits += TS_FRAME_SIZE * 8;
    m_muxedPacketCnt[m_muxedPacketCnt.size() - 1]++;
}

void TSMuxer::writeEmptyPacketWithPCR(const int64_t pcrVal)
{
    if (m_m2tsMode)
    {
        m_outBufLen += 4;
        m_processedBlockSize += 4;
        m_pcrBits += 4 * 8;
    }
    uint8_t* curBuf = m_outBuf + m_outBufLen;
    const auto tsPacket = reinterpret_cast<TSPacket*>(curBuf);
    const auto initTS = reinterpret_cast<uint32_t*>(curBuf);
    *initTS = TSPacket::TS_FRAME_SYNC_BYTE;
    initTS[1] = 0x07 + TSPacket::PCR_BIT_VAL;
    tsPacket->setPID(m_pmt.pcr_pid);
    tsPacket->afExists = 1;
    tsPacket->adaptiveField.setPCR33(pcrVal);
    tsPacket->counter = m_streamInfo[m_pmt.pcr_pid].m_tsCnt;  // do not increment counter becouse data_exists == 0

    memset(curBuf + tsPacket->getHeaderSize(), 0xff, TS_FRAME_SIZE - tsPacket->getHeaderSize());
    tsPacket->adaptiveField.length += TS_FRAME_SIZE - tsPacket->getHeaderSize();
    m_outBufLen += TS_FRAME_SIZE;
    m_processedBlockSize += TS_FRAME_SIZE;
    m_pcrBits += TS_FRAME_SIZE * 8;
    m_muxedPacketCnt[m_muxedPacketCnt.size() - 1]++;
    if (m_m2tsMode)
        processM2TSPCR(pcrVal, 0);
    writeOutBuffer();
}

void TSMuxer::buildPesHeader(const uint8_t pesStreamID, AVPacket& avPacket, int pid)
{
    const int64_t curDts = internalClockToPts(avPacket.dts) + m_timeOffset;
    const int64_t curPts = internalClockToPts(avPacket.pts) + m_timeOffset;
    uint8_t tmpBuffer[2048]{0};
    const auto pesPacket = reinterpret_cast<PESPacket*>(tmpBuffer);
    if (curDts != curPts)
        pesPacket->serialize(curPts, curDts, pesStreamID);
    else
        pesPacket->serialize(curPts, pesStreamID);
    m_lastPESDTS = curDts;
    m_fullPesDTS = avPacket.dts;
    m_fullPesPTS = avPacket.pts;
    pesPacket->flagsHi |= PES_DATA_ALIGNMENT;
    // int additionDataSize = avPacket.codec->writePESExtension(pesPacket);
    const auto ast = dynamic_cast<AbstractStreamReader*>(avPacket.codec);
    if (ast)
        ast->writePESExtension(pesPacket, avPacket);
    if (avPacket.flags & AVPacket::IS_COMPLETE_FRAME)
        pesPacket->setPacketLength(avPacket.size + pesPacket->getHeaderLength());

    PriorityDataInfo tmpPriorityData;
    const int additionDataSize = avPacket.codec->writeAdditionData(
        tmpBuffer + pesPacket->getHeaderLength(), tmpBuffer + sizeof(tmpBuffer), avPacket, &tmpPriorityData);
    const int bufLen = pesPacket->getHeaderLength() + additionDataSize;
    m_pesData.resize(bufLen);
    memcpy(m_pesData.data(), tmpBuffer, bufLen);
    for (auto& i : tmpPriorityData) m_priorityData.emplace_back(i.first + pesPacket->getHeaderLength(), i.second);
}

void TSMuxer::addData(const uint8_t pesStreamID, const int pid, AVPacket& avPacket)
{
    int beforePesLen = static_cast<int>(m_pesData.size());
    if (m_pesData.size() == 0)
    {
        buildPesHeader(pesStreamID, avPacket, pid);
        m_pesPID = pid;
        m_pesIFrame = avPacket.flags & AVPacket::IS_IFRAME;
        m_pesSpsPps = avPacket.flags & AVPacket::IS_SPS_PPS_IN_GOP;
    }
    const int oldLen = static_cast<int>(m_pesData.size());
    const int pesHeaderLen = oldLen - beforePesLen;
    if (oldLen > 100000000)
        THROW(ERR_COMMON, "Pes packet len too large ( >100Mb). Bad stream or invalid codec speciffed.")
    m_pesData.grow(avPacket.size /*+ additionDataSize*/);
    uint8_t* dst = m_pesData.data() + oldLen;
    // memcpy(dst, tmpBuffer, additionDataSize);
    memcpy(dst /*+ additionDataSize*/, avPacket.data, avPacket.size);
    if (avPacket.flags & AVPacket::PRIORITY_DATA)
    {
        if (!m_priorityData.empty() && m_priorityData.rbegin()->first + m_priorityData.rbegin()->second == beforePesLen)
            m_priorityData.rbegin()->second += avPacket.size + pesHeaderLen;
        else
            m_priorityData.emplace_back(beforePesLen, avPacket.size + pesHeaderLen);
    }
}

void TSMuxer::flushTSFrame() { writePESPacket(); }

void TSMuxer::writePATPMT(const int64_t pcr, const bool force)
{
    if (pcr == -1 || pcr - m_lastPMTPCR >= m_patPmtDelta || force)
    {
        m_lastPMTPCR = pcr != -1 ? pcr : m_fixed_pcr_offset;
        writePAT();
        writePMT();
        if (force || pcr - m_lastSITPCR > SIT_INTERVAL)
        {
            m_lastSITPCR = pcr;
            writeSIT();
        }
    }
}

bool TSMuxer::isSplitPoint(const AVPacket& avPacket) const
{
    if (avPacket.stream_index != m_mainStreamIndex || !(avPacket.flags & AVPacket::IS_IFRAME))
        return false;

    if (m_splitSize > 0)
    {
        return m_muxedPacketCnt[m_muxedPacketCnt.size() - 1] * m_frameSize > m_splitSize;
    }
    if (m_splitDuration > 0)
    {
        return (avPacket.pts - m_curFileStartPts) > m_splitDuration;
    }

    return false;
}

int TSMuxer::getFirstFileNum() const
{
    const string fileName = extractFileName(m_origFileName);
    return strToInt32(fileName.c_str());
}

std::string TSMuxer::getNextName(const std::string curName)
{
    std::string result = curName;
    if (m_bluRayMode)
    {
        const string fileExt = extractFileExt(m_origFileName);
        const string fileName = extractFileName(m_origFileName);
        const string filePath = extractFilePath(m_origFileName);
        int origNum = strToInt32(fileName.c_str());
        origNum += m_curFileNum;
        m_curFileNum++;
        result = closeDirPath(filePath) + strPadLeft(int32ToStr(origNum), 5, '0') + "." + fileExt;
    }
    else if (m_splitSize > 0 || m_splitDuration > 0)
    {
        m_curFileNum++;
        const string fileExt = extractFileExt(m_origFileName);
        result = m_origFileName.substr(0, m_origFileName.size() - fileExt.size() - 1) + ".split." +
                 int32ToStr(m_curFileNum) + "." + fileExt;
    }
    return toNativeSeparators(result);
}

void TSMuxer::gotoNextFile(const int64_t newPts)
{
    // 2. CloseCurrentFile
    if (m_owner->isAsyncMode())
        m_owner->waitForWriting();

    m_muxFile->close();
    assert(m_outBufLen == 0);

    // 3. open new file
    m_outFileName = getNextName(m_outFileName);
    if (m_sublingMuxer)
        m_outFileName = getNextName(m_outFileName);  // inc by 2
    m_fileNames.push_back(m_outFileName);
    openDstFile();

    for (auto& [pid, si] : m_pmt.pidList)
    {
        PMTStreamInfo& pmtInfo = si;
        pmtInfo.m_codecReader->onSplitEvent();
        if (pmtInfo.m_index.empty())
            continue;
        pmtInfo.m_index.emplace_back();
    }

    m_muxedPacketCnt.push_back(0);
    m_interleaveInfo.emplace_back();
    m_interleaveInfo.rbegin()->push_back(0);

    m_lastPts[m_lastPts.size() - 1] = newPts;  //(curPts-FIXED_PTS_OFFSET)*INT_FREQ_TO_TS_FREQ;
    m_firstPts.push_back(newPts);              //((curPts-FIXED_PTS_OFFSET)*INT_FREQ_TO_TS_FREQ);
    m_lastPts.push_back(newPts);               //((curPts-FIXED_PTS_OFFSET)*INT_FREQ_TO_TS_FREQ);
    m_curFileStartPts = newPts;
}

void TSMuxer::writePESPacket()
{
    if (m_pesData.size() > 0)
    {
        uint32_t tsPackets = 0;

        const size_t size = m_pesData.size() - 6;
        if (size <= 0xffff)
        {
            m_pesData.data()[4] = static_cast<uint8_t>(size / 256);
            m_pesData.data()[5] = static_cast<uint8_t>(size % 256);
        }

        PMTStreamInfo& streamInfo = m_pmt.pidList[m_pesPID];
        const auto pesPacket = reinterpret_cast<PESPacket*>(m_pesData.data());
        bool updateIdx = false;
        if (m_computeMuxStats && (pesPacket->flagsLo & 0x80) == 0x80)
        {
            uint64_t curPts = pesPacket->getPts();

            size_t idxSize = streamInfo.m_index.size();
            if (idxSize == 0)
                streamInfo.m_index.emplace_back();
            const auto vCodec = dynamic_cast<MPEGStreamReader*>(streamInfo.m_codecReader);
            // bool isH264 = dynamic_cast <H264StreamReader*> (streamInfo.m_codecReader);
            const bool SPSRequired = streamInfo.m_codecReader->needSPSForSplit();
            const auto aCodec = dynamic_cast<SimplePacketizerReader*>(streamInfo.m_codecReader);
            if (vCodec && m_pesIFrame)
            {
                // skip some I-frames for H.264 if no SPS/PPS in a gop
                if (m_pesSpsPps || !SPSRequired)
                {
                    PMTIndex& curIndex = *streamInfo.m_index.rbegin();
                    if (curIndex.empty() || curPts > curIndex.rbegin()->first)
                    {
                        curIndex.insert(
                            std::make_pair(curPts, PMTIndexData(m_muxedPacketCnt[m_muxedPacketCnt.size() - 1], 0)));
                        updateIdx = true;
                    }
                }

                m_lastGopNullCnt = m_nullCnt;
            }
            else if (aCodec)
            {
                if (m_videoTrackCnt + m_videoSecondTrackCnt == 0)
                {
                    m_lastGopNullCnt = m_nullCnt;
                }
                PMTIndex& curIndex = *streamInfo.m_index.rbegin();
                idxSize = curIndex.size();
                if (idxSize == 0 || curPts - curIndex.rbegin()->first >= 90000)
                {
                    curIndex.insert(
                        std::make_pair(curPts, PMTIndexData(m_muxedPacketCnt[m_muxedPacketCnt.size() - 1], 0)));
                    updateIdx = true;
                }
            }
        }

        const uint8_t* curPtr = m_pesData.data();
        const uint8_t* dataEnd = curPtr + m_pesData.size();
        bool payloadStart = true;
        for (const auto& i : m_priorityData)
        {
            const uint8_t* blockPtr = m_pesData.data() + i.first;
            if (blockPtr > curPtr)
            {
                tsPackets += writeTSFrames(m_pesPID, curPtr, blockPtr - curPtr, false, payloadStart);
                payloadStart = false;
            }
            tsPackets += writeTSFrames(m_pesPID, blockPtr, i.second, true, payloadStart);
            curPtr = blockPtr + i.second;
        }
        tsPackets += writeTSFrames(m_pesPID, curPtr, dataEnd - curPtr, false, payloadStart);

        m_pesData.resize(0);
        m_priorityData.clear();
        if (updateIdx)
        {
            PMTIndex& curIndex = *streamInfo.m_index.rbegin();
            assert(curIndex.rbegin()->second.m_frameLen == 0);
            curIndex.rbegin()->second.m_frameLen = (tsPackets + 1) * m_frameSize;
        }
    }
}

void TSMuxer::writePCR(const int64_t newPCR)
{
    int bitsRest = 0;
    if (m_cbrBitrate != -1 && m_minBitrate != -1 && m_lastPCR != -1)
    {
        int expectedBits = lroundl(static_cast<double>(newPCR - m_lastPCR) / 90000.0 * m_minBitrate);
        expectedBits -= m_pcrBits;
        if (expectedBits > 0)
        {
            int tsFrames = expectedBits / 8 / m_frameSize;
            if (tsFrames * m_frameSize * 8 < expectedBits)
                tsFrames++;
            writeNullPackets(tsFrames);
            if (m_cbrBitrate == m_minBitrate)
                bitsRest = (tsFrames * m_frameSize * 8) - expectedBits;
        }
    }
    m_pcrBits = bitsRest;
    // assert(m_pcrBits % 8 == 0);
    writeEmptyPacketWithPCR(newPCR);
    m_lastPCR = newPCR;
}

void TSMuxer::flushTSBuffer()
{
    if (m_owner->isAsyncMode())
        m_owner->waitForWriting();

    const int64_t fileSize = m_muxFile->size();
    if (fileSize == -1)
        THROW(ERR_FILE_COMMON, "Can't determine size for file " << m_outFileName)
    m_muxFile->close();

    if (!m_muxFile->open(m_outFileName.c_str(), File::ofWrite + File::ofAppend))
        THROW(ERR_FILE_COMMON, "Can't reopen file " << m_outFileName)

    if (writeOutFile(m_outBuf, m_outBufLen) != m_outBufLen)
        THROW(ERR_FILE_COMMON, "Can't write last data block to file " << m_outFileName)
    delete[] m_outBuf;
    m_outBufLen = 0;
}

void TSMuxer::finishFileBlock(const int64_t newPts, const int64_t newPCR, const bool doChangeFile, const bool recursive)
{
    if (m_processedBlockSize > 0)
    {
        constexpr auto gapForPATPMT = static_cast<int64_t>((192 * 4.0) / 35000000.0 * 27000000.0);
        doFlush(newPCR, gapForPATPMT);
        if (!doChangeFile)
            m_interleaveInfo.rbegin()->push_back(static_cast<int32_t>(m_processedBlockSize / 192));
        m_processedBlockSize = 0;
        m_owner->muxBlockFinished(this);
        if (m_m2tsMode)
            assert(m_outBuf == nullptr && m_outBufLen == 0);
        else
            flushTSBuffer();
        m_outBuf = new uint8_t[m_writeBlockSize + 1024];
        m_prevM2TSPCROffset = 0;
    }

    if (doChangeFile)
        gotoNextFile(newPts);

    writePATPMT(newPCR, true);
    writePCR(newPCR);

    if (m_sublingMuxer && recursive)
        m_sublingMuxer->finishFileBlock(newPts, newPCR, doChangeFile, false);
}

bool TSMuxer::blockFull() const { return m_processedBlockSize >= m_interliaveBlockSize; }

bool TSMuxer::muxPacket(AVPacket& avPacket)
{
    if (avPacket.data == nullptr || avPacket.size == 0)
    {
        return true;
    }

#ifdef _DEBUG
    // assert (avPacket.dts >= m_lastProcessedDts);
    m_lastProcessedDts = avPacket.dts;
    m_lastStreamIndex = avPacket.stream_index;
#endif

    if (avPacket.stream_index == m_mainStreamIndex)
    {
        const auto mpegReader = dynamic_cast<MPEGStreamReader*>(avPacket.codec);
        if (mpegReader)
            m_additionCLPISize = static_cast<int64_t>(INTERNAL_PTS_FREQ / mpegReader->getFPS());

        if (avPacket.duration > 0)
            *m_lastPts.rbegin() = FFMAX(*m_lastPts.rbegin(), avPacket.pts + avPacket.duration);
        else
            *m_lastPts.rbegin() = FFMAX(*m_lastPts.rbegin(), avPacket.pts);
    }
    if (m_firstPts[m_firstPts.size() - 1] == -1 /*|| avPacket.pts < m_firstPts[m_firstPts.size()-1]*/)
    {
        if (m_firstPts[m_firstPts.size() - 1] == -1)
        {
            m_curFileStartPts = avPacket.pts;
            const int64_t firstPtsShift = internalClockToPts(avPacket.pts);
            m_fixed_pcr_offset = m_timeOffset - m_vbvLen + firstPtsShift;
            if (m_fixed_pcr_offset < 0)
                m_fixed_pcr_offset = 0;
            m_prevM2TSPCR = (m_fixed_pcr_offset - 100) *
                            300;  // add a little extra delta (100) to prevent 0 bitrate for first several packets
        }
        m_firstPts[m_firstPts.size() - 1] = avPacket.pts;
    }

    if (m_pcr_delta == -1)
    {
        if (m_cbrBitrate != -1)
            m_pcr_delta = M_CBR_PCR_DELTA;
        else
            m_pcr_delta = M_PCR_DELTA;
        m_patPmtDelta = m_m2tsMode ? M_PCR_DELTA : PCR_FREQUENCY / 4;
    }

    if (m_minDts == -1)
        m_minDts = avPacket.dts;

    const int tsIndex = m_extIndexToTSIndex[avPacket.stream_index];
    if (tsIndex == 0)
        THROW(ERR_TS_COMMON, "Unknown track number " << avPacket.stream_index)

    auto newPCR = (avPacket.dts - m_minDts) / INT_FREQ_TO_TS_FREQ + m_fixed_pcr_offset;
    if (m_lastPCR == -1)
    {
        writePATPMT(newPCR, true);
        writePCR(newPCR);
    }

    bool newPES = false;
    if (avPacket.dts != m_streamInfo[tsIndex].m_dts || avPacket.pts != m_streamInfo[tsIndex].m_pts ||
        tsIndex != m_lastTSIndex || avPacket.flags & AVPacket::FORCE_NEW_FRAME)
    {
        writePESPacket();
        newPES = true;
    }

    m_lastTSIndex = tsIndex;

    if (m_cbrBitrate != -1 && m_lastPCR != -1)
    {
        const auto cbrPCR = llround(static_cast<double>(m_lastPCR + m_pcrBits) * 90000.0 / m_cbrBitrate);
        newPCR = FFMAX(newPCR, cbrPCR);
    }

    if (newPES && m_canSwithBlock && isSplitPoint(avPacket))
    {
        finishFileBlock(avPacket.pts, newPCR, true);  // goto next file
    }
    else if (newPES && m_interliaveBlockSize > 0 && m_canSwithBlock &&
             (blockFull() || (m_sublingMuxer && m_sublingMuxer->blockFull())))
    {
        finishFileBlock(avPacket.pts, newPCR, false);  // interleave SSIF here
    }
    else if (newPCR - m_lastPCR >= m_pcr_delta)
    {
        if (m_interliaveBlockSize == 0 || avPacket.stream_index == m_mainStreamIndex)
        {
            writePATPMT(newPCR);
            writePCR(newPCR);
        }
    }

    m_streamInfo[tsIndex].m_pts = avPacket.pts;
    m_streamInfo[tsIndex].m_dts = avPacket.dts;
    uint8_t pesStreamID = m_pesType[tsIndex];
    if (pesStreamID <= SYSTEM_START_CODE)
    {
        if (m_useNewStyleAudioPES)
            pesStreamID = PES_VC1_ID;
        else
            pesStreamID = PES_PRIVATE_DATA1;
    }

    addData(pesStreamID, tsIndex, avPacket);

    const auto mpegReader = dynamic_cast<MPEGStreamReader*>(avPacket.codec);
    if (avPacket.duration > 0)
        m_endStreamDTS = avPacket.dts + avPacket.duration;
    else if (mpegReader)
        m_endStreamDTS = avPacket.dts + static_cast<int64_t>(INTERNAL_PTS_FREQ / mpegReader->getFPS());
    else
        m_endStreamDTS = avPacket.dts;

    return true;
}

int TSMuxer::writeTSFrames(const int pid, const uint8_t* buffer, const int64_t len, const bool priorityData,
                           bool payloadStart)
{
    int result = 0;

    const uint8_t* curPos = buffer;
    const uint8_t* end = buffer + len;

    const bool tsPriority = priorityData;
    StreamInfo& streamInfo = m_streamInfo[pid];

    while (curPos < end)
    {
        if (m_cbrBitrate != -1 && m_lastPCR != -1)
        {
            const auto newPCR = llround(static_cast<double>(m_lastPCR + m_pcrBits) * 90000.0 / m_cbrBitrate);
            if (newPCR - m_lastPCR >= m_pcr_delta && m_lastPCR != -1)
            {
                m_pcrBits = 0;
                writePATPMT(newPCR);
                writePCR(newPCR);
                if (m_lastPESDTS != -1 && m_lastPCR > m_lastPESDTS)
                {
                    LTRACE(LT_ERROR, 2,
                           "VBV buffer overflow at position " << (double)(m_lastPCR - m_fixed_pcr_offset) / 90000.0
                                                              << " sec");
                }
            }
        }

        if (m_m2tsMode)
        {
            m_outBufLen += 4;
            m_processedBlockSize += 4;
            m_pcrBits += 4 * 8;
        }
        const int64_t tmpBufferLen = end - curPos;
        const auto initTS = reinterpret_cast<uint32_t*>(m_outBuf + m_outBufLen);
        *initTS = TSPacket::TS_FRAME_SYNC_BYTE + TSPacket::DATA_EXIST_BIT_VAL;
        const auto tsPacket = reinterpret_cast<TSPacket*>(m_outBuf + m_outBufLen);
        int64_t payloadLen = TS_FRAME_SIZE - tsPacket->getHeaderSize();
        tsPacket->setPID(pid);
        tsPacket->counter = streamInfo.m_tsCnt++;
        tsPacket->payloadStart = payloadStart;  // curPos == buffer;
        payloadStart = false;
        tsPacket->priority = tsPriority;
        if (tmpBufferLen < payloadLen)
        {
            // insert padding bytes
            if (!tsPacket->afExists)
            {
                tsPacket->afExists = 1;
                if (payloadLen - tmpBufferLen == 1)
                {
                    tsPacket->adaptiveField.length = 0;
                    payloadLen--;
                }
                else
                {
                    initTS[1] = 0x01;  // zero all af flags, set af len to 1.
                    payloadLen -= 2;
                }
            }
            memset(reinterpret_cast<uint8_t*>(tsPacket) + tsPacket->getHeaderSize(), 0xff, payloadLen - tmpBufferLen);
            tsPacket->adaptiveField.length += static_cast<unsigned>(payloadLen - tmpBufferLen);
            payloadLen = tmpBufferLen;
        }
        const int tsHeaderSize = tsPacket->getHeaderSize();
        memcpy(m_outBuf + m_outBufLen + tsHeaderSize, curPos, payloadLen);

        curPos += payloadLen;
        m_outBufLen += TS_FRAME_SIZE;
        m_processedBlockSize += TS_FRAME_SIZE;
        m_pcrBits += TS_FRAME_SIZE * 8;
        m_muxedPacketCnt[m_muxedPacketCnt.size() - 1]++;
        writeOutBuffer();
        result++;
    }
    return result;
}

void TSMuxer::buildNULL()
{
    *reinterpret_cast<uint32_t*>(m_nullBuffer) = TSPacket::TS_FRAME_SYNC_BYTE;
    const auto tsPacket = reinterpret_cast<TSPacket*>(m_nullBuffer);
    tsPacket->setPID(NULL_PID);
    tsPacket->dataExists = 1;
    memset(m_nullBuffer + TSPacket::TS_HEADER_SIZE, 0xff, TS_FRAME_SIZE - TSPacket::TS_HEADER_SIZE);
}

void TSMuxer::buildPAT()
{
    *reinterpret_cast<uint32_t*>(m_patBuffer) = TSPacket::TS_FRAME_SYNC_BYTE + TSPacket::DATA_EXIST_BIT_VAL;
    const auto tsPacket = reinterpret_cast<TSPacket*>(m_patBuffer);
    tsPacket->setPID(PAT_PID);
    tsPacket->dataExists = 1;
    tsPacket->payloadStart = 1;
    m_pat.pmtPids[SIT_PID] = 0;          // add NIT. program number = 0
    m_pat.pmtPids[DEFAULT_PMT_PID] = 1;  // add PMT. program number = 1
    m_pat.transport_stream_id = 0;       // version of transport stream
    const uint32_t size =
        m_pat.serialize(m_patBuffer + TSPacket::TS_HEADER_SIZE, TS_FRAME_SIZE - TSPacket::TS_HEADER_SIZE);
    memset(m_patBuffer + TSPacket::TS_HEADER_SIZE + size, 0xff, TS_FRAME_SIZE - TSPacket::TS_HEADER_SIZE - size);
}

void TSMuxer::buildPMT()
{
    *reinterpret_cast<uint32_t*>(m_pmtBuffer) = TSPacket::TS_FRAME_SYNC_BYTE + TSPacket::DATA_EXIST_BIT_VAL;
    const auto tsPacket = reinterpret_cast<TSPacket*>(m_pmtBuffer);
    tsPacket->setPID(DEFAULT_PMT_PID);
    tsPacket->dataExists = 1;
    tsPacket->payloadStart = 1;
    const uint32_t size =
        m_pmt.serialize(m_pmtBuffer + TSPacket::TS_HEADER_SIZE, 3864, m_bluRayMode, m_hdmvDescriptors);
    uint8_t* pmtEnd = m_pmtBuffer + TSPacket::TS_HEADER_SIZE + size;
    uint8_t* curPos = m_pmtBuffer + TS_FRAME_SIZE;
    for (; curPos < pmtEnd; curPos += TS_FRAME_SIZE)
    {
        memmove(curPos + 4, curPos, pmtEnd - curPos);
        pmtEnd += 4;
        *reinterpret_cast<uint32_t*>(curPos) = TSPacket::TS_FRAME_SYNC_BYTE + TSPacket::DATA_EXIST_BIT_VAL;
        const auto tsPacket2 = reinterpret_cast<TSPacket*>(curPos);
        tsPacket2->setPID(DEFAULT_PMT_PID);
        tsPacket2->dataExists = 1;
        tsPacket2->payloadStart = 0;
    }
    const int64_t oldPmtSize = pmtEnd - m_pmtBuffer;
    int64_t roundSize = pmtEnd - m_pmtBuffer;
    if (roundSize % TS_FRAME_SIZE != 0)
        roundSize = (roundSize / TS_FRAME_SIZE + 1) * TS_FRAME_SIZE;
    memset(pmtEnd, 0xff, roundSize - oldPmtSize);
    m_pmtFrames = roundSize / TS_FRAME_SIZE;
}

void TSMuxer::buildSIT() {}

void TSMuxer::writeNullPackets(const int cnt)
{
    for (int i = 0; i < cnt; i++)
    {
        if (m_m2tsMode)
        {
            m_outBufLen += 4;
            m_processedBlockSize += 4;
            m_pcrBits += 4 * 8;
        }
        memcpy(m_outBuf + m_outBufLen, m_nullBuffer, TS_FRAME_SIZE);
        const auto tsPacket = reinterpret_cast<TSPacket*>(m_outBuf + m_outBufLen);
        tsPacket->counter = m_nullCnt++;
        m_outBufLen += TS_FRAME_SIZE;
        m_processedBlockSize += TS_FRAME_SIZE;
        m_pcrBits += TS_FRAME_SIZE * 8;
        m_muxedPacketCnt[m_muxedPacketCnt.size() - 1]++;
        writeOutBuffer();
    }
}

bool TSMuxer::appendM2TSNullPacketToFile(const int64_t curFileSize, int counter, int* packetsWrited) const
{
    *packetsWrited = 0;
    while (counter < 0) counter += 16;
    uint8_t tmpBuff[192];

    File* file = dynamic_cast<File*>(m_muxFile);

    file->seek(curFileSize - 192, File::SeekMethod::smBegin);
    const int readCnt = file->read(tmpBuff, 192);
    if (readCnt != 192)
        return false;
    memcpy(tmpBuff + 4, m_nullBuffer, TS_FRAME_SIZE);
    const auto tsPacket = reinterpret_cast<TSPacket*>(tmpBuff);
    for (int64_t newFileSize = curFileSize; newFileSize % (2048LL * 3) != 0; newFileSize += 192)
    {
        tsPacket->counter = counter++;
        if (file->write(tmpBuff, 192) != 192)
            return false;
        // m_muxedPacketCnt[m_muxedPacketCnt.size()-1]++;
        (*packetsWrited)++;
    }
    return true;
}

void TSMuxer::writePAT()
{
    if (m_m2tsMode)
    {
        m_outBufLen += 4;
        m_processedBlockSize += 4;
        m_pcrBits += 4 * 8;
    }
    memcpy(m_outBuf + m_outBufLen, m_patBuffer, TS_FRAME_SIZE);
    const auto tsPacket = reinterpret_cast<TSPacket*>(m_outBuf + m_outBufLen);
    tsPacket->counter = m_patCnt++;
    m_outBufLen += TS_FRAME_SIZE;
    m_processedBlockSize += TS_FRAME_SIZE;
    m_pcrBits += TS_FRAME_SIZE * 8;
    m_muxedPacketCnt[m_muxedPacketCnt.size() - 1]++;
    writeOutBuffer();
}

void TSMuxer::writePMT()
{
    const uint8_t* curPos = m_pmtBuffer;
    for (int i = 0; i < m_pmtFrames; i++)
    {
        if (m_m2tsMode)
        {
            m_outBufLen += 4;
            m_processedBlockSize += 4;
            m_pcrBits += 4 * 8;
        }
        memcpy(m_outBuf + m_outBufLen, curPos, TS_FRAME_SIZE);
        const auto tsPacket = reinterpret_cast<TSPacket*>(m_outBuf + m_outBufLen);
        tsPacket->counter = m_pmtCnt++;
        m_outBufLen += TS_FRAME_SIZE;
        m_processedBlockSize += TS_FRAME_SIZE;
        m_pcrBits += TS_FRAME_SIZE * 8;
        m_muxedPacketCnt[m_muxedPacketCnt.size() - 1]++;
        writeOutBuffer();
        curPos += TS_FRAME_SIZE;
    }
}

void TSMuxer::writeSIT()
{
    if (m_m2tsMode)
    {
        m_outBufLen += 4;
        m_processedBlockSize += 4;
        m_pcrBits += 4 * 8;
    }
    memcpy(m_outBuf + m_outBufLen, DefaultSitTableOne, TS_FRAME_SIZE);
    const auto tsPacket = reinterpret_cast<TSPacket*>(m_outBuf + m_outBufLen);
    tsPacket->counter = m_sitCnt++;
    m_outBufLen += TS_FRAME_SIZE;
    m_processedBlockSize += TS_FRAME_SIZE;
    m_pcrBits += TS_FRAME_SIZE * 8;
    m_muxedPacketCnt[m_muxedPacketCnt.size() - 1]++;
    writeOutBuffer();
}

void TSMuxer::writeOutBuffer()
{
    if (m_outBufLen >= m_writeBlockSize)
    {
        int toFileLen = m_writeBlockSize & ~(MuxerManager::PHYSICAL_SECTOR_SIZE - 1);
        if (m_owner->isAsyncMode())
        {
            const auto newBuf = new uint8_t[m_writeBlockSize + 1024];
            memcpy(newBuf, m_outBuf + toFileLen, m_outBufLen - toFileLen);
            if (m_m2tsMode)
            {
                if (m_prevM2TSPCROffset >= toFileLen && m_m2tsDelayBlocks.empty())
                {
                    m_owner->asyncWriteBuffer(this, m_outBuf, toFileLen, m_muxFile);
                    m_prevM2TSPCROffset -= toFileLen;
                }
                else
                    m_m2tsDelayBlocks.emplace_back(m_outBuf, toFileLen);
            }
            else
                m_owner->asyncWriteBuffer(this, m_outBuf, toFileLen, m_muxFile);
            m_outBuf = newBuf;
        }
        else
        {
            if (!m_m2tsMode)
                m_owner->syncWriteBuffer(this, m_outBuf, toFileLen, m_muxFile);
            else
            {
                if (m_prevM2TSPCROffset >= toFileLen && m_m2tsDelayBlocks.empty())
                {
                    m_owner->syncWriteBuffer(this, m_outBuf, toFileLen, m_muxFile);
                    m_prevM2TSPCROffset -= toFileLen;
                }
                else
                {
                    auto newBuf = new uint8_t[toFileLen];
                    memcpy(newBuf, m_outBuf, toFileLen);
                    m_m2tsDelayBlocks.emplace_back(newBuf, toFileLen);
                }
            }
            memmove(m_outBuf, m_outBuf + toFileLen, m_outBufLen - toFileLen);
        }
        m_outBufLen -= toFileLen;
    }
}

void TSMuxer::parseMuxOpt(const std::string& opts)
{
    const vector<string> params = splitStr(opts.c_str(), ' ');
    for (auto& i : params)
    {
        vector<string> paramPair = splitStr(trimStr(i).c_str(), '=');
        if (paramPair.empty())
            continue;
        if (paramPair[0] == "--no-pcr-on-video-pid")
            setPCROnVideoPID(false);
        else if (paramPair[0] == "--new-audio-pes")
            setNewStyleAudioPES(true);
        else if (paramPair[0] == "--no-hdmv-descriptors")
            m_hdmvDescriptors = false;
        else if (paramPair[0] == "--bitrate" && paramPair.size() > 1)
        {
            setMaxBitrate(static_cast<int>(strToDouble(paramPair[1].c_str()) * 1000.0));
            setMinBitrate(static_cast<int>(strToDouble(paramPair[1].c_str()) * 1000.0));
        }
        else if (paramPair[0] == "--maxbitrate" && paramPair.size() > 1)
            setMaxBitrate(static_cast<int>(strToDouble(paramPair[1].c_str()) * 1000.0));
        else if (paramPair[0] == "--minbitrate" && paramPair.size() > 1)
            setMinBitrate(static_cast<int>(strToDouble(paramPair[1].c_str()) * 1000.0));
        else if (paramPair[0] == "--vbv-len" && paramPair.size() > 1)
            setVBVBufferLen(strToInt32(paramPair[1].c_str()));
        else if (paramPair[0] == "--split-duration")
        {
            setSplitDuration(strToInt64(paramPair[1].c_str()) * INTERNAL_PTS_FREQ);
            m_computeMuxStats = true;
        }
        else if (paramPair[0] == "--split-size")
        {
            int64_t coeff = 1;
            string postfix;
            for (const auto& j : paramPair[1])
                if (!((j >= '0' && j <= '9') || j == '.'))
                    postfix += j;
            postfix = strToUpperCase(postfix);
            if (postfix == "GB")
                coeff = 1000LL * 1000 * 1000;
            else if (postfix == "GIB")
                coeff = 1024LL * 1024 * 1024;
            else if (postfix == "MB")
                coeff = 1000LL * 1000;
            else if (postfix == "MIB")
                coeff = 1024LL * 1024;
            else if (postfix == "KB")
                coeff = 1000;
            else if (postfix == "KIB")
                coeff = 1024;
            string prefix = paramPair[1].substr(0, paramPair[1].size() - postfix.size());
            setSplitSize(static_cast<uint32_t>(strToDouble(prefix.c_str()) * static_cast<double>(coeff)));
            m_computeMuxStats = true;
        }
        else if (paramPair[0] == "--blu-ray" || paramPair[0] == "--blu-ray-v3" || paramPair[0] == "--avchd")
        {
            m_bluRayMode = true;
            m_computeMuxStats = true;
        }
        else if (paramPair[0] == "--cut-start" || paramPair[0] == "--cut-end")
        {
            m_computeMuxStats = true;
        }
    }
}

void TSMuxer::setSubMode(AbstractMuxer* mainMuxer, const bool flushInterleavedBlock)
{
    m_sublingMuxer = dynamic_cast<TSMuxer*>(mainMuxer);
    m_subMode = true;
    m_canSwithBlock = flushInterleavedBlock;
    m_interleaveInfo.rbegin()->push_back(0);
}

void TSMuxer::joinToMasterFile()
{
    m_isExternalFile = true;
    m_muxFile = m_sublingMuxer->getDstFile();
}

void TSMuxer::setMasterMode(AbstractMuxer* subMuxer, const bool flushInterleavedBlock)
{
    m_sublingMuxer = dynamic_cast<TSMuxer*>(subMuxer);
    m_masterMode = true;
    m_canSwithBlock = flushInterleavedBlock;
    m_interleaveInfo.rbegin()->push_back(0);
}

void TSMuxer::openDstFile()
{
    m_muxFile = m_fileFactory ? m_fileFactory->createFile() : new File();

    int systemFlags = 0;
#ifdef _WIN32
    if (m_owner->isAsyncMode())
        systemFlags += FILE_FLAG_NO_BUFFERING;
#endif
    if (!m_muxFile->open(m_outFileName.c_str(), File::ofWrite, systemFlags))
        THROW(ERR_CANT_CREATE_FILE, "Can't create file " << m_outFileName)
}

vector<int64_t> TSMuxer::getFirstPts() const
{
    std::vector<int64_t> rez;
    rez.reserve(m_firstPts.size());
    for (const auto& i : m_firstPts) rez.push_back(internalClockToPts(i) + m_timeOffset);
    return rez;
}

void TSMuxer::alignPTS(TSMuxer* otherMuxer)
{
    if (!m_lastPts.empty() && !otherMuxer->m_lastPts.empty())
    {
        const int64_t minPTS = FFMIN(*m_lastPts.rbegin(), *otherMuxer->m_lastPts.rbegin());
        *m_lastPts.rbegin() = minPTS;
        *otherMuxer->m_lastPts.rbegin() = minPTS;
    }
}

vector<int64_t> TSMuxer::getLastPts() const
{
    std::vector<int64_t> rez;
    rez.reserve(m_lastPts.size());
    for (const auto& i : m_lastPts) rez.push_back(internalClockToPts(i) + m_timeOffset);
    return rez;
}

void TSMuxer::setFileName(const std::string& fileName, FileFactory* fileFactory)
{
    m_curFileNum = 0;
    AbstractMuxer::setFileName(fileName, fileFactory);
    m_outFileName = getNextName(fileName);
    const string ext = strToUpperCase(extractFileExt(m_outFileName));
    setMuxFormat(ext);

    m_fileNames.clear();
    m_fileNames.push_back(m_outFileName);
}

std::string TSMuxer::getFileNameByIdx(const size_t idx)
{
    if (idx < m_fileNames.size())
        return m_fileNames[idx];
    assert(1);
    return {};
}

void TSMuxer::setMuxFormat(const std::string& format)
{
    m_m2tsMode = format == "M2TS" || format == "M2T" || format == "MTS" || format == "SSIF";
    m_writeBlockSize = m_m2tsMode ? DEFAULT_FILE_BLOCK_SIZE : TS188_ROUND_BLOCK_SIZE;
    m_outBuf = new uint8_t[m_writeBlockSize + 1024];
    m_frameSize = m_m2tsMode ? 192 : 188;
    if (m_m2tsMode)
        m_sectorSize = 1024 * 6;
}

bool TSMuxer::isInterleaveMode() const { return m_masterMode || m_subMode; }

std::vector<int32_t> TSMuxer::getInterleaveInfo(const size_t idx) const { return m_interleaveInfo[idx]; }
