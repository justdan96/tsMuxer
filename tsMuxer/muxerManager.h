#ifndef MUXER_MANAGER_H_
#define MUXER_MANAGER_H_

#include "abstractMuxer.h"
#include "bufferedFileWriter.h"
#include "bufferedReaderManager.h"
#include "metaDemuxer.h"

class FileFactory;

class MuxerManager final
{
   public:
    static constexpr uint32_t PHYSICAL_SECTOR_SIZE =
        2048;  // minimum write align requirement. Should be readed from OS in a next version
    static constexpr int BLURAY_SECTOR_SIZE =
        PHYSICAL_SECTOR_SIZE * 3;  // real sector size is 2048, but M2TS frame required addition rounding by 3 blocks

    MuxerManager(BufferedReaderManager& readManager, AbstractMuxerFactory& factory);
    virtual ~MuxerManager();

    void setAsyncMode(const bool val) { m_asyncMode = val; }

    // void setFileBlockSize ( uint32_t nFileBlockSize) { m_fileBlockSize = nFileBlockSize; }
    // int32_t getFileBlockSize() const { return m_fileBlockSize; }

    bool isAsyncMode() const { return m_asyncMode; }

    virtual bool openMetaFile(const std::string& fileName);
    int addStream(const std::string& codecName, const std::string& fileName,
                  const std::map<std::string, std::string>& addParams);

    void doMux(const std::string& outFileName, FileFactory* fileFactory);

    void setCutStart(const int64_t value) { m_cutStart = value; }
    int64_t getCutStart() const { return m_cutStart; }

    void setCutEnd(const int64_t value) { m_cutEnd = value; }
    int64_t getCutEnd() const { return m_cutEnd; }

    void waitForWriting() const;

    virtual void asyncWriteBuffer(AbstractMuxer* muxer, uint8_t* buff, int len, AbstractOutputStream* dstFile);
    virtual int syncWriteBuffer(AbstractMuxer* muxer, uint8_t* buff, int len, AbstractOutputStream* dstFile);
    void muxBlockFinished(const AbstractMuxer* muxer);

    void parseMuxOpt(const std::string& opts);
    int getTrackCnt() { return static_cast<int>(m_metaDemuxer.getCodecInfo().size()); }
    bool getHevcFound() const { return m_metaDemuxer.m_HevcFound; }
    AbstractMuxer* getMainMuxer() const;
    AbstractMuxer* getSubMuxer() const;
    bool isStereoMode() const;

    void setAllowStereoMux(bool value);

    bool isMvcBaseViewR() const { return m_mvcBaseViewR; }
    int64_t totalSize() const { return m_metaDemuxer.totalSize(); }
    int getExtraISOBlocks() const { return m_extraIsoBlocks; }

    bool useReproducibleIsoHeader() const { return m_reproducibleIsoHeader; }

    enum class SubTrackMode
    {
        All,
        Forced
    };
    int getDefaultAudioTrackIdx() const;
    int getDefaultSubTrackIdx(SubTrackMode& mode) const;

   private:
    void preinitMux(const std::string& outFileName, FileFactory* fileFactory);
    AbstractMuxer* createMuxer();
    void asyncWriteBlock(const WriterData& data) const;
    void checkTrackList(const std::vector<StreamInfo>& ci) const;

   private:
    AbstractMuxer* m_mainMuxer;
    AbstractMuxer* m_subMuxer;

    bool m_asyncMode;
    // int32_t m_fileBlockSize;
    std::string m_outFileName;
    std::condition_variable reinitCond;
    METADemuxer m_metaDemuxer;
    int64_t m_cutStart;
    int64_t m_cutEnd;
    BufferedFileWriter* m_fileWriter;
    AbstractMuxerFactory& m_factory;
    bool m_allowStereoMux;
    std::set<int> m_subStreamIndex;
    std::string m_muxOpts;
    bool m_interleave;

    std::vector<WriterData> m_delayedData;  // ssif interlieave
    bool m_subBlockFinished;
    bool m_mainBlockFinished;
    bool m_mvcBaseViewR;
    int64_t m_ptsOffset;
    int m_extraIsoBlocks;
    bool m_bluRayMode;
    bool m_demuxMode;
    bool m_reproducibleIsoHeader = false;
};

#endif  // _MUXER_MANAGER_H_
