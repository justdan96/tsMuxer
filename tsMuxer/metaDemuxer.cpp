

#include "metaDemuxer.h"

#include <fs/directory.h>
#include <fs/textfile.h>
#include <types/types.h>

#include "aacStreamReader.h"
#include "ac3StreamReader.h"
#include "bufferedReaderManager.h"
#include "combinedH264Demuxer.h"
#include "dtsStreamReader.h"
#include "dvbSubStreamReader.h"
#include "h264StreamReader.h"
#include "hevcStreamReader.h"
#include "limits.h"
#include "lpcmStreamReader.h"
#include "math.h"
#include "matroskaDemuxer.h"
#include "movDemuxer.h"
#include "mpeg2StreamReader.h"
#include "mpegAudioStreamReader.h"
#include "mpegStreamReader.h"
#include "programStreamDemuxer.h"
#include "psgStreamReader.h"
#include "srtStreamReader.h"
#include "subTrackFilter.h"
#include "tsDemuxer.h"
#include "utf8Converter.h"
#include "vc1StreamReader.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace std;

const static int MAX_DEMUX_BUFFER_SIZE = 1024 * 1024 * 192;
const static int MIN_READED_BLOCK = 16384;

METADemuxer::METADemuxer(const BufferedReaderManager& readManager)
    : m_containerReader(*this, readManager), m_readManager(readManager)
{
    m_flushDataMode = false;
    m_HevcFound = false;
    m_totalSize = 0;
    m_lastProgressY = 0;
    m_lastReadRez = 0;
}

METADemuxer::~METADemuxer()
{
    readClose();
    for (unsigned i = 0; i < m_iterators.size(); i++) delete m_iterators[i];
}

uint64_t METADemuxer::getDemuxedSize()
{
    uint64_t rez = 0;
    for (unsigned i = 0; i < m_codecInfo.size(); i++)
        rez += m_codecInfo[i].m_streamReader->getProcessedSize();  // m_codecInfo[i].m_dataProcessed;
    return rez + m_containerReader.getDiscardedSize();
}

int METADemuxer::readPacket(AVPacket& avPacket)

{
    avPacket.stream_index = 0;
    avPacket.data = 0;
    avPacket.size = 0;
    avPacket.codec = 0;
    m_lastReadRez = 0;
    while (1)
    {
        int minDtsIndex = -1;
        int64_t minDts = LLONG_MAX;
        bool allDataDelayed = true;
        while (allDataDelayed)
        {
            for (unsigned i = 0; i < m_codecInfo.size(); i++)
            {
                StreamInfo& streamInfo = m_codecInfo[i];
                if (!m_flushDataMode)
                {
                    streamInfo.lastReadRez = streamInfo.read();
                    if (streamInfo.lastReadRez == BufferedFileReader::DATA_DELAYED)
                        continue;  // skip stream
                    allDataDelayed = false;
                    if (streamInfo.lastReadRez == BufferedFileReader::DATA_NOT_READY)
                    {
                        m_lastReadRez = BufferedFileReader::DATA_NOT_READY;
                        return BufferedFileReader::DATA_NOT_READY;
                    }
                    if (streamInfo.lastReadRez != BufferedFileReader::DATA_EOF2)
                    {
                        if (streamInfo.m_lastDTS < minDts)
                        {
                            minDtsIndex = i;
                            minDts = streamInfo.m_lastDTS;
                        }
                    }
                    else if (streamInfo.m_lastDTS < minDts && !streamInfo.m_flushed)
                    {
                        minDtsIndex = i;
                        minDts = streamInfo.m_lastDTS;
                    }
                }
                else
                {
                    allDataDelayed = false;
                    if (streamInfo.m_lastDTS < minDts && !streamInfo.m_flushed)
                    {
                        minDtsIndex = i;
                        minDts = streamInfo.m_lastDTS;
                    }
                }
            }
            if (allDataDelayed)
                for (unsigned i = 0; i < m_codecInfo.size(); i++)
                {
                    ContainerToReaderWrapper* cReader =
                        dynamic_cast<ContainerToReaderWrapper*>(m_codecInfo[i].m_dataReader);
                    if (cReader)
                        cReader->resetDelayedMark();
                }
        }
        if (minDtsIndex != -1)
        {
            if (!m_flushDataMode)
            {
                if (m_codecInfo[minDtsIndex].lastReadRez != BufferedFileReader::DATA_EOF2)
                {
                    int res = m_codecInfo[minDtsIndex].m_streamReader->readPacket(avPacket);
                    m_codecInfo[minDtsIndex].m_lastAVRez = res;
                }
                else
                {
                    // flush single stream
                    m_codecInfo[minDtsIndex].m_streamReader->flushPacket(avPacket);
                    m_codecInfo[minDtsIndex].m_flushed = true;
                }
                // add time shift from external sync source
                // add static time shift
                avPacket.dts += m_codecInfo[minDtsIndex].m_timeShift;
                avPacket.pts += m_codecInfo[minDtsIndex].m_timeShift;
                m_codecInfo[minDtsIndex].m_lastDTS = avPacket.dts + avPacket.duration;
            }
            else
            {  // flush all streams
                m_codecInfo[minDtsIndex].m_streamReader->flushPacket(avPacket);
                m_codecInfo[minDtsIndex].m_flushed = true;
            }
            updateReport(true);
            return 0;
        }
        else
        {
            if (!m_flushDataMode)
                m_flushDataMode = true;
            else
            {
                updateReport(false);
                m_lastReadRez = BufferedFileReader::DATA_EOF;
                return BufferedReader::DATA_EOF;
            }
        }
    }
}

void METADemuxer::openFile(const string& streamName)
{
    m_streamName = streamName;
    readClose();

    TextFile file(m_streamName.c_str(), File::ofRead);
    string str;
    file.readLine(str);
    while (str.length() > 0)
    {
        str = trimStr(str);
        if (str.length() == 0 || str[0] == '#')
        {
            file.readLine(str);
            continue;
        }
        if (strStartWith(str, "MUXOPT"))
        {
            file.readLine(str);
            continue;
        }
        vector<string> params = splitQuotedStr(str.c_str(), ',');
        if (params.size() < 2)
            THROW(ERR_INVALID_CODEC_FORMAT, "Invalid codec format: " << str);
        map<string, string> addParams;
        for (unsigned i = 2; i < params.size(); i++)
        {
            // params[i] = strToLowerCase ( params[i] );
            vector<string> tmp = splitStr(params[i].c_str(), '=');
            addParams.insert(std::make_pair(trimStr(tmp[0]), trimStr(tmp.size() > 1 ? tmp[1] : "")));
        }
        string codec = trimStr(params[0]);
        string codecStreamName = trimStr(params[1]);
        codec = strToUpperCase(codec);
        if (!m_HevcFound)
            m_HevcFound = (codec.find("HEVC") == 12);
        addStream(codec, codecStreamName, addParams);
        file.readLine(str);
    }

    H264StreamReader::SeiMethod primarySEI = H264StreamReader::SEI_NotDefined;
    for (int i = 0; i < m_codecInfo.size(); ++i)
    {
        H264StreamReader* reader = dynamic_cast<H264StreamReader*>(m_codecInfo[i].m_streamReader);
        if (reader && !reader->isSubStream())
        {
            primarySEI = reader->getInsertSEI();
            break;
        }
    }

    bool warned = false;
    if (primarySEI != H264StreamReader::SEI_NotDefined)
    {
        for (int i = 0; i < m_codecInfo.size(); ++i)
        {
            H264StreamReader* reader = dynamic_cast<H264StreamReader*>(m_codecInfo[i].m_streamReader);
            if (reader && reader->isSubStream())
            {
                if (!warned && (int)reader->getInsertSEI() != primarySEI)
                {
                    LTRACE(LT_INFO, 2,
                           "Parameter 'insertSEI' for MVC dependent view differs from same param for AVC base view. "
                           "Ignore and apply same value");
                    warned = true;
                }
                reader->setInsertSEI(primarySEI);
            }
        }
    }
}

