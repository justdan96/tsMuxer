#include "singleFileMuxer.h"

#include <fs/directory.h>
#include <fs/textfile.h>

#include "abstractMuxer.h"
#include "ac3StreamReader.h"
#include "avCodecs.h"
#include "lpcmStreamReader.h"
#include "mpegAudioStreamReader.h"
#include "muxerManager.h"
#include "psgStreamReader.h"
#include "srtStreamReader.h"
#include "vodCoreException.h"

#ifndef win32
#include <stdio.h>
#endif

using namespace std;

std::string getNewName(const std::string& oldName, int cnt)
{
    if (strEndWith(oldName, ".wav"))
        return oldName.substr(0, oldName.size() - 4) + "." + int32ToStr(cnt) + ".wav";
    else
        return oldName + ".wav" + int32ToStr(cnt);
}

SingleFileMuxer::SingleFileMuxer(MuxerManager* owner) : AbstractMuxer(owner), m_lastIndex(-1) {}

SingleFileMuxer::~SingleFileMuxer()
{
    for (map<int, StreamInfo*>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
        delete itr->second;
}

void SingleFileMuxer::intAddStream(const std::string& streamName, const std::string& codecName, int streamIndex,
                                   const map<string, string>& params, AbstractStreamReader* codecReader)
{
    codecReader->setDemuxMode(true);
    uint8_t descrBuffer[188];
    int descriptorLen = codecReader->getTSDescriptor(descrBuffer, true, true);
    string fileExt = "track";
    if (codecName == "A_AC3")
        fileExt = ".ac3";
    else if (codecName == "A_AAC")
        fileExt = ".aac";
    else if (codecName == "A_MP3")
    {
        MpegAudioStreamReader* mp3Reader = (MpegAudioStreamReader*)codecReader;
        if (mp3Reader->getLayer() == 3)
            fileExt = ".mp3";
        else
            fileExt = ".mpa";
    }
    else if (codecName == "A_DTS")
        fileExt = ".dts";
    else if (codecName == "A_LPCM")
    {
        fileExt = ".wav";
        LPCMStreamReader* lpcmReader = (LPCMStreamReader*)codecReader;
        // lpcmReader->setDemuxMode(true);
    }
    else if (codecName == "A_AC3")
    {
        AC3StreamReader* ac3Reader = (AC3StreamReader*)codecReader;
        if (ac3Reader->isTrueHD() && !ac3Reader->getDownconvertToAC3())
            fileExt = ".trueHD";
        else if (ac3Reader->isEAC3())
            fileExt = ".ddp";
        else
            fileExt = ".ac3";
    }
    else if (codecName == "S_SUP")
    {
        fileExt = ".sup";
    }
    else if (codecName == "S_HDMV/PGS")
    {
        fileExt = ".sup";
        PGSStreamReader* pgsReader = (PGSStreamReader*)codecReader;
        // pgsReader->setDemuxMode(true);
    }
    else if (codecName == "S_TEXT/UTF8")
    {
        fileExt = ".sup";
        SRTStreamReader* srtReader = (SRTStreamReader*)codecReader;
        // srtReader->setDemuxMode(true);
    }
    else if (codecName[0] == 'V')
    {
        if (codecName == "V_MS/VFW/WVC1")
        {
            fileExt = ".vc1";
        }
        else if (codecName == "V_MPEG4/ISO/AVC")
        {
            fileExt = ".264";
        }
        else if (codecName == "V_MPEG4/ISO/MVC")
        {
            fileExt = ".mvc";
        }
        else if (codecName == "V_MPEGH/ISO/HEVC")
        {
            fileExt = ".hevc";
        }
        else
        {
            fileExt = ".mpv";
        }
    }

    /*
    if (codecName[0] == 'A') {
            // Used for "more correct" switching between the boundaries of Blu-Ray disc files
            ((SimplePacketizerReader*) codecReader)->setMPLSInfo(m_mplsParser.m_playItems);
    }
    */

    vector<string> fileList = extractFileList(streamName);
    string fileName;
    if (fileList.size() < 3)
        for (int i = 0; i < (int)fileList.size(); i++)
        {
            if (i > 0)
                fileName += '+';
            fileName += extractFileName(fileList[i]);
        }
    else
        fileName = extractFileName(fileList[0]) + "+___+" + extractFileName(fileList[fileList.size() - 1]);

    map<string, string>::const_iterator itr = params.find("track");
    if (itr != params.end())
    {
        if (params.find("subClip") != params.end())
            fileName += ".subclip_track_";
        else
            fileName += ".track_";
        fileName += itr->second;
    }

    int cnt = ++m_trackNameTmp[fileName];
    if (cnt > 1)
    {
        fileName += "_";
        fileName += int32ToStr(cnt);
    }
    StreamInfo* streamInfo = new StreamInfo((unsigned)DEFAULT_FILE_BLOCK_SIZE);
    streamInfo->m_fileName = fileName + fileExt;
    if (streamInfo->m_fileName.size() > 254)
        LTRACE(LT_ERROR, 2, "Error: File name too long.");
    streamInfo->m_codecReader = codecReader;
    m_streamInfo[streamIndex] = streamInfo;
}

void SingleFileMuxer::openDstFile()
{
    string dir = closeDirPath(toNativeSeparators(m_origFileName));
    // if (!createDir(dstFileName, true))
    //	THROW(ERR_CANT_CREATE_FILE, "Can't create output directory " << dstFileName);
    int systemFlags = 0;
#ifdef _WIN32
    if (m_owner->isAsyncMode())
        systemFlags += FILE_FLAG_NO_BUFFERING;
#endif
    for (map<int, StreamInfo*>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
    {
        itr->second->m_fileName = dir + itr->second->m_fileName;
        if (!itr->second->m_file.open(itr->second->m_fileName.c_str(), File::ofWrite, systemFlags))
            THROW(ERR_CANT_CREATE_FILE, "Can't create output file " << itr->second->m_fileName);
    }
}

void SingleFileMuxer::writeOutBuffer(StreamInfo* streamInfo)
{
    const uint32_t blockSize = DEFAULT_FILE_BLOCK_SIZE;
    if (streamInfo->m_bufLen >= blockSize)
    {
        int toFileLen = blockSize & 0xffff0000;
        if (m_owner->isAsyncMode())
        {
            uint8_t* newBuf = new uint8_t[blockSize + MAX_AV_PACKET_SIZE];
            memcpy(newBuf, streamInfo->m_buffer + toFileLen, streamInfo->m_bufLen - toFileLen);
            m_owner->asyncWriteBuffer(this, streamInfo->m_buffer, toFileLen, &streamInfo->m_file);
            streamInfo->m_buffer = newBuf;
        }
        else
        {
            m_owner->syncWriteBuffer(this, streamInfo->m_buffer, toFileLen, &streamInfo->m_file);
            memcpy(streamInfo->m_buffer, streamInfo->m_buffer + toFileLen, streamInfo->m_bufLen - toFileLen);
        }
        streamInfo->m_totalWrited += toFileLen;
        streamInfo->m_bufLen -= toFileLen;
    }

    LPCMStreamReader* lpcmReader = dynamic_cast<LPCMStreamReader*>(streamInfo->m_codecReader);
    if (lpcmReader && streamInfo->m_totalWrited >= 0xffff0000ul - blockSize)
    // if (lpcmReader && streamInfo->m_totalWrited >= 0x0ffffffful)
    {
        if (m_owner->isAsyncMode())
            m_owner->waitForWriting();
        streamInfo->m_file.close();
        streamInfo->m_file.open(streamInfo->m_fileName.c_str(), File::ofWrite + File::ofAppend);
        streamInfo->m_file.write(streamInfo->m_buffer, streamInfo->m_bufLen);
        streamInfo->m_file.close();
        streamInfo->m_file.open(streamInfo->m_fileName.c_str(), File::ofWrite + File::ofNoTruncate);
        lpcmReader->beforeFileCloseEvent(streamInfo->m_file);
        streamInfo->m_file.close();
        std::string newName = getNewName(streamInfo->m_fileName.c_str(), streamInfo->m_part);
        deleteFile(newName.c_str());
        if (rename(streamInfo->m_fileName.c_str(), newName.c_str()) != 0)
            THROW(ERR_COMMON, "Can't rename file " << streamInfo->m_fileName << " to " << newName);
        streamInfo->m_part++;
        int systemFlags = 0;
        streamInfo->m_bufLen = 0;
#ifdef _WIN32
        if (m_owner->isAsyncMode())
            systemFlags += FILE_FLAG_NO_BUFFERING;
#endif
        if (!streamInfo->m_file.open(streamInfo->m_fileName.c_str(), File::ofWrite + systemFlags))
            THROW(ERR_COMMON, "Can't open file " << streamInfo->m_fileName);
        lpcmReader->setFirstFrame(true);
        streamInfo->m_totalWrited = 0;
    }
}

bool SingleFileMuxer::muxPacket(AVPacket& avPacket)
{
    if (avPacket.data == 0 || avPacket.size == 0)
        return true;
    StreamInfo* streamInfo = m_streamInfo[avPacket.stream_index];
    if (avPacket.dts != streamInfo->m_dts || avPacket.pts != streamInfo->m_pts ||
        m_lastIndex != avPacket.stream_index || avPacket.flags & AVPacket::FORCE_NEW_FRAME)
    {
        const uint32_t blockSize = DEFAULT_FILE_BLOCK_SIZE;
        streamInfo->m_bufLen += avPacket.codec->writeAdditionData(
            streamInfo->m_buffer + streamInfo->m_bufLen,
            streamInfo->m_buffer + blockSize + MAX_AV_PACKET_SIZE + ADD_DATA_SIZE, avPacket, 0);
        writeOutBuffer(streamInfo);
    }
    m_lastIndex = avPacket.stream_index;
    streamInfo->m_dts = avPacket.dts;
    streamInfo->m_pts = avPacket.pts;
    memcpy(streamInfo->m_buffer + streamInfo->m_bufLen, avPacket.data, avPacket.size);
    streamInfo->m_bufLen += avPacket.size;
    writeOutBuffer(streamInfo);
    return true;
}

bool SingleFileMuxer::doFlush()
{
    for (map<int, StreamInfo*>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
    {
        StreamInfo* streamInfo = itr->second;
        unsigned lastBlockSize = streamInfo->m_bufLen & 0xffff;  // last 64K of data
        unsigned roundBufLen = streamInfo->m_bufLen & 0xffff0000;
        if (m_owner->isAsyncMode())
        {
            if (lastBlockSize > 0)
            {
                uint8_t* newBuff = new uint8_t[lastBlockSize];
                memcpy(newBuff, streamInfo->m_buffer + roundBufLen, lastBlockSize);
                m_owner->asyncWriteBuffer(this, streamInfo->m_buffer, roundBufLen, &streamInfo->m_file);
                streamInfo->m_buffer = newBuff;
            }
            else
            {
                m_owner->asyncWriteBuffer(this, streamInfo->m_buffer, roundBufLen, &streamInfo->m_file);
                streamInfo->m_buffer = 0;
            }
        }
        else
        {
            m_owner->syncWriteBuffer(this, streamInfo->m_buffer, roundBufLen, &streamInfo->m_file);
            memcpy(streamInfo->m_buffer, streamInfo->m_buffer + roundBufLen, lastBlockSize);
        }
        streamInfo->m_bufLen = lastBlockSize;
    }
    return true;
}

bool SingleFileMuxer::close()
{
    for (map<int, StreamInfo*>::iterator itr = m_streamInfo.begin(); itr != m_streamInfo.end(); ++itr)
    {
        StreamInfo* streamInfo = itr->second;
        if (!streamInfo->m_file.close())
            return false;
        if (streamInfo->m_bufLen > 0)
        {
            if (!streamInfo->m_file.open(streamInfo->m_fileName.c_str(), File::ofWrite + File::ofAppend))
                return false;
            if (!streamInfo->m_file.write(streamInfo->m_buffer, streamInfo->m_bufLen))
                return false;
            if (streamInfo->m_codecReader)
            {
                if (!streamInfo->m_file.close())
                    return false;
                if (streamInfo->m_file.open(streamInfo->m_fileName.c_str(), File::ofWrite + File::ofNoTruncate))
                    return false;
                if (!streamInfo->m_codecReader->beforeFileCloseEvent(streamInfo->m_file))
                    return false;
            }
            if (!streamInfo->m_file.close())
                return false;

            if (streamInfo->m_part > 1)
            {
                std::string newName = getNewName(streamInfo->m_fileName.c_str(), streamInfo->m_part);
                rename(streamInfo->m_fileName.c_str(), newName.c_str());
            }
        }
    }
    return true;
}

void SingleFileMuxer::parseMuxOpt(const std::string& opts)
{
    vector<string> params = splitStr(opts.c_str(), ' ');
    for (int i = 0; i < params.size(); i++)
    {
        vector<string> paramPair = splitStr(trimStr(params[i]).c_str(), '=');
        if (paramPair.size() == 0)
            continue;
        if (paramPair[0] == "--split-duration")
        {
            LTRACE(LT_WARN, 2,
                   "Warning! Splitting does not implemented for demux mode. Parameter " << paramPair[0] << " ignored.");
        }
        if (paramPair[0] == "--split-size")
        {
            LTRACE(LT_WARN, 2,
                   "Warning! Splitting does not implemented for demux mode. Parameter " << paramPair[0] << " ignored.");
        }
    }
}
