#include "muxerManager.h"

#include "fs/textfile.h"
#include "h264StreamReader.h"
#include "iso_writer.h"
#include "muxerManager.h"
#include "tsMuxer.h"
#include "vodCoreException.h"

using namespace std;

// static const int SSIF_INTERLEAVE_BLOCKSIZE = 1024 * 1024 * 7;
static const int MAX_FRAME_SIZE = 1200000;  // 1.2m

MuxerManager::MuxerManager(const BufferedReaderManager& readManager, AbstractMuxerFactory& factory)
    : m_metaDemuxer(readManager), m_factory(factory)
{
    m_asyncMode = true;
    m_fileWriter = 0;
    m_cutStart = 0;
    m_cutEnd = 0;
    m_mainMuxer = m_subMuxer = 0;
    m_allowStereoMux = false;
    m_interleave = false;
    m_subBlockFinished = false;
    m_mainBlockFinished = false;
    m_ptsOffset = 378000000ll;
    m_mvcBaseViewR = false;
    m_extraIsoBlocks = 0;
    m_bluRayMode = false;
    m_demuxMode = false;
}

MuxerManager::~MuxerManager()
{
    delete m_mainMuxer;
    delete m_subMuxer;
}

void MuxerManager::preinitMux(const std::string& outFileName, FileFactory* fileFactory)
{
    vector<StreamInfo>& ci = m_metaDemuxer.getCodecInfo();
    bool mvcTrackFirst = false;
    bool firstH264Track = true;
    for (vector<StreamInfo>::iterator itr = ci.begin(); itr != ci.end(); ++itr)
    {
        StreamInfo& si = *itr;
        H264StreamReader* h264Reader = dynamic_cast<H264StreamReader*>(si.m_streamReader);
        if (h264Reader)
        {
            h264Reader->setStartPTS(m_ptsOffset);
            if (firstH264Track)
            {
                if (si.m_isSubStream)
                    mvcTrackFirst = true;
                firstH264Track = false;
            }
        }
        if (si.m_isSubStream && m_allowStereoMux)
        {
            if (!m_subMuxer)
            {
                m_subMuxer = m_factory.newInstance(this);
                TSMuxer* tsMuxer = dynamic_cast<TSMuxer*>(m_subMuxer);
                if (tsMuxer)
                    tsMuxer->setPtsOffset(m_ptsOffset);
                m_subMuxer->parseMuxOpt(m_muxOpts);
            }
        }
        else
        {
            if (!m_mainMuxer)
            {
                m_mainMuxer = m_factory.newInstance(this);
                TSMuxer* tsMuxer = dynamic_cast<TSMuxer*>(m_mainMuxer);
                if (tsMuxer)
                    tsMuxer->setPtsOffset(m_ptsOffset);
                m_mainMuxer->parseMuxOpt(m_muxOpts);
            }
        }
    }

    if (m_mainMuxer)
    {
        m_mainMuxer->setFileName(outFileName, fileFactory);
        // m_mainMuxer->openDstFile();
    }

    if (m_subMuxer)
    {
        m_subMuxer->setFileName(outFileName, fileFactory);
        if (strEndWith(strToLowerCase(outFileName), ".ssif"))
        {
            m_interleave = true;
        }
        else
        {
            m_subMuxer->setFileName(m_subMuxer->getNextName(outFileName), fileFactory);
            if (fileFactory && fileFactory->isVirtualFS())
                m_interleave = true;
        }
        m_subMuxer->setBlockMuxMode(SUB_INTERLEAVE_BLOCKSIZE - MAX_FRAME_SIZE, BLURAY_SECTOR_SIZE);
        if (m_mainMuxer)
            m_mainMuxer->setBlockMuxMode(MAIN_INTERLEAVE_BLOCKSIZE - MAX_FRAME_SIZE, BLURAY_SECTOR_SIZE);
    }

    if (m_subMuxer && m_mainMuxer)
    {
        m_subMuxer->setSubMode(m_mainMuxer, mvcTrackFirst);
        m_mainMuxer->setMasterMode(m_subMuxer, !mvcTrackFirst);
    }

    for (vector<StreamInfo>::iterator itr = ci.begin(); itr != ci.end(); ++itr)
    {
        StreamInfo& si = *itr;
        int rez = si.read();
        if (si.m_isSubStream && m_allowStereoMux)
        {
            m_subStreamIndex.insert(si.m_streamReader->getStreamIndex());
            m_subMuxer->intAddStream(si.m_fullStreamName, si.m_codec, si.m_streamReader->getStreamIndex(),
                                     si.m_addParams, si.m_streamReader);
        }
        else
        {
            m_mainMuxer->intAddStream(si.m_fullStreamName, si.m_codec, si.m_streamReader->getStreamIndex(),
                                      si.m_addParams, si.m_streamReader);
        }
    }

    checkTrackList(ci);

    if (m_mainMuxer)
        m_mainMuxer->openDstFile();
    if (m_subMuxer)
    {
        if (!m_interleave || (fileFactory && fileFactory->isVirtualFS()))
            m_subMuxer->openDstFile();
        else
            m_subMuxer->joinToMasterFile();  // direct ssif writing
    }
}