std::string METADemuxer::mplsTrackToFullName(const std::string& mplsFileName, std::string& mplsNum)
{
    string path = toNativeSeparators(extractFilePath(mplsFileName));
    int tmp = path.find_last_of(getDirSeparator());
    if (tmp == string::npos)
        return string();
    path = path.substr(0, tmp + 1) + string("STREAM") + getDirSeparator();

    string mplsExt = strToLowerCase(extractFileExt(mplsFileName));
    string m2tsExt;
    if (mplsExt == "mpls")
        m2tsExt = "m2ts";
    else
        m2tsExt = "mts";

    return path + mplsNum + string(".") + m2tsExt;
}

std::string METADemuxer::mplsTrackToSSIFName(const std::string& mplsFileName, std::string& mplsNum)
{
    string path = toNativeSeparators(extractFilePath(mplsFileName));
    int tmp = path.find_last_of(getDirSeparator());
    if (tmp == string::npos)
        return string();
    path = path.substr(0, tmp + 1) + string("STREAM") + getDirSeparator() + string("SSIF") + getDirSeparator();

    string mplsExt = strToLowerCase(extractFileExt(mplsFileName));
    string ssifExt;
    if (mplsExt == "mpls")
        ssifExt = "ssif";
    else
        ssifExt = "sif";

    return path + mplsNum + string(".") + ssifExt;
}

int METADemuxer::addPGSubStream(const string& codec, const string& _codecStreamName,
                                const map<string, string>& addParams, MPLSStreamInfo* subStream)
{
    map<string, string> params = addParams;
    params["track"] = int32ToStr(subStream->streamPID);
    if (subStream->type == 2)
        params["subClip"] = "1";
    return addStream(codec, _codecStreamName, params);
}

std::vector<MPLSPlayItem> METADemuxer::mergePlayItems(const std::vector<MPLSParser>& mplsInfoList)
{
    std::vector<MPLSPlayItem> result;
    for (int i = 0; i < mplsInfoList.size(); ++i)
    {
        const MPLSParser& mplsInfo = mplsInfoList[i];
        for (int j = 0; j < mplsInfo.m_playItems.size(); ++j)
        {
            result.push_back(mplsInfo.m_playItems[j]);
        }
    }
    return result;
}

