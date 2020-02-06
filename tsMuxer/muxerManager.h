#ifndef __MUXER_MANAGER_H
#define __MUXER_MANAGER_H

#include "abstractMuxer.h"
#include "bufferedFileWriter.h"
#include "bufferedReaderManager.h"
#include "metaDemuxer.h"

class FileFactory;

class MuxerManager
{
   public:
    static const uint32_t PHYSICAL_SECTOR_SIZE =
        2048;  // minimum write align requirement. Should be readed from OS in a next version
    static const int BLURAY_SECTOR_SIZE =
        PHYSICAL_SECTOR_SIZE * 3;  // real sector size is 2048, but M2TS frame required addition rounding by 3 blocks

    MuxerManager(const BufferedReaderManager& readManager, AbstractMuxerFactory& factory);
    virtual ~MuxerManager();

    void setAsyncMode(bool val) { m_asyncMode = val; }

    // void setFileBlockSize ( uint32_t nFileBlockSize) { m_fileBlockSize = nFileBlockSize; }
    // int32_t getFileBlockSize() const { return m_fileBlockSize; }

    bool isAsyncMode() const { return m_asyncMode; }

    virtual bool openMetaFile(const std::string& fileName);
    int addStream(const std::string& codecName, const std::string& fileName,
                  const std::map<std::string, std::string>& addParams);

    void doMux(const std::string& outFileName, FileFactory* fileFactory);

    void setCutStart(int64_t value) { m_cutStart = value; }
    int64_t getCutStart() const { return m_cutStart; }

    void setCutEnd(int64_t value) { m_cutEnd = value; }
    int64_t getCutEnd() const { return m_cutEnd; }

    void waitForWriting();

    virtual void asyncWriteBuffer(AbstractMuxer* muxer, uint8_t* buff, int len, AbstractOutputStream* dstFile);
    virtual int syncWriteBuffer(AbstractMuxer* muxer, uint8_t* buff, int len, AbstractOutputStream* dstFile);
    void muxBlockFinished(AbstractMuxer* muxer);

    void parseMuxOpt(const std::string& opts);
    int getTrackCnt() { return (int)m_metaDemuxer.getCodecInfo().size(); }
    bool getHevcFound() { return m_metaDemuxer.m_HevcFound; }
    AbstractMuxer* getMainMuxer();
    AbstractMuxer* getSubMuxer();
    bool isStereoMode() const;

    void setAllowStereoMux(bool value);

    bool isMvcBaseViewR() const { return m_mvcBaseViewR; }
    int64_t totalSize() const { return m_metaDemuxer.totalSize(); }
    int getExtraISOBlocks() const { return m_extraIsoBlocks; }

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
    void asyncWriteBlock(const WriterData& data);
    void checkTrackList(const std::vector<StreamInfo>& ci);

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
};

#endif  // __MUXER_MANAGER_H