void MuxerManager::checkTrackList(const vector<StreamInfo>& ci)
{
    if (m_demuxMode)
        return;

    bool avcFound = false;
    bool mvcFound = false;
    bool aacFound = false;

    for (vector<StreamInfo>::const_iterator itr = ci.begin(); itr != ci.end(); ++itr)
    {
        const StreamInfo& si = *itr;
        if (si.m_codec == h264CodecInfo.programName)
            avcFound = true;
        else if (si.m_codec == h264DepCodecInfo.programName)
            mvcFound = true;
        else if (si.m_codec == aacCodecInfo.programName)
            aacFound = true;
    }

    if (m_bluRayMode && aacFound)
        LTRACE(LT_ERROR, 2, "Warning! AAC codec is not standard for BD disks!");

    if (!avcFound && mvcFound)
        THROW(ERR_INVALID_STREAMS_SELECTED,
              "Fatal error: MVC depended view track can't be muxed without AVC base view track");
}

void MuxerManager::doMux(const string& outFileName, FileFactory* fileFactory)
{
    preinitMux(outFileName, fileFactory);

    m_fileWriter = new BufferedFileWriter();
    AVPacket avPacket;

    while (true)
    {
        int avRez = m_metaDemuxer.readPacket(avPacket);

        if (avRez == BufferedReader::DATA_EOF)
            break;
        if (m_cutStart > 0)
        {
            if (avPacket.pts < m_cutStart)
                continue;
        }
        if (m_cutEnd > 0 && avPacket.pts >= m_cutEnd)
            break;

        if (m_subStreamIndex.find(avPacket.stream_index) != m_subStreamIndex.end())
            m_subMuxer->muxPacket(avPacket);
        else
            m_mainMuxer->muxPacket(avPacket);
    }

    LTRACE(LT_INFO, 2, "Flushing write buffer");

    if (m_subMuxer)
        m_subMuxer->doFlush();
    m_mainMuxer->doFlush();

    for (int i = 0; i < m_delayedData.size(); ++i) asyncWriteBlock(m_delayedData[i]);

    waitForWriting();

    m_mainMuxer->close();
    if (m_subMuxer)
        m_subMuxer->close();

    delete m_fileWriter;

    m_fileWriter = 0;
}

int MuxerManager::addStream(const string& codecName, const string& fileName, const map<string, string>& addParams)
{
    int rez = m_metaDemuxer.addStream(codecName, fileName, addParams);
    return rez;
}

bool MuxerManager::openMetaFile(const string& fileName)
{
    TextFile file(fileName.c_str(), File::ofRead);
    std::string str;
    file.readLine(str);
    while (str.length() > 0)
    {
        if (strStartWith(str, "MUXOPT"))
        {
            m_muxOpts = str;
            parseMuxOpt(m_muxOpts);
            m_mvcBaseViewR = m_muxOpts.find("right-eye") != string::npos;
        }
        file.readLine(str);
    }
    file.close();

    m_metaDemuxer.openFile(fileName);
    return true;
}

void MuxerManager::muxBlockFinished(AbstractMuxer* muxer)
{
    if (muxer == m_subMuxer)
        m_subBlockFinished = true;
    else
        m_mainBlockFinished = true;

    if (m_subBlockFinished && m_mainBlockFinished)
    {
        for (int i = 0; i < m_delayedData.size(); ++i)
        {
            asyncWriteBlock(m_delayedData[i]);
        }
        m_delayedData.clear();
        m_subBlockFinished = false;
        m_mainBlockFinished = false;
    }
}