int METADemuxer::addStream(const string codec, const string& codecStreamName, const map<string, string>& addParams)
{
    uint32_t pid = 0;
    map<string, string>::const_iterator tmpitr = addParams.find("track");
    if (tmpitr != addParams.end())
        pid = strToInt32(tmpitr->second.c_str());

    tmpitr = addParams.find("subTrack");
    if (tmpitr != addParams.end())
        pid = SubTrackFilter::pidToSubPid(pid, strToInt32(tmpitr->second));

    vector<string> fileList;
    string unquotedStreamName = unquoteStr(codecStreamName);
    string fileExt = strToLowerCase(extractFileExt(codecStreamName));
    std::vector<MPLSParser> mplsInfoList;

    // filds for SS PG
    int leftEyeSubStreamIdx = -1;
    int rightEyeSubStreamIdx = -1;
    bool isSSPG = false;
    int ssPGOffset = 0xff;
    ////

    bool isSubStream = (codec == h264DepCodecInfo.programName) || addParams.find("subClip") != addParams.end();
    if (fileExt == "mpls" || fileExt == "mpl")
    {
        mplsInfoList = getMplsInfo(codecStreamName);
        vector<string> mplsNames = splitQuotedStr(codecStreamName.c_str(), '+');
        for (int k = 0; k < mplsInfoList.size(); ++k)
        {
            MPLSParser& mplsInfo = mplsInfoList[k];
            string unquotedStreamName = unquoteStr(mplsNames[k]);
            for (int i = 0; i < mplsInfo.m_playItems.size(); ++i)
            {
                string playItemName;
                if (isSubStream)
                {
                    if (mplsInfo.m_mvcFiles.size() == 0)
                    {
                        THROW(ERR_INVALID_CODEC_FORMAT,
                              "Current playlist file doesn't has MVC track info. Please, remove MVC track from the "
                              "track list");
                    }
                    else if (mplsInfo.m_mvcFiles.size() <= i)
                        THROW(ERR_INVALID_CODEC_FORMAT,
                              "Bad playlist file: number of CLPI files for AVC and VMC parts do not match");
                    playItemName = mplsInfo.m_mvcFiles[i];
                }
                else
                {
                    playItemName = mplsInfo.m_playItems[i].fileName;
                }
                string fileName = mplsTrackToFullName(unquotedStreamName, playItemName);
                if (mplsInfo.isDependStreamExist && !fileExists(fileName))
                    fileName = mplsTrackToSSIFName(unquotedStreamName, mplsInfo.m_playItems[i].fileName);
                fileList.push_back(fileName);
            }

            MPLSStreamInfo streamInfo = mplsInfo.getStreamByPID(pid);
            if (streamInfo.stream_coding_type == 0x90 && streamInfo.isSSPG)
            {
                // add refs to addition tracks for stereo subtitles
                leftEyeSubStreamIdx = addPGSubStream(codec, unquotedStreamName, addParams, streamInfo.leftEye);
                rightEyeSubStreamIdx = addPGSubStream(codec, unquotedStreamName, addParams, streamInfo.rightEye);

                isSSPG = streamInfo.isSSPG;
                ssPGOffset = streamInfo.SS_PG_offset_sequence_id;
            }
        }
    }
    else
    {
        fileList = extractFileList(codecStreamName);
    }

    for (int i = fileList.size() - 1; i >= 0; --i)
    {
        string trackKey = fileList[i] + string("_#") + int32ToStr(i) + string("_");
        if (addParams.find("subClip") == addParams.end())
            trackKey += string("_track_");
        else
            trackKey += string("_subtrack_");
        trackKey += int32ToStr(pid);
        if (m_processedTracks.find(trackKey) != m_processedTracks.end())
            fileList.erase(fileList.begin() + i);
        else
            m_processedTracks.insert(trackKey);
    }
    if (fileList.empty())
        return -1;

    FileListIterator* listIterator = 0;
    if (fileList.size() > 1)
    {
        listIterator = new FileListIterator();
        m_iterators.push_back(listIterator);
        for (unsigned i = 0; i < fileList.size(); i++) listIterator->addFile(fileList[i]);
    }

    uint64_t fileSize = 0;

    if (m_containerReader.m_demuxers.find(fileList[0]) == m_containerReader.m_demuxers.end())
    {
        File tmpFile;
        for (unsigned i = 0; i < fileList.size(); i++)
        {
            if (!tmpFile.open(fileList[i].c_str(), File::ofRead))
                THROW(ERR_INVALID_CODEC_FORMAT, "Can't open file: " << fileList[i].c_str());
            uint64_t tmpSize = 0;
            tmpFile.size(&tmpSize);
            fileSize += tmpSize;
            tmpFile.close();
        }
    }

    AbstractStreamReader* codecReader = createCodec(codec, addParams, fileList[0], mergePlayItems(mplsInfoList));
    codecReader->setStreamIndex(m_codecInfo.size() + 1);
    codecReader->setTimeOffset(m_timeOffset);

    if (m_codecInfo.size() == 0)
        codecReader->m_flags |= AVPacket::PCR_STREAM;

    AbstractReader* dataReader;
    string tmpname = strToLowerCase(fileList[0]);
    tmpname = unquoteStr(trimStr(tmpname));

    if (strEndWith(tmpname, ".h264") || strEndWith(tmpname, ".264") || strEndWith(tmpname, ".mvc"))
    {
        if (pid)
        {
            dataReader = &m_containerReader;
            codecReader->setSrcContainerType(AbstractStreamReader::ctMultiH264);
            if (listIterator)
                ((ContainerToReaderWrapper*)dataReader)->setFileIterator(fileList[0].c_str(), listIterator);
        }
        else
        {
            dataReader = (const_cast<BufferedReaderManager&>(m_readManager)).getReader(fileList[0].c_str());
        }
    }
    else if (strEndWith(tmpname, ".ts") || strEndWith(tmpname, ".m2ts") || strEndWith(tmpname, ".mts") ||
             strEndWith(tmpname, ".ssif"))
    {
        if (pid)
            dataReader = &m_containerReader;
        else
            THROW(ERR_INVALID_CODEC_FORMAT, "For streams inside TS/M2TS container need track parameter.");
        if (listIterator)
            ((ContainerToReaderWrapper*)dataReader)->setFileIterator(fileList[0].c_str(), listIterator);
        if (strEndWith(tmpname, ".ts"))
            codecReader->setSrcContainerType(AbstractStreamReader::ctTS);
        else
            codecReader->setSrcContainerType(AbstractStreamReader::ctM2TS);
    }
    else if (strEndWith(tmpname, ".vob") || strEndWith(tmpname, ".evo") || strEndWith(tmpname, ".mpg"))
    {
        if (pid)
            dataReader = &m_containerReader;
        else
            THROW(ERR_INVALID_CODEC_FORMAT, "For streams inside MPG/VOB/EVO container need track parameter.");
        if (listIterator)
            ((ContainerToReaderWrapper*)dataReader)->setFileIterator(fileList[0].c_str(), listIterator);
        if (strEndWith(tmpname, ".evo") || strEndWith(tmpname, ".evo\""))
            codecReader->setSrcContainerType(AbstractStreamReader::ctEVOB);
        else
            codecReader->setSrcContainerType(AbstractStreamReader::ctVOB);
    }
    else if (strEndWith(tmpname, ".mkv") || strEndWith(tmpname, ".mka"))
    {
        if (pid)
            dataReader = &m_containerReader;
        else
            THROW(ERR_INVALID_CODEC_FORMAT, "For streams inside MKV container need track parameter.");
        if (listIterator)
            ((ContainerToReaderWrapper*)dataReader)->setFileIterator(fileList[0].c_str(), listIterator);
        codecReader->setSrcContainerType(AbstractStreamReader::ctMKV);
    }
    else if (strEndWith(tmpname, ".mov") || strEndWith(tmpname, ".mp4") || strEndWith(tmpname, ".m4v") ||
             strEndWith(tmpname, ".m4a"))
    {
        if (pid)
            dataReader = &m_containerReader;
        else
            THROW(ERR_INVALID_CODEC_FORMAT, "For streams inside MOV/MP4 container need track parameter.");
        if (listIterator)
            ((ContainerToReaderWrapper*)dataReader)->setFileIterator(fileList[0].c_str(), listIterator);
        codecReader->setSrcContainerType(AbstractStreamReader::ctMOV);
    }
    else
    {
        dataReader = (const_cast<BufferedReaderManager&>(m_readManager)).getReader(fileList[0].c_str());
    }
    if (dataReader == 0)
    {
        delete codecReader;
        THROW(ERR_INVALID_CODEC_FORMAT, "This version do not support multicast or other network steams for muxing");
    }

    m_codecInfo.push_back(StreamInfo(dataReader, codecReader, fileList[0], codecStreamName, pid, isSubStream));
    if (listIterator)
    {
        BufferedFileReader* fileReader = dynamic_cast<BufferedFileReader*>(dataReader);
        if (fileReader)
            fileReader->setFileIterator(listIterator, m_codecInfo.rbegin()->m_readerID);
    }

    StreamInfo& streamInfo = *m_codecInfo.rbegin();
    streamInfo.m_codec = codec;
    streamInfo.m_addParams = addParams;

    // fields for SS PG stream
    PGSStreamReader* pgStream = dynamic_cast<PGSStreamReader*>(codecReader);
    if (pgStream)
    {
        pgStream->isSSPG = isSSPG;
        pgStream->ssPGOffset = ssPGOffset;
        pgStream->leftEyeSubStreamIdx = leftEyeSubStreamIdx;
        pgStream->rightEyeSubStreamIdx = rightEyeSubStreamIdx;
    }
    //

    map<string, string>::const_iterator itr = addParams.find("timeshift");
    if (itr != addParams.end())
    {
        string timeShift = itr->second;
        int pos = -1;
        uint64_t coeff = 1;
        int64_t value = 0;
        if ((pos = timeShift.find("ms")) != std::string::npos)
        {
            coeff = 1000000ull;
            value = strToInt32(timeShift.substr(0, pos).c_str());
        }
        else if ((pos = timeShift.find("s")) != std::string::npos)
        {
            coeff = 1000000000ull;
            value = strToInt32(timeShift.substr(0, pos).c_str());
        }
        else if ((pos = timeShift.find("ns")) != std::string::npos)
        {
            coeff = 1ull;
            value = strToInt32(timeShift.substr(0, pos).c_str());
        }
        else
        {
            coeff = 1000000ull;
            value = strToInt32(timeShift.c_str());
        }
        streamInfo.m_timeShift = value * coeff;
        if (value * coeff > 0)
            streamInfo.m_lastDTS = value * coeff;
    }

    itr = addParams.find("lang");
    if (itr != addParams.end())
        streamInfo.m_lang = itr->second;
    m_totalSize += fileSize;

    return codecReader->getStreamIndex();
}

void METADemuxer::readClose()
{
    for (auto& codecInfo : m_codecInfo)
    {
        codecInfo.m_dataReader->deleteReader(codecInfo.m_readerID);
        delete codecInfo.m_streamReader;
    }
    m_codecInfo.clear();
}

