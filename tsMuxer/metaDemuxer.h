#ifndef META_DEMUXER_H_
#define META_DEMUXER_H_

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "abstractDemuxer.h"
#include "abstractStreamReader.h"
#include "avPacket.h"
#include "bufferedReaderManager.h"
#include "vodCoreException.h"

// META file demuxer

struct StreamInfo
{
    AbstractReader* m_dataReader;
    AbstractStreamReader* m_streamReader;
    StreamInfo(AbstractReader* dataReader, AbstractStreamReader* streamReader, const std::string& streamName,
               const std::string& fullStreamName, int pid, bool isSubStream = false)
    {
        m_streamName = streamName;
        m_fullStreamName = fullStreamName;
        m_dataReader = dataReader;
        m_readerID = dataReader->createReader(streamReader->getTmpBufferSize());
        if (!dataReader->openStream(m_readerID, m_streamName.c_str(), pid, &streamReader->getCodecInfo()))
            THROW(ERR_CANT_OPEN_STREAM, "Can't open stream: " << m_streamName)
        m_streamReader = streamReader;
        m_pid = pid;
        m_readCnt = 0;
        m_lastAVRez = AbstractStreamReader::NEED_MORE_DATA;
        m_lastDTS = 0;
        lastReadRez = 0;
        m_flushed = false;
        m_timeShift = 0;
        m_isEOF = false;
        m_notificated = true;
        m_asyncMode = true;
        m_blockSize = 0;
        m_isSubStream = isSubStream;
    }
    ~StreamInfo()
    {
        // delete m_streamReader;
    }

    int read();

    int m_lastAVRez;
    int64_t m_readCnt;
    bool m_notificated;
    int m_readerID;
    uint32_t m_blockSize;
    // int m_readRez;
    int64_t m_lastDTS;
    uint8_t* m_data;
    std::string m_streamName;
    std::string m_fullStreamName;
    int lastReadRez;
    uint32_t m_pid;
    bool m_flushed;
    int64_t m_timeShift;
    std::string m_lang;
    std::string m_codec;
    std::map<std::string, std::string> m_addParams;
    bool m_isEOF;
    bool m_asyncMode;
    bool m_isSubStream;
};

enum class DemuxerReadPolicy
{
    drpReadSequence,
    drpFragmented
};

class METADemuxer;

class ContainerToReaderWrapper : public AbstractReader
{
   public:
    struct DemuxerData
    {
        std::map<uint32_t, DemuxerReadPolicy> m_pids;
        PIDSet m_pidSet;  // same as pids
        AbstractDemuxer* m_demuxer;
        std::string m_streamName;
        DemuxedData demuxedData;
        FileNameIterator* m_iterator;
        std::map<uint32_t, uint32_t> lastReadCnt;
        std::map<uint32_t, uint32_t> lastReadRez;
        DemuxerData()
        {
            m_demuxer = nullptr;
            m_firstRead = true;
            m_iterator = nullptr;
            m_allFragmented = true;
        }
        bool m_firstRead;
        bool m_allFragmented;  // // container reader does not contain any sequence track reader(s)
    };

    struct ReaderInfo
    {
        ReaderInfo(DemuxerData& demuxerData, const uint32_t pid) : m_demuxerData(demuxerData), m_pid(pid) {}
        DemuxerData& m_demuxerData;
        uint32_t m_pid;
    };

    ContainerToReaderWrapper(const METADemuxer& owner, BufferedReaderManager& readManager)
        : m_readBuffOffset(0), m_readManager(readManager), m_owner(owner)
    {
        auto& brm = const_cast<BufferedReaderManager&>(readManager);

        m_blockSize = brm.getBlockSize();
        m_allocSize = brm.getAllocSize();
        m_prereadThreshold = brm.getPreReadThreshold();

        m_readerCnt = 0;
        m_discardedSize = 0;
        m_terminated = false;
    }
    uint8_t* readBlock(uint32_t readerID, uint32_t& readCnt, int& rez, bool* firstBlockVar = nullptr) override;
    void notify(uint32_t readerID, uint32_t dataReaded) override { return; }
    int32_t createReader(int readBuffOffset = 0) override;
    void deleteReader(uint32_t readerID) override;
    bool openStream(uint32_t readerID, const char* streamName, int pid = 0, const CodecInfo* codecInfo = nullptr) override;
    void setFileIterator(const char* streamName, FileNameIterator* itr);
    void resetDelayedMark() const;
    int64_t getDiscardedSize() { return m_discardedSize; };