void MuxerManager::asyncWriteBuffer(AbstractMuxer* muxer, uint8_t* buff, int len, AbstractOutputStream* dstFile)
{
    WriterData data;
    data.m_buffer = buff;
    data.m_bufferLen = len;
    data.m_mainFile = dstFile;
    data.m_command = WriterData::wdWrite;

    if (m_interleave && muxer == m_mainMuxer)
    {
        // do interlieave of SSIF blocks. Place sub channel blocks first, delay main muxer blocks
        m_delayedData.push_back(data);
        return;
    }

    asyncWriteBlock(data);
}

void MuxerManager::asyncWriteBlock(const WriterData& data)
{
    int nMaxWriteQueueSize = 256 * 1024 * 1024 / DEFAULT_FILE_BLOCK_SIZE;
    while (m_fileWriter->getQueueSize() > nMaxWriteQueueSize)
    {
        Process::sleep(1);
    }
    m_fileWriter->addWriterData(data);
}

int MuxerManager::syncWriteBuffer(AbstractMuxer* muxer, uint8_t* buff, int len, AbstractOutputStream* dstFile)
{
    assert(m_interleave == 0);
    int rez = dstFile->write(buff, len);
    dstFile->sync();
    return rez;
}
void MuxerManager::parseMuxOpt(const string& opts)
{
    vector<string> params = splitQuotedStr(opts.c_str(), ' ');
    for (size_t i = 0; i < params.size(); i++)
    {
        vector<string> paramPair = splitStr(trimStr(params[i]).c_str(), '=');
        if (paramPair.size() == 0)
            continue;

        if (paramPair[0] == "--start-time" && paramPair.size() > 1)
        {
            if (paramPair[1].find(":") != string::npos)
                m_ptsOffset = int64_t(timeToFloat(paramPair[1]) * 90000.0 + 0.5);
            else
                m_ptsOffset = strToInt64u(paramPair[1].c_str()) * 2;  // source in a 45Khz clock
        }
        else if (paramPair[0] == "--no-asyncio")
            setAsyncMode(false);
        else if (paramPair[0] == "--cut-start" || paramPair[0] == "--cut-end")
        {
            uint64_t coeff = 1;
            string postfix;
            for (int i = paramPair[1].size() - 1; i >= 0; i--)
                if (!(paramPair[1][i] >= '0' && paramPair[1][i] <= '9' || paramPair[1][i] == '.'))
                    postfix = paramPair[1][i] + postfix;

            postfix = strToUpperCase(postfix);

            if (postfix == "MS")
                coeff = 1000000;
            else if (postfix == "S")
                coeff = 1000000000;
            else if (postfix == "MIN")
                coeff = 60000000000ull;
            string prefix = paramPair[1].substr(0, paramPair[1].size() - postfix.size());
            if (paramPair[0] == "--cut-start")
                setCutStart(strToInt64(prefix.c_str()) * coeff);
            else
                setCutEnd(strToInt64(prefix.c_str()) * coeff);
        }
        else if (paramPair[0] == "--split-duration" || paramPair[0] == "--split-size")
        {
            if (m_extraIsoBlocks == 0)
                m_extraIsoBlocks = 4;
        }
        else if (paramPair[0] == "--extra-iso-space")
        {
            m_extraIsoBlocks = strToInt32(paramPair[1]);
        }
        else if (paramPair[0] == "--blu-ray" || paramPair[0] == "--blu-ray-v3" || paramPair[0] == "--avchd")
        {
            m_bluRayMode = true;
        }
        else if (paramPair[0] == "--demux")
        {
            m_demuxMode = true;
        }
    }
}

void MuxerManager::waitForWriting()
{
    while (!m_fileWriter->isQueueEmpty()) Process::sleep(1);
}

AbstractMuxer* MuxerManager::createMuxer() { return m_factory.newInstance(this); }

AbstractMuxer* MuxerManager::getMainMuxer() { return m_mainMuxer; }

AbstractMuxer* MuxerManager::getSubMuxer() { return m_subMuxer; }

bool MuxerManager::isStereoMode() const { return m_subMuxer != 0; }

void MuxerManager::setAllowStereoMux(bool value) { m_allowStereoMux = value; }