DetectStreamRez METADemuxer::DetectStreamReader(BufferedReaderManager& readManager, const string& fileName,
                                                bool calcDuration)
{
    AVChapters chapters;
    int64_t fileDuration = 0;
    vector<CheckStreamRez> streams, Vstreams;
    AbstractDemuxer* demuxer = 0;
    auto unquoted = unquoteStr(fileName);
    string fileExt = strToLowerCase(extractFileExt(unquoted));
    AbstractStreamReader::ContainerType containerType = AbstractStreamReader::ctNone;
    CLPIParser clpi;
    bool clpiParsed = false;
    if (fileExt == "m2ts" || fileExt == "mts" || fileExt == "ssif")
    {
        demuxer = new TSDemuxer(readManager, "");
        containerType = AbstractStreamReader::ctM2TS;
        string clpiFileName = findBluRayFile(extractFileDir(unquoted), "CLIPINF", extractFileName(unquoted) + ".clpi");
        if (!clpiFileName.empty())
            clpiParsed = clpi.parse(clpiFileName.c_str());
    }
    else if (fileExt == "ts")
    {
        demuxer = new TSDemuxer(readManager, "");
        containerType = AbstractStreamReader::ctTS;
    }
    else if (fileExt == "vob" || fileExt == "mpg")
    {
        demuxer = new ProgramStreamDemuxer(readManager);
        containerType = AbstractStreamReader::ctVOB;
    }
    else if (fileExt == "evo")
    {
        demuxer = new ProgramStreamDemuxer(readManager);
        containerType = AbstractStreamReader::ctEVOB;
    }
    else if (fileExt == "mkv" || fileExt == "mka")
    {
        demuxer = new MatroskaDemuxer(readManager);
        containerType = AbstractStreamReader::ctMKV;
    }
    else if (fileExt == "mp4" || fileExt == "m4v" || fileExt == "m4a" || fileExt == "mov")
    {
        demuxer = new MovDemuxer(readManager);
        containerType = AbstractStreamReader::ctMOV;
    }

    if (demuxer)
    {
        int fileBlockSize = demuxer->getFileBlockSize();

        demuxer->openFile(fileName);
        int64_t discardedSize = 0;
        DemuxedData demuxedData;
        map<uint32_t, TrackInfo> acceptedPidMap;
        demuxer->getTrackList(acceptedPidMap);
        PIDSet acceptedPidSet;
        for (map<uint32_t, TrackInfo>::const_iterator itr = acceptedPidMap.begin(); itr != acceptedPidMap.end(); ++itr)
            acceptedPidSet.insert(itr->first);
        for (DemuxedData::iterator itr = demuxedData.begin(); itr != demuxedData.end(); ++itr)
        {
            StreamData& vect = itr->second;
            vect.reserve(fileBlockSize);
        }

        for (int i = 0; i < DETECT_STREAM_BUFFER_SIZE / fileBlockSize; i++)
            demuxer->simpleDemuxBlock(demuxedData, acceptedPidSet, discardedSize);

        for (map<uint32_t, StreamData>::iterator itr = demuxedData.begin(); itr != demuxedData.end(); ++itr)
        {
            StreamData& vect = itr->second;
            CheckStreamRez trackRez = detectTrackReader(vect.data(), vect.size(), containerType,
                                                        acceptedPidMap[itr->first].m_trackType, itr->first);
            if (trackRez.codecInfo.programName.size() > 0)
            {
                if (trackRez.codecInfo.programName[0] != 'S')
                    trackRez.delay = demuxer->getTrackDelay(itr->first);
            }
            trackRez.trackID = itr->first;
            trackRez.lang = acceptedPidMap[trackRez.trackID].m_lang;
            if (clpiParsed)
            {
                map<int, CLPIStreamInfo>::const_iterator clpiStream = clpi.m_streamInfo.find(itr->first);
                if (clpiStream != clpi.m_streamInfo.end() && trackRez.lang.size() == 0)
                    trackRez.lang = clpiStream->second.language_code;
            }

            if (strStartWith(trackRez.codecInfo.programName, "A_") && dynamic_cast<TSDemuxer*>(demuxer))
            {
                if (trackRez.trackID >= 0x1A00)
                    trackRez.isSecondary = true;
            }

            if (strStartWith(trackRez.codecInfo.programName, "V_"))
                addTrack(Vstreams, trackRez);
            else
                addTrack(streams, trackRez);
        }
        chapters = demuxer->getChapters();
        if (calcDuration)
            fileDuration = demuxer->getFileDurationNano();
        delete demuxer;
    }
    else
    {
        File file;
        containerType = AbstractStreamReader::ctNone;
        if (!file.open(fileName.c_str(), File::ofRead))
            return DetectStreamRez();
        uint8_t* tmpBuffer = new uint8_t[DETECT_STREAM_BUFFER_SIZE];
        int len = file.read(tmpBuffer, DETECT_STREAM_BUFFER_SIZE);
        if (fileExt == "sup")
            containerType = AbstractStreamReader::ctSUP;
        else if (fileExt == "pcm" || fileExt == "lpcm" || fileExt == "wav" || fileExt == "w64")
            containerType = AbstractStreamReader::ctLPCM;
        else if (fileExt == "srt")
            containerType = AbstractStreamReader::ctSRT;
        CheckStreamRez trackRez = detectTrackReader(tmpBuffer, len, containerType, 0, 0);

        if (strStartWith(trackRez.codecInfo.programName, "V_"))
            addTrack(Vstreams, trackRez);
        else
            addTrack(streams, trackRez);

        delete[] tmpBuffer;
    }
    Vstreams.insert(Vstreams.end(), streams.begin(), streams.end());

    DetectStreamRez rez;
    rez.chapters = chapters;
    rez.fileDurationNano = fileDuration;
    rez.streams = Vstreams;
    return rez;
}

void METADemuxer::addTrack(vector<CheckStreamRez>& rez, CheckStreamRez trackRez)
{
    if (trackRez.codecInfo.codecID == h264DepCodecInfo.codecID && trackRez.multiSubStream)
    {
        // split combined MVC/AVC track to substreams
        rez.push_back(trackRez);

        trackRez.codecInfo = h264CodecInfo;
        int postfixPos = trackRez.streamDescr.find("3d-pg");
        if (postfixPos != string::npos)
            trackRez.streamDescr = trackRez.streamDescr.substr(0, postfixPos);

        rez.push_back(trackRez);
    }
    else
        rez.push_back(trackRez);
}

