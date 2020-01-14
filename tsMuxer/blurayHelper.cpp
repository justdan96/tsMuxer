#include "blurayHelper.h"

#include <fs/directory.h>

#include "hevc.h"
#include "iso_writer.h"
#include "psgStreamReader.h"
#include "tsMuxer.h"
#include "tsPacket.h"

using namespace std;

uint8_t bdIndexData[] = {
    0x49, 0x4e, 0x44, 0x58, 0x30, 0x32, 0x30, 0x30, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x00, 0x00, 0x00,  // 0x78,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x01, 0x92, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x00, 0x00, 0x01, 0x7e, 0x49, 0x44, 0x45, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x62, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x36, 0x10, 0x13, 0x00, 0x01, 0x54, 0x52, 0x20, 0x30,
    0x2e, 0x30, 0x2e, 0x31, 0x2e, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xff, 0xff, 0x42, 0x20, 0x07, 0x08, 0x08, 0x00,
    0x54, 0x53, 0x00, 0x90, 0x0a, 0x54, 0x52, 0x20, 0x30, 0x2e, 0x30, 0x2e, 0x31, 0x2e, 0x38, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x18, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t bdMovieObjectData[] = {
    0x4D, 0x4F, 0x42, 0x4A, 0x30, 0x31, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x05,
    0x50, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x50, 0x40, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x40,  // here 5040
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0A, 0x21, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x09, 0x50, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x03, 0x50, 0x40, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0xFF, 0xFF, 0x48, 0x40, 0x03, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00,
    0xFF, 0xFF, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x04, 0x50, 0x40, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x48, 0x40, 0x03, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x21, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x21, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x80, 0x00, 0x00, 0x05, 0x50, 0x40, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x50, 0x40, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x50, 0x40, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x00, 0xFF, 0xFF, 0x50, 0x40, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x21, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ------------------------- BlurayHelper ---------------------------

BlurayHelper::BlurayHelper() : m_isoWriter(0) {}

BlurayHelper::~BlurayHelper() { close(); }

void BlurayHelper::close()
{
    if (m_isoWriter)
    {
        LTRACE(LT_INFO, 2, "Finalize ISO disk");
        delete m_isoWriter;
        m_isoWriter = 0;
    }
}

bool BlurayHelper::open(const string& dst, DiskType dt, int64_t diskSize, int extraISOBlocks)
{
    m_dstPath = toNativeSeparators(dst);

    m_dt = dt;
    string fileExt = extractFileExt(m_dstPath);
    fileExt = unquoteStr(strToUpperCase(fileExt));
    if (fileExt == "ISO")
    {
        m_isoWriter = new IsoWriter();
        m_isoWriter->setLayerBreakPoint(0xBA7200);  // around 25Gb
        return m_isoWriter->open(m_dstPath, diskSize, extraISOBlocks);
    }
    else
    {
        m_dstPath = closeDirPath(m_dstPath, getDirSeparator());
        return true;
    }
}

bool BlurayHelper::createBluRayDirs()
{
    if (m_dt == DT_BLURAY)
    {
        if (m_isoWriter)
        {
            m_isoWriter->createDir("BDMV/META");
            m_isoWriter->createDir("BDMV/BDJO");
            m_isoWriter->createDir("BDMV/JAR");
            m_isoWriter->createDir("BDMV/BACKUP/BDJO");
        }
        else
        {
            createDir(m_dstPath + toNativeSeparators("BDMV/META"), true);
            createDir(m_dstPath + toNativeSeparators("BDMV/BDJO"), true);
            createDir(m_dstPath + toNativeSeparators("BDMV/JAR"), true);
            createDir(m_dstPath + toNativeSeparators("BDMV/BACKUP/BDJO"), true);
        }
    }

    if (m_isoWriter)
    {
        m_isoWriter->createDir("CERTIFICATE/BACKUP");
        m_isoWriter->createDir("BDMV/AUXDATA");

        m_isoWriter->createDir("BDMV/CLIPINF");
        m_isoWriter->createDir("BDMV/PLAYLIST");
        m_isoWriter->createDir("BDMV/STREAM");
        m_isoWriter->createDir("BDMV/BACKUP/CLIPINF");
        m_isoWriter->createDir("BDMV/BACKUP/PLAYLIST");
    }
    else
    {
        createDir(m_dstPath + toNativeSeparators("CERTIFICATE/BACKUP"), true);
        createDir(m_dstPath + toNativeSeparators("BDMV/AUXDATA"), true);

        createDir(m_dstPath + toNativeSeparators("BDMV/CLIPINF"), true);
        createDir(m_dstPath + toNativeSeparators("BDMV/PLAYLIST"), true);
        createDir(m_dstPath + toNativeSeparators("BDMV/STREAM"), true);
        createDir(m_dstPath + toNativeSeparators("BDMV/BACKUP/CLIPINF"), true);
        createDir(m_dstPath + toNativeSeparators("BDMV/BACKUP/PLAYLIST"), true);
    }

    return true;
}

bool BlurayHelper::writeBluRayFiles(bool usedBlackPL, int mplsNum, int blankNum, bool stereoMode)
{
    int fileSize = sizeof(bdIndexData);
    string prefix = m_isoWriter ? "" : m_dstPath;
    AbstractOutputStream* file;
    if (m_isoWriter)
        file = m_isoWriter->createFile();
    else
        file = new File();

    if (!file->open((prefix + "BDMV/index.bdmv").c_str(), AbstractOutputStream::ofWrite))
    {
        delete file;
        return false;
    }
    uint8_t* emptyCommand;
    if (m_dt == DT_BLURAY)
    {
        if (V3_flags)
        {
            bdMovieObjectData[5] = bdIndexData[5] = '3';  // V3
            fileSize = 0x9C;                              // add 36 bytes for UHD data extension
            bdIndexData[15] = 0x78;                       // start address of UHD data extension
            emptyCommand = bdIndexData + 0x78;
            // UHD data extension
            memcpy(emptyCommand,
                   "\x00\x00\x00\x20\x00\x00\x00\x18\x00\x00\x00\x01"
                   "\x00\x03\x00\x01\x00\x00\x00\x18\x00\x00\x00\x0C"
                   "\x00\x00\x00\x08\x20\x00\x00\x00\x00\x00\x00\x00",
                   36);
            // 4K flag => 66/100 GB Disk, 109 MB/s Recording_Rate
            if (V3_flags & 0x20)
                bdIndexData[0x94] = 0x51;
            // no HDR10 detected => SDR flag
            if (!(V3_flags & 0x1e))
                V3_flags |= 1;
            // include HDR/SDR flags
            bdIndexData[0x96] = (V3_flags & 0x1f);
        }
        else
        {  // V2
            bdMovieObjectData[5] = bdIndexData[5] = '2';
            fileSize = 0x78;
        }
    }
    else
    {
        bdMovieObjectData[5] = bdIndexData[5] = '1';
        bdIndexData[15] = 0x78;
    }
    bdIndexData[0x2c] = stereoMode ? 0x60 : 0;  // set initial_output_mode_preference and SS_content_exist_flag

    emptyCommand = bdMovieObjectData + 78;
    if (usedBlackPL)
    {
        memcpy(emptyCommand, "\x42\x82\x00\x00", 4);
        uint32_t blackPlNum = my_htonl(blankNum);
        memcpy(emptyCommand + 4, &blackPlNum, 4);
        memcpy(emptyCommand + 8, "x00\x00\x00\x0a", 4);
    }
    else
        memcpy(emptyCommand, "\x50\x40\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00", 12);
    uint32_t* mplsOff = (uint32_t*)(bdMovieObjectData + 0x5e);
    *mplsOff = my_htonl(mplsNum);
    file->write(bdIndexData, fileSize);
    file->close();

    if (!file->open((prefix + "BDMV/MovieObject.bdmv").c_str(), File::ofWrite))
    {
        delete file;
        return false;
    }

    int moLen = sizeof(bdMovieObjectData);
    file->write(bdMovieObjectData, moLen);
    file->close();

    if (!file->open((prefix + "BDMV/BACKUP/index.bdmv").c_str(), File::ofWrite))
    {
        delete file;
        return false;
    }
    file->write(bdIndexData, fileSize);
    file->close();

    if (!file->open((prefix + "BDMV/BACKUP/MovieObject.bdmv").c_str(), File::ofWrite))
    {
        delete file;
        return false;
    }
    file->write(bdMovieObjectData, moLen);
    file->close();

    delete file;
    return true;
}

bool BlurayHelper::createCLPIFile(TSMuxer* muxer, int clpiNum, bool doLog)
{
    static const int CLPI_BUFFER_SIZE = 1024 * 1024;
    uint8_t* clpiBuffer = new uint8_t[CLPI_BUFFER_SIZE];
    CLPIParser clpiParser;
    string version_number;
    if (m_dt == DT_BLURAY)
        memcpy(&clpiParser.version_number, V3_flags ? "0300" : "0200", 5);
    else
        memcpy(&clpiParser.version_number, "0100", 5);
    clpiParser.clip_stream_type = 1;  // AV stream
    clpiParser.isDependStream = muxer->isSubStream();
    if (clpiParser.isDependStream)
        clpiParser.application_type = 8;  // Sub TS for a sub-path of MPEG-4 MVC Dependent view
    else
        clpiParser.application_type = 1;  // 1 - Main TS for a main-path of Movie
    clpiParser.is_ATC_delta = false;
    if (doLog)
    {
        if (m_dt == DT_BLURAY)
        {
            LTRACE(LT_INFO, 2, "Creating Blu-ray stream info and seek index");
        }
        else
        {
            LTRACE(LT_INFO, 2, "Creating AVCHD stream info and seek index");
        }
    }

    PIDListMap pidList = muxer->getPidList();
    for (PIDListMap::const_iterator itr = pidList.begin(); itr != pidList.end(); ++itr)
    {
        CLPIStreamInfo streamInfo(itr->second);
        clpiParser.m_streamInfo.insert(make_pair(streamInfo.streamPID, streamInfo));
    }
    vector<int64_t> packetCount = muxer->getMuxedPacketCnt();
    vector<int64_t> firstPts = muxer->getFirstPts();
    vector<int64_t> lastPts = muxer->getLastPts();

    for (unsigned i = 0; i < muxer->splitFileCnt(); i++)
    {
        if (muxer->isInterleaveMode())
            clpiParser.interleaveInfo = muxer->getInterleaveInfo(i);

        double avDuration = (lastPts[i] - firstPts[i]) / 90000.0;
        // clpiParser.TS_recording_rate = (packetCount[i] * 192) / avDuration; // avarage bitrate

        if (muxer->isSubStream())
            clpiParser.TS_recording_rate = MAX_SUBMUXER_RATE / 8;
        else
            clpiParser.TS_recording_rate = MAX_MAIN_MUXER_RATE / 8;
        // max rate is 109 mbps for 4K
        if (V3_flags & 0x20)
            clpiParser.TS_recording_rate = (clpiParser.TS_recording_rate * 109) / 48;
        clpiParser.number_of_source_packets = packetCount[i];
        clpiParser.presentation_start_time = firstPts[i] / 2;
        clpiParser.presentation_end_time = lastPts[i] / 2;
        clpiParser.m_clpiNum = i;

        int fileLen = clpiParser.compose(clpiBuffer, CLPI_BUFFER_SIZE);

        string prefix = m_isoWriter ? "" : m_dstPath;
        AbstractOutputStream* file;
        if (m_isoWriter)
            file = m_isoWriter->createFile();
        else
            file = new File();

        string dstDir = string("BDMV") + getDirSeparator() + string("CLIPINF") + getDirSeparator();
        string clipName = extractFileName(muxer->getFileNameByIdx(i));
        if (!file->open((prefix + dstDir + clipName + string(".clpi")).c_str(), File::ofWrite))
        {
            delete[] clpiBuffer;
            delete file;
            return false;
        }
        if (file->write(clpiBuffer, fileLen) != fileLen)
        {
            file->close();
            delete[] clpiBuffer;
            delete file;
            return false;
        }
        file->close();

        dstDir = string("BDMV") + getDirSeparator() + string("BACKUP") + getDirSeparator() + string("CLIPINF") +
                 getDirSeparator();
        if (!file->open((prefix + dstDir + clipName + string(".clpi")).c_str(), File::ofWrite))
        {
            delete[] clpiBuffer;
            delete file;
            return false;
        }
        if (file->write(clpiBuffer, fileLen) != fileLen)
        {
            file->close();
            delete[] clpiBuffer;
            delete file;
            return false;
        }
        file->close();
        delete file;
    }
    delete[] clpiBuffer;
    return true;
}

const PMTStreamInfo* streamByIndex(int index, const PIDListMap& pidList)
{
    for (PIDListMap::const_iterator itr = pidList.begin(); itr != pidList.end(); ++itr)
    {
        const PMTStreamInfo& stream = itr->second;
        if (stream.m_codecReader->getStreamIndex() == index)
            return &stream;
    }
    return 0;
}

bool BlurayHelper::createMPLSFile(TSMuxer* mainMuxer, TSMuxer* subMuxer, int autoChapterLen,
                                  vector<double> customChapters, DiskType dt, int mplsOffset, bool isMvcBaseViewR)
{
    uint32_t firstPts = *(mainMuxer->getFirstPts().begin());
    uint32_t lastPts = *(mainMuxer->getLastPts().rbegin());

    uint8_t mplsBuffer[1024 * 100];
    MPLSParser mplsParser;
    mplsParser.m_m2tsOffset = mainMuxer->getFirstFileNum();
    mplsParser.PlayList_playback_type = 1;
    mplsParser.ref_to_STC_id = 0;
    mplsParser.IN_time = (uint32_t)(firstPts / 2);
    mplsParser.OUT_time = (uint32_t)(lastPts / 2);
    mplsParser.mvc_base_view_r = isMvcBaseViewR;

    if (customChapters.size() == 0)
    {
        mplsParser.m_chapterLen = autoChapterLen;
    }
    else
    {
        for (int i = 0; i < customChapters.size(); i++)
        {
            int64_t mark = customChapters[i] * 45000.0;
            if (mark >= 0 && mark <= (mplsParser.OUT_time - mplsParser.IN_time))
                mplsParser.m_marks.push_back(PlayListMark(-1, mark + mplsParser.IN_time));
        }
    }
    mplsParser.PlayItem_random_access_flag = false;
    PIDListMap pidList = mainMuxer->getPidList();
    PIDListMap pidListMVC;
    if (subMuxer)
        pidListMVC = subMuxer->getPidList();

    for (PIDListMap::const_iterator itr = pidList.begin(); itr != pidList.end(); ++itr)
    {
        mplsParser.m_streamInfo.push_back(MPLSStreamInfo(itr->second));
        MPLSStreamInfo& info = *(mplsParser.m_streamInfo.rbegin());
        PGSStreamReader* pgStream = dynamic_cast<PGSStreamReader*>(itr->second.m_codecReader);
        if (pgStream)
        {
            info.offsetId = pgStream->getOffsetId();
            info.SS_PG_offset_sequence_id = pgStream->ssPGOffset;
            info.isSSPG = pgStream->isSSPG;
            if (info.isSSPG)
            {
                bool isSecondary = false;
                const PMTStreamInfo* left = streamByIndex(pgStream->leftEyeSubStreamIdx, pidList);
                if (!left)
                {
                    left = streamByIndex(pgStream->leftEyeSubStreamIdx, pidListMVC);
                    isSecondary = true;
                }
                if (left)
                {
                    info.leftEye = new MPLSStreamInfo(*left);
                    if (isSecondary)
                        info.leftEye->type = 2;  // ref to subclip
                }

                isSecondary = false;
                const PMTStreamInfo* right = streamByIndex(pgStream->rightEyeSubStreamIdx, pidList);
                if (!right)
                {
                    right = streamByIndex(pgStream->rightEyeSubStreamIdx, pidListMVC);
                    isSecondary = true;
                }
                if (right)
                {
                    info.rightEye = new MPLSStreamInfo(*right);
                    if (isSecondary)
                        info.rightEye->type = 2;  // ref to subclip
                }
            }
        }
    }
    if (subMuxer)
    {
        mplsParser.isDependStreamExist = true;
        for (PIDListMap::const_iterator itr = pidListMVC.begin(); itr != pidListMVC.end(); ++itr)
        {
            MPLSStreamInfo data(itr->second);
            data.type = 2;  // Identify an elementary stream of the Clip used by a SubPath with SubPath_type set to
                            // 2,3,4,5,6,8 or 9
            mplsParser.m_streamInfoMVC.push_back(data);
        }
    }

    if (dt == DT_BLURAY)
    {
        LTRACE(LT_INFO, 2, "Creating Blu-ray playlist");
    }
    else
    {
        LTRACE(LT_INFO, 2, "Creating AVCHD playlist");
    }
    int fileLen = mplsParser.compose(mplsBuffer, sizeof(mplsBuffer), dt);

    string prefix = m_isoWriter ? "" : m_dstPath;
    AbstractOutputStream* file;
    if (m_isoWriter)
        file = m_isoWriter->createFile();
    else
        file = new File();

    string dstDir = string("BDMV") + getDirSeparator() + string("PLAYLIST") + getDirSeparator();
    if (!file->open((prefix + dstDir + strPadLeft(int32ToStr(mplsOffset), 5, '0') + string(".mpls")).c_str(),
                    File::ofWrite))
        return false;
    if (file->write(mplsBuffer, fileLen) != fileLen)
    {
        delete file;
        return false;
    }
    file->close();

    dstDir = string("BDMV") + getDirSeparator() + string("BACKUP") + getDirSeparator() + string("PLAYLIST") +
             getDirSeparator();
    if (!file->open((prefix + dstDir + strPadLeft(int32ToStr(mplsOffset), 5, '0') + string(".mpls")).c_str(),
                    File::ofWrite))
    {
        delete file;
        return false;
    }
    if (file->write(mplsBuffer, fileLen) != fileLen)
    {
        delete file;
        return false;
    }
    file->close();
    delete file;
    return true;
}

string BlurayHelper::m2tsFileName(int num)
{
    string prefix = m_isoWriter ? "" : m_dstPath;
    char separator = m_isoWriter ? '/' : getDirSeparator();
    return prefix + string("BDMV") + separator + string("STREAM") + separator + strPadLeft(int32ToStr(num), 5, '0') +
           string(".m2ts");
}

string BlurayHelper::ssifFileName(int num)
{
    string prefix = m_isoWriter ? "" : m_dstPath;
    char separator = m_isoWriter ? '/' : getDirSeparator();
    return prefix + string("BDMV") + separator + string("STREAM") + separator + string("SSIF") + separator +
           strPadLeft(int32ToStr(num), 5, '0') + string(".ssif");
}

IsoWriter* BlurayHelper::isoWriter() const { return m_isoWriter; }

AbstractOutputStream* BlurayHelper::createFile()
{
    if (m_isoWriter)
        return m_isoWriter->createFile();
    else
        return new File();
}

bool BlurayHelper::isVirtualFS() const { return m_isoWriter != 0; }

void BlurayHelper::setVolumeLabel(const std::string& label)
{
    if (m_isoWriter)
        m_isoWriter->setVolumeLabel(unquoteStr(label));
}