    bool gotoByte(uint32_t readerID, uint64_t seekDist) override { return false; }
    void terminate();
    std::map<std::string, DemuxerData> m_demuxers;

   private:
    int64_t m_discardedSize;
    int m_readerCnt;
    size_t m_readBuffOffset;
    const BufferedReaderManager& m_readManager;
    std::map<uint32_t, ReaderInfo> m_readerInfo;
    const METADemuxer& m_owner;
    bool m_terminated;
};

typedef std::map<std::string, MPLSParser> MPLSCache;

struct DetectStreamRez
{
    DetectStreamRez() : fileDurationNano(0) {}

    AVChapters chapters;
    std::vector<CheckStreamRez> streams;
    int64_t fileDurationNano;
};

class METADemuxer : public AbstractDemuxer
{
   public:
    METADemuxer(BufferedReaderManager& readManager);
    ~METADemuxer() override;
    int readPacket(AVPacket& avPacket);
    void readClose() final;
    uint64_t getDemuxedSize() override;
    int addStream(const std::string& codec, const std::string& codecStreamName,
                  const std::map<std::string, std::string>& addParams);
    void openFile(const std::string& streamName) override;
    const std::vector<StreamInfo>& getStreamInfo() const { return m_codecInfo; }
    static DetectStreamRez DetectStreamReader(BufferedReaderManager& readManager, const std::string& fileName,
                                              bool calcDuration);
    std::vector<StreamInfo>& getCodecInfo() { return m_codecInfo; }
    int getLastReadRez() override { return m_lastReadRez; }
    int64_t totalSize() const { return m_totalSize; }
    static std::string mplsTrackToFullName(const std::string& mplsFileName, const std::string& mplsNum);
    static std::string mplsTrackToSSIFName(const std::string& mplsFileName, const std::string& mplsNum);
    bool m_HevcFound;

   private:
    std::vector<FileListIterator*> m_iterators;
    int m_lastReadRez;
    ContainerToReaderWrapper m_containerReader;
    int m_lastProgressY;
    std::chrono::steady_clock::time_point m_lastReportTime;
    int64_t m_totalSize;
    bool m_flushDataMode;
    const BufferedReaderManager& m_readManager;
    std::string m_streamName;
    std::vector<StreamInfo> m_codecInfo;

    // MPLSPlayItemsMap m_mplsPlayItemsMap;
    // MPLSPlayItemsMap m_mplsStreamMap;
    MPLSCache m_mplsStreamMap;
    std::set<std::string> m_processedTracks;

    friend class ContainerToReaderWrapper;

    static AbstractStreamReader* createCodec(const std::string& codecName,
                                             const std::map<std::string, std::string>& addParams,
                                             const std::string& codecStreamName,
                                             const std::vector<MPLSPlayItem>& mplsInfo);
    inline void updateReport(bool checkTime);
    void lineBack();
    static CheckStreamRez detectTrackReader(uint8_t* tmpBuffer, int len,
                                            AbstractStreamReader::ContainerType containerType, int containerDataType,
                                            int containerStreamIndex);
    static std::string findBluRayFile(const std::string& streamDir, const std::string& requestDir,
                                      const std::string& requestFile);
    std::vector<MPLSParser> getMplsInfo(const std::string& mplsFileName);

    int addPGSubStream(const std::string& codec, const std::string& _codecStreamName,
                       const std::map<std::string, std::string>& addParams, const MPLSStreamInfo* subStream);
    static void addTrack(std::vector<CheckStreamRez>& rez, CheckStreamRez trackRez);
    static std::vector<MPLSPlayItem> mergePlayItems(const std::vector<MPLSParser>& mplsInfoList);
};

#endif