CheckStreamRez METADemuxer::detectTrackReader(uint8_t* tmpBuffer, int len,
                                              AbstractStreamReader::ContainerType containerType, int containerDataType,
                                              int containerStreamIndex)
{
    CheckStreamRez rez;

    PGSStreamReader pgsReader;
    rez = pgsReader.checkStream(tmpBuffer, len, containerType, containerDataType, containerStreamIndex);
    if (rez.codecInfo.codecID)
        return rez;

    SRTStreamReader srtReader;
    rez = srtReader.checkStream(tmpBuffer, len, containerType, containerDataType, containerStreamIndex);
    if (rez.codecInfo.codecID)
        return rez;

    if (len == 0)
        return rez;

    LPCMStreamReader lpcmReader;
    rez = lpcmReader.checkStream(tmpBuffer, len, containerType, containerDataType, containerStreamIndex);
    if (rez.codecInfo.codecID)
        return rez;

    H264StreamReader h264codec;
    rez = h264codec.checkStream(tmpBuffer, len);
    if (rez.codecInfo.codecID)
        return rez;

    DTSStreamReader dtscodec;
    rez = dtscodec.checkStream(tmpBuffer, len, containerType, containerDataType, containerStreamIndex);
    if (rez.codecInfo.codecID)
        return rez;

    AC3StreamReader ac3codec;
    rez = ac3codec.checkStream(tmpBuffer, len, containerType, containerDataType, containerStreamIndex);
    if (rez.codecInfo.codecID)
        return rez;

    AACStreamReader aaccodec;
    rez = aaccodec.checkStream(tmpBuffer, len, containerType, containerDataType, containerStreamIndex);
    if (rez.codecInfo.codecID)
        return rez;

    VC1StreamReader vc1ccodec;
    rez = vc1ccodec.checkStream(tmpBuffer, len);
    if (rez.codecInfo.codecID)
        return rez;

    HEVCStreamReader hevcCodec;
    rez = hevcCodec.checkStream(tmpBuffer, len);
    if (rez.codecInfo.codecID)
        return rez;

    MPEG2StreamReader mpeg2ccodec;
    rez = mpeg2ccodec.checkStream(tmpBuffer, len);
    if (rez.codecInfo.codecID)
        return rez;

    MpegAudioStreamReader mpegAudioCodec;
    rez = mpegAudioCodec.checkStream(tmpBuffer, len, containerType, containerDataType, containerStreamIndex);
    if (rez.codecInfo.codecID)
        return rez;

    DVBSubStreamReader supReader;
    rez = supReader.checkStream(tmpBuffer, len, containerType, containerDataType, containerStreamIndex);
    if (rez.codecInfo.codecID)
        return rez;

    return rez;
}

VideoAspectRatio arNameToCode(const string& arName)
{
    if (arName == "9x16" || arName == "16x9" || arName == "9:16" || arName == "16:9")
        return AR_16_9;
    else if (arName == "3:4" || arName == "4:3" || arName == "4x3" || arName == "3x4")
        return AR_3_4;
    else if (arName == "Square" || arName == "VGA" || arName == "1:1" || arName == "1x1" || arName == "1:1 (Square)")
        return AR_VGA;
    else if (arName == "WIDE" || arName == "1x2,21" || arName == "1x2.21" || arName == "1:2.21" || arName == "1:2,21" ||
             arName == "2.21:1" || arName == "2,21:1" || arName == "2.21x1" || arName == "2,21x1")
        return AR_221_100;
    else
        return AR_KEEP_DEFAULT;
}

double correctFps(double fps)
{
    if (fabs(fps - 23.976) < 1e-4)
        return 23.97602397602397;
    else if (fabs(fps - 29.97) < 1e-4)
        return 29.97002997002997;
    else if (fabs(fps - 59.94) < 1e-4)
        return 59.94005994005994;
    else
        return fps;
}

PIPParams::PipCorner pipCornerFromStr(const std::string& value)
{
    std::string v = trimStr(strToLowerCase(value));
    if (v == "topleft")
        return PIPParams::TopLeft;
    else if (v == "topright")
        return PIPParams::TopRight;
    else if (v == "bottomright")
        return PIPParams::BottomRight;
    else
        return PIPParams::BottomLeft;
}

int pipScaleFromStr(const std::string& value)
{
    std::string v = trimStr(strToLowerCase(value));
    if (v == "1")
        return 1;
    else if (v == "1/2" || v == "0.5")
        return 2;
    else if (v == "1/4" || v == "0.25")
        return 3;
    else if (v == "1.5")
        return 4;
    else if (v == "fullscreen")
        return 5;
    else
        return 1;  // default
}

AbstractStreamReader* METADemuxer::createCodec(const string& codecName, const map<string, string>& addParams,
                                               const std::string& codecStreamName, const vector<MPLSPlayItem>& mplsInfo)
{
    AbstractStreamReader* rez = 0;
    if (codecName == "V_MPEG4/ISO/AVC" || codecName == "V_MPEG4/ISO/MVC")
    {
        H264StreamReader* h264Reader = new H264StreamReader();
        rez = h264Reader;
        if (codecName == "V_MPEG4/ISO/MVC")
            h264Reader->setIsSubStream(true);

        map<string, string>::const_iterator itr = addParams.find("fps");
        if (itr != addParams.end())
        {
            double fps = strToDouble(itr->second.c_str());
            fps = correctFps(fps);
            ((H264StreamReader*)rez)->setFPS(fps);
        }

        itr = addParams.find("delPulldown");
        if (itr != addParams.end())
            ((H264StreamReader*)rez)->setRemovePulldown(true);

        itr = addParams.find("level");
        if (itr != addParams.end())
            ((H264StreamReader*)rez)->setForceLevel(strToDouble(itr->second.c_str()) * 10);
        itr = addParams.find("insertSEI");
        if (itr != addParams.end())
        {
            ((H264StreamReader*)rez)->setInsertSEI(H264StreamReader::SEI_InsertAuto);
        }
        itr = addParams.find("autoSEI");
        if (itr != addParams.end())
        {
            ((H264StreamReader*)rez)->setInsertSEI(H264StreamReader::SEI_InsertAuto);
        }
        itr = addParams.find("forceSEI");
        if (itr != addParams.end())
        {
            ((H264StreamReader*)rez)->setInsertSEI(H264StreamReader::SEI_InsertForce);
        }
        itr = addParams.find("contSPS");
        if (itr != addParams.end())
        {
            ((H264StreamReader*)rez)->setH264SPSCont(true);
        }
    }
    else if (codecName == "V_MPEGH/ISO/HEVC")
    {
        rez = new HEVCStreamReader();
        map<string, string>::const_iterator itr = addParams.find("fps");
        if (itr != addParams.end())
        {
            double fps = strToDouble(itr->second.c_str());
            fps = correctFps(fps);
            ((HEVCStreamReader*)rez)->setFPS(fps);
        }
    }
    else if (codecName == "V_MS/VFW/WVC1")
    {
        rez = new VC1StreamReader();
        map<string, string>::const_iterator itr = addParams.find("fps");
        if (itr != addParams.end())
        {
            double fps = strToDouble(itr->second.c_str());
            fps = correctFps(fps);
            ((VC1StreamReader*)rez)->setFPS(fps);
        }

        itr = addParams.find("delPulldown");
        if (itr != addParams.end())
            ((VC1StreamReader*)rez)->setRemovePulldown(true);
    }
    else if (codecName == "V_MPEG-2")
    {
        rez = new MPEG2StreamReader();
        map<string, string>::const_iterator itr = addParams.find("fps");
        if (itr != addParams.end())
        {
            double fps = strToDouble(itr->second.c_str());
            fps = correctFps(fps);
            ((MPEG2StreamReader*)rez)->setFPS(fps);
        }

        itr = addParams.find("ar");
        if (itr != addParams.end())
            ((MPEGStreamReader*)rez)->setAspectRatio(arNameToCode(itr->second));

        itr = addParams.find("delPulldown");
        if (itr != addParams.end())
            ((MPEG2StreamReader*)rez)->setRemovePulldown(true);
    }
    else if (codecName == "A_AAC")
        rez = new AACStreamReader();
    else if (codecName == "A_MP3")
        rez = new MpegAudioStreamReader();
    else if (codecName == "S_SUP")
        rez = new DVBSubStreamReader();
    else if (codecName == "A_LPCM")
    {
        rez = new LPCMStreamReader();
    }
    else if (codecName == "A_DTS")
    {
        rez = new DTSStreamReader();
        map<string, string>::const_iterator itr = addParams.find("down-to-dts");
        if (itr != addParams.end())
            ((DTSStreamReader*)rez)->setDownconvertToDTS(true);
    }
    else if (codecName == "A_AC3")
    {
        rez = new AC3StreamReader();
        map<string, string>::const_iterator itr = addParams.find("down-to-ac3");
        if (itr != addParams.end())
            ((AC3StreamReader*)rez)->setDownconvertToAC3(true);
    }
    else if (codecName == "S_HDMV/PGS")
    {
        rez = new PGSStreamReader();
        map<string, string>::const_iterator itr = addParams.find("bottom-offset");
        if (itr != addParams.end())
            ((PGSStreamReader*)rez)->setBottomOffset(strToInt32(itr->second.c_str()));

        itr = addParams.find("3d-plane");
        if (itr != addParams.end())
            ((PGSStreamReader*)rez)->setOffsetId(strToInt32(itr->second.c_str()));

        double fps = 0.0;
        int width = 0;
        int height = 0;
        itr = addParams.find("fps");
        if (itr != addParams.end())
            fps = strToDouble(itr->second.c_str());
        itr = addParams.find("video-width");
        if (itr != addParams.end())
            width = strToInt32(itr->second.c_str());
        itr = addParams.find("video-height");
        if (itr != addParams.end())
            height = strToInt32(itr->second.c_str());
        ((PGSStreamReader*)rez)->setVideoInfo(width, height, fps);
        itr = addParams.find("font-border");
        if (itr != addParams.end())
            ((PGSStreamReader*)rez)->setFontBorder(strToInt32(itr->second.c_str()));

        /*
        map<string,string>::const_iterator itr = addParams.find("fps");
        if (itr != addParams.end())
                ((PGSStreamReader*) rez)->setFPS(strToDouble(itr->second.c_str()));
        itr = addParams.find("video-width");
        if (itr != addParams.end())
                ((PGSStreamReader*) rez)->setVideoWidth(strToInt32(itr->second.c_str()));
        itr = addParams.find("video-height");
        if (itr != addParams.end())
                ((PGSStreamReader*) rez)->setVideoHeight(strToInt32(itr->second.c_str()));
        */
    }
    else if (codecName == "S_TEXT/UTF8")
    {
        SRTStreamReader* srtReader = new SRTStreamReader();
        rez = srtReader;
        text_subtitles::Font font;
        int srtWidth = 0, srtHeight = 0;
        double fps = 0.0;
        text_subtitles::TextAnimation animation;
        for (map<string, string>::const_iterator itr = addParams.begin(); itr != addParams.end(); ++itr)
        {
            if (itr->first == "font-name")
            {
                font.m_name = unquoteStr(itr->second);
            }
            else if (itr->first == "font-size")
                font.m_size = strToInt32(itr->second.c_str());
            else if (itr->first == "font-bold")
                font.m_opts |= font.BOLD;
            else if (itr->first == "font-italic")
                font.m_opts |= font.ITALIC;
            else if (itr->first == "font-underline")
                font.m_opts |= font.UNDERLINE;
            else if (itr->first == "font-strike-out")
                font.m_opts |= font.STRIKE_OUT;
            else if (itr->first == "font-color")
            {
                const string& s = itr->second;
                if (s.size() >= 2 && s[0] == '0' && s[1] == 'x')
                    font.m_color = strToInt32u(s.substr(2, s.size() - 2).c_str(), 16);
                else if (s.size() >= 1 && s[0] == 'x')
                    font.m_color = strToInt32u(s.substr(1, s.size() - 1).c_str(), 16);
                else
                    font.m_color = strToInt32u(s.c_str());
                if ((font.m_color & 0xff000000u) == 0)
                    font.m_color |= 0xff000000u;
            }
            else if (itr->first == "line-spacing")
                font.m_lineSpacing = strToDouble(itr->second.c_str());
            else if (itr->first == "font-charset")
                font.m_charset = strToInt32(itr->second.c_str());
            else if (itr->first == "font-border")
                font.m_borderWidth = strToInt32(itr->second.c_str());
            else if (itr->first == "fps")
            {
                fps = strToDouble(itr->second.c_str());
            }
            else if (itr->first == "video-width")
                srtWidth = strToInt32(itr->second.c_str());
            else if (itr->first == "video-height")
                srtHeight = strToInt32(itr->second.c_str());
            else if (itr->first == "bottom-offset")
                srtReader->setBottomOffset(strToInt32(itr->second.c_str()));
            else if (itr->first == "fadein-time")
                animation.fadeInDuration = strToDouble(itr->second.c_str());
            else if (itr->first == "fadeout-time")
                animation.fadeOutDuration = strToDouble(itr->second.c_str());
        }
        if (srtWidth == 0 || srtHeight == 0 || fps == 0)
            THROW(ERR_COMMON, "video-width, video-height and fps parameters MUST be provided for SRT tracks");
        srtReader->setVideoInfo(srtWidth, srtHeight, fps);
        srtReader->setFont(font);
        srtReader->setAnimation(animation);
    }
    else
        THROW(ERR_UNKNOWN_CODEC, "Unsupported codec " << codecName);

    if (codecName[0] == 'A')
    {
        map<string, string>::const_iterator itr = addParams.find("stretch");
        if (itr != addParams.end())
        {
            double stretch;
            int divPos = itr->second.find('/');
            if (divPos == std::string::npos)
                stretch = strToDouble(itr->second.c_str());
            else
            {
                string firstStr = itr->second.substr(0, divPos);
                string secondStr = itr->second.substr(divPos + 1, itr->second.size() - divPos - 1);
                double dFirst = strToDouble(firstStr.c_str());
                double dSecond = strToDouble(secondStr.c_str());
                if (dSecond != 0)
                    stretch = dFirst / dSecond;
                else
                    THROW(ERR_COMMON, "Second argument at stretch parameter can not be 0");
            }
            if (itr != addParams.end())
                ((SimplePacketizerReader*)rez)->setStretch(stretch);
        }
    }
    map<string, string>::const_iterator itr = addParams.find("secondary");
    if (itr != addParams.end())
        rez->setIsSecondary(true);

    PIPParams pipParams;
    itr = addParams.find("pipCorner");
    if (itr != addParams.end())
        pipParams.corner = pipCornerFromStr(itr->second);
    itr = addParams.find("pipHOffset");
    if (itr != addParams.end())
        pipParams.hOffset = strToInt32(itr->second);
    itr = addParams.find("pipVOffset");
    if (itr != addParams.end())
        pipParams.vOffset = strToInt32(itr->second);
    itr = addParams.find("pipLumma");
    if (itr != addParams.end())
        pipParams.lumma = strToInt32(itr->second);
    itr = addParams.find("pipScale");
    if (itr != addParams.end())
        pipParams.scaleIndex = pipScaleFromStr(itr->second);
    rez->setPipParams(pipParams);

    if (!mplsInfo.empty() && dynamic_cast<SimplePacketizerReader*>(rez))
        ((SimplePacketizerReader*)rez)->setMPLSInfo(mplsInfo);

    return rez;
}

const std::vector<MPLSParser> METADemuxer::getMplsInfo(const string& mplsFileName)
{
    std::vector<MPLSParser> result;

    std::vector<std::string> mplsFiles = splitQuotedStr(mplsFileName.c_str(), '+');
    for (int i = 0; i < mplsFiles.size(); ++i)
    {
        MPLSCache::iterator itr = m_mplsStreamMap.find(mplsFiles[i]);
        if (itr != m_mplsStreamMap.end())
            result.push_back(itr->second);
        else
        {
            MPLSParser parser;
            if (!parser.parse(unquoteStr(mplsFiles[i]).c_str()))
                THROW(ERR_COMMON, "Can't parse play list file " << mplsFiles[i]);
            pair<MPLSCache::iterator, bool> insRez = m_mplsStreamMap.insert(make_pair(mplsFiles[i], parser));
            m_mplsStreamMap.insert(make_pair(mplsFiles[i], insRez.first->second));
            result.push_back(insRez.first->second);
        }
    }
    return result;
}

string METADemuxer::findBluRayFile(const string& streamDir, const string& requestDir, const string& requestFile)
{
    string dirName = streamDir.substr(0, streamDir.size() - 1);
    if (strEndWith(strToLowerCase(dirName), "ssif"))
    {
        int pos = dirName.find_last_of(getDirSeparator());
        if (pos > 0)
            dirName = streamDir.substr(0, pos);
    }

    int tmp = dirName.find_last_of(getDirSeparator());
    if (tmp != std::string::npos)
    {
        dirName = streamDir.substr(0, tmp + 1);
        string fileName = dirName + requestDir + getDirSeparator() + requestFile;
        if (!fileExists(fileName))
        {
            fileName = dirName + string("BACKUP") + getDirSeparator() + requestDir + getDirSeparator() + requestFile;
            if (!fileExists(fileName))
                return "";
            else
                return fileName;
        }
        else
            return fileName;
    }
    else
        return "";
}

void METADemuxer::updateReport(bool checkTime)
{
    auto currentTime = std::chrono::steady_clock::now();
    if (!checkTime || currentTime - m_lastReportTime > std::chrono::microseconds(250000))
    {
        uint64_t currentProcessedSize = 0;
        double progress = 100.0;
        if (m_totalSize > 0)
        {
            currentProcessedSize = getDemuxedSize();
            progress = currentProcessedSize / (double)m_totalSize * 100.0;
            if (progress > 100.0)
                progress = 100.0;
        }
        lineBack();
        cout << doubleToStr(progress, 1) << "% complete" << std::endl;
        m_lastReportTime = currentTime;
    }
}

void METADemuxer::lineBack()
{
#ifdef _WIN32
    HANDLE consoleOutput;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(consoleOutput, &csbi);
    if (csbi.dwCursorPosition.Y == m_lastProgressY)
    {
        csbi.dwCursorPosition.X = 0;
        m_lastProgressY = csbi.dwCursorPosition.Y;
        csbi.dwCursorPosition.Y--;
        SetConsoleCursorPosition(consoleOutput, csbi.dwCursorPosition);
    }
    else
        m_lastProgressY = csbi.dwCursorPosition.Y + 1;
#else
    if (!sLastMsg)
        std::cout << "\x1bM";
    sLastMsg = false;
#endif
}

// ------------------- StreamInfo ---------------------
int StreamInfo::read()
{
    // m_readRez = 0;
    int readRez = 0;
    if (m_asyncMode && !m_notificated && !m_isEOF)
    {
        m_dataReader->notify(m_readerID, m_dataReader->getPreReadThreshold());
        m_notificated = true;
    }

    if (m_lastAVRez != 0)
    {
        if (m_isEOF)
        {
            m_blockSize = 0;
            return BufferedFileReader::DATA_EOF2;
        }
        m_lastAVRez = 0;

        m_data = m_dataReader->readBlock(m_readerID, m_blockSize, readRez);
        if (readRez == BufferedFileReader::DATA_NOT_READY || readRez == BufferedFileReader::DATA_DELAYED)
        {
            m_lastAVRez = readRez;
            return readRez;
        }
        else if (readRez == BufferedFileReader::DATA_EOF)
            m_isEOF = true;
        m_streamReader->setBuffer(m_data, m_blockSize, m_isEOF);
        m_readCnt += m_blockSize;
        m_notificated = false;
    }
    return readRez;
}

// ------------------------------ ContainerToReaderWrapper --------------------------------

uint8_t* ContainerToReaderWrapper::readBlock(uint32_t readerID, uint32_t& readCnt, int& rez, bool* firstBlockVar)
{
    rez = 0;
    uint8_t* data = 0;
    map<uint32_t, ReaderInfo>::iterator itr = m_readerInfo.find(readerID);
    if (itr == m_readerInfo.end())
        return 0;

    DemuxerData& demuxerData = itr->second.m_demuxerData;
    uint32_t pid = itr->second.m_pid;
    uint64_t nFileBlockSize = demuxerData.m_demuxer->getFileBlockSize();

    if (demuxerData.m_firstRead)
    {
        for (map<uint32_t, DemuxerReadPolicy>::iterator itr = demuxerData.m_pids.begin();
             itr != demuxerData.m_pids.end(); ++itr)
        {
            MemoryBlock& vect = demuxerData.demuxedData[itr->first];
            vect.reserve(nFileBlockSize + m_readBuffOffset);
            vect.resize(m_readBuffOffset);
        }
        demuxerData.m_firstRead = false;
    }
    StreamData& streamData = demuxerData.demuxedData[pid];

    uint32_t lastReadCnt = demuxerData.lastReadCnt[pid];
    if (lastReadCnt > 0)
    {
        demuxerData.lastReadCnt[pid] = 0;
        uint32_t currentSize = streamData.size() - m_readBuffOffset;
        assert(currentSize >= lastReadCnt);
        if (currentSize > lastReadCnt)
        {
            uint8_t* dataStart = streamData.data() + m_readBuffOffset;
            memcpy(dataStart, dataStart + lastReadCnt, currentSize - lastReadCnt);
        }
        streamData.resize(m_readBuffOffset + currentSize - lastReadCnt);
        demuxerData.lastReadCnt[pid] = 0;
    }

    readCnt = FFMIN(streamData.size(), nFileBlockSize) - m_readBuffOffset;
    DemuxerReadPolicy policy = demuxerData.m_pids[pid];
    if ((readCnt > 0 && (policy == drpFragmented || demuxerData.lastReadCnt[pid] == BufferedFileReader::DATA_EOF2 ||
                         demuxerData.lastReadCnt[pid] == BufferedFileReader::DATA_EOF2)) ||
        readCnt >= MIN_READED_BLOCK)
    {
        data = streamData.data();
        demuxerData.lastReadCnt[pid] = readCnt;
        demuxerData.lastReadRez[pid] = 0;
    }
    else if (demuxerData.lastReadRez[pid] != AbstractReader::DATA_DELAYED || demuxerData.m_allFragmented)
    {
        map<uint32_t, uint32_t> dataTypeMap;
        int demuxRez = 0;
        do
        {
            int64_t discardSize = 0;
            demuxRez =
                demuxerData.m_demuxer->simpleDemuxBlock(demuxerData.demuxedData, demuxerData.m_pidSet, discardSize);
            for (DemuxedData::const_iterator itr = demuxerData.demuxedData.begin();
                 itr != demuxerData.demuxedData.end() && !m_terminated; ++itr)
            {
                if (itr->second.size() > MAX_DEMUX_BUFFER_SIZE)
                    THROW(ERR_CONTAINER_STREAM_NOT_SYNC,
                          "Reading buffer overflow. Possible container streams are not syncronized. Please, verify "
                          "stream fps. File name: "
                              << demuxerData.m_streamName);
            }
            m_discardedSize += discardSize;
            readCnt = streamData.size() - m_readBuffOffset;
        } while (demuxRez == 0 && readCnt < MIN_READED_BLOCK && policy != drpFragmented && !m_terminated);

        demuxerData.lastReadCnt[pid] = readCnt;
        data = streamData.data();
        if (readCnt > 0)
        {
            rez = demuxerData.m_demuxer->getLastReadRez();
        }
        else if (demuxerData.m_demuxer->getLastReadRez() == AbstractReader::DATA_EOF)
            rez = AbstractReader::DATA_EOF;
        else
        {
            if (policy == drpReadSequence)
                rez = AbstractReader::DATA_NOT_READY;
            else
                rez = AbstractReader::DATA_DELAYED;
        }
        demuxerData.lastReadRez[pid] = rez;
    }
    else
        rez = AbstractReader::DATA_DELAYED;
    return data;
}

void ContainerToReaderWrapper::terminate()
{
    m_terminated = true;
    for (std::map<std::string, DemuxerData>::iterator i = m_demuxers.begin(); i != m_demuxers.end(); ++i)
        i->second.m_demuxer->terminate();
}

void ContainerToReaderWrapper::resetDelayedMark()
{
    for (map<uint32_t, ReaderInfo>::iterator itr = m_readerInfo.begin(); itr != m_readerInfo.end(); ++itr)
    {
        DemuxerData& demuxerData = itr->second.m_demuxerData;
        for (map<uint32_t, uint32_t>::iterator itr2 = demuxerData.lastReadRez.begin();
             itr2 != demuxerData.lastReadRez.end(); ++itr2)
            if (itr2->second == AbstractReader::DATA_DELAYED)
                itr2->second = 0;
    }
}

uint32_t ContainerToReaderWrapper::createReader(int readBuffOffset)
{
    m_readBuffOffset = readBuffOffset;
    return ++m_readerCnt;
}

void ContainerToReaderWrapper::deleteReader(uint32_t readerID)
{
    map<uint32_t, ReaderInfo>::iterator itr = m_readerInfo.find(readerID);
    if (itr == m_readerInfo.end())
        return;
    ReaderInfo& ri = itr->second;
    ri.m_demuxerData.m_pids.erase(ri.m_pid);
    if (ri.m_demuxerData.m_pids.empty())
    {
        delete ri.m_demuxerData.m_demuxer;
        m_demuxers.erase(ri.m_demuxerData.m_streamName);
    }
    m_readerInfo.erase(itr);
}

bool ContainerToReaderWrapper::openStream(uint32_t readerID, const char* streamName, int pid,
                                          const CodecInfo* codecInfo)
{
    AbstractDemuxer* demuxer = m_demuxers[streamName].m_demuxer;
    if (demuxer == 0)
    {
        string ext = strToUpperCase(extractFileExt(streamName));
        if ((ext == "264" || ext == "H264" || ext == "MVC") && pid)
        {
            demuxer = m_demuxers[streamName].m_demuxer = new CombinedH264Demuxer(m_readManager, "");
            m_demuxers[streamName].m_streamName = streamName;
        }
        else if (ext == "TS" || ext == "M2TS" || ext == "MTS" || ext == "M2T" || ext == "SSIF")
        {
            demuxer = m_demuxers[streamName].m_demuxer = new TSDemuxer(m_readManager, "");
            m_demuxers[streamName].m_streamName = streamName;
        }
        else if (ext == "EVO" || ext == "VOB" || ext == "MPG" || ext == "MPEG")
        {
            demuxer = m_demuxers[streamName].m_demuxer = new ProgramStreamDemuxer(m_readManager);
            m_demuxers[streamName].m_streamName = streamName;
        }
        else if (ext == "MKV" || ext == "MKA")
        {
            demuxer = m_demuxers[streamName].m_demuxer = new MatroskaDemuxer(m_readManager);
            m_demuxers[streamName].m_streamName = streamName;
        }
        else if (ext == "MOV" || ext == "MP4" || ext == "M4V" || ext == "M4A")
        {
            demuxer = m_demuxers[streamName].m_demuxer = new MovDemuxer(m_readManager);
            m_demuxers[streamName].m_streamName = streamName;
        }
        else
            THROW(ERR_UNSUPPORTER_CONTAINER_FORMAT, "Unsupported container format: " << streamName);
        demuxer->setFileIterator(m_demuxers[streamName].m_iterator);

        demuxer->openFile(streamName);
    }

    if (SubTrackFilter::isSubTrack(pid))
    {
        if (!demuxer || !demuxer->isPidFilterSupported())
            THROW(ERR_INVALID_CODEC_FORMAT, "Unsupported parameter subTrack for format " << extractFileExt(streamName));
        int srcPID = pid >> 16;
        if (demuxer->getPidFilter(srcPID) == 0)
        {
            if (codecInfo->codecID == CODEC_V_MPEG4_H264 || codecInfo->codecID == CODEC_V_MPEG4_H264_DEP)
                demuxer->setPidFilter(srcPID, new CombinedH264Filter(srcPID));
            else
                THROW(ERR_INVALID_CODEC_FORMAT, "Unsupported parameter subTrack for codec " << codecInfo->displayName);
        }
    }

    TSDemuxer* tsDemuxer = dynamic_cast<TSDemuxer*>(demuxer);
    if (tsDemuxer)
    {
        MPLSCache::const_iterator itr = m_owner.m_mplsStreamMap.find(streamName);
        if (itr != m_owner.m_mplsStreamMap.end())
            tsDemuxer->setMPLSInfo(itr->second.m_playItems);
    }

    if (codecInfo &&
        (codecInfo->codecID == CODEC_S_PGS || codecInfo->codecID == CODEC_S_SUP || codecInfo->codecID == CODEC_S_SRT))
        m_demuxers[streamName].m_pids.insert(std::make_pair(pid, drpFragmented));
    else
    {
        m_demuxers[streamName].m_pids.insert(std::make_pair(pid, drpReadSequence));
        m_demuxers[streamName].m_allFragmented = false;
    }
    m_demuxers[streamName].m_pidSet.insert(pid);
    m_readerInfo.insert(std::make_pair(readerID, ReaderInfo(m_demuxers[streamName], pid)));
    return true;
}

void ContainerToReaderWrapper::setFileIterator(const char* streamName, FileNameIterator* itr)
{
    if (m_demuxers[streamName].m_iterator == 0)
        m_demuxers[streamName].m_iterator = itr;
}
