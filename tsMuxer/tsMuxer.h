#ifndef TS_MUXER_H_
#define TS_MUXER_H_

#include <types/types.h>

#include <map>
#include <vector>

#include "abstractMuxer.h"
#include "avPacket.h"
#include "hevc.h"
#include "limits.h"

enum V3Flags
{
    HDMV_V3 = 1,
    HDR10 = 2,
    DV = 4,
    SL_HDR2 = 8,
    HDR10PLUS = 16,
    FOUR_K = 32,
    BL_TRACK = 64,
    BL_NOTCOMPAT = 128
};

extern int V3_flags;
extern unsigned HDR10_metadata[6];
extern bool isV3();
extern bool is4K();

static constexpr int MAX_PES_HEADER_LEN = 512;

class TSMuxer final : public AbstractMuxer
{
    typedef AbstractMuxer base_class;

   public:
    TSMuxer(MuxerManager* owner);
    ~TSMuxer() override;
    void intAddStream(const std::string& streamName, const std::string& codecName, int streamIndex,
                      const std::map<std::string, std::string>& params, AbstractStreamReader* codecReader) override;
    bool doFlush() override;
    bool close() override;

    int64_t getVBVLength() const { return m_vbvLen / 90; }
    void setNewStyleAudioPES(const bool val) { m_useNewStyleAudioPES = val; }
    void setM2TSMode(const bool val) { m_m2tsMode = val; }
    void setPCROnVideoPID(const bool val) { m_pcrOnVideo = val; }
    void setMaxBitrate(const int val) { m_cbrBitrate = val; }
    void setMinBitrate(const int val) { m_minBitrate = val; }
    void openDstFile() override;
    void setVBVBufferLen(int value);
    const PIDListMap& getPidList() const { return m_pmt.pidList; }
    std::vector<int64_t> getFirstPts() const;
    void alignPTS(TSMuxer* otherMuxer);
    std::vector<int64_t> getLastPts() const;
    const std::vector<uint32_t>& getMuxedPacketCnt() { return m_muxedPacketCnt; }
    size_t splitFileCnt() const { return m_fileNames.size(); }
    void setSplitDuration(const int64_t value) { m_splitDuration = value; }
    void setSplitSize(const int64_t value) { m_splitSize = value; }
    void parseMuxOpt(const std::string& opts) override;

    void setFileName(const std::string& fileName, FileFactory* fileFactory) override;
    std::string getFileNameByIdx(size_t idx);
    int getFirstFileNum() const;
    bool isInterleaveMode() const;
    std::vector<int32_t> getInterleaveInfo(size_t idx) const;
    bool isSubStream() const { return m_subMode; }

    void setPtsOffset(int64_t value);

   protected:
    bool muxPacket(AVPacket& avPacket) override;
    virtual void internalReset();
    void setMuxFormat(const std::string& format);
    bool isSplitPoint(const AVPacket& avPacket) const;
    bool blockFull() const;

   private:
    bool doFlush(uint64_t newPCR, int64_t pcrGAP);
    void flushTSFrame();
    int writeTSFrames(int pid, const uint8_t* buffer, int64_t len, bool priorityData, bool payloadStart);
    void writeSIT();
    void writePMT();
    void writePAT();
    void writeNullPackets(int cnt);
    void writeOutBuffer();
    void writeEmptyPacketWithPCR(int64_t pcrVal);
    void buildNULL();
    void buildPAT();
    void buildPMT();
    static void buildSIT();
    void addData(int pesStreamID, int pid, AVPacket& avPacket);
    void buildPesHeader(int pesStreamID, AVPacket& avPacket, int pid);
    void writePESPacket();
    void processM2TSPCR(int64_t pcrVal, int64_t pcrGAP);
    inline int calcM2tsFrameCnt() const;
    static void writeM2TSHeader(uint8_t* buffer, const uint64_t m2tsPCR)
    {
        const auto cur = reinterpret_cast<uint32_t*>(buffer);
        *cur = my_htonl(m2tsPCR & 0x3fffffff);
    }
    void writePATPMT(int64_t pcr, bool force = false);
    void writePCR(uint64_t newPCR);
    std::string getNextName(std::string curName) override;
    void writeEmptyPacketWithPCRTest(int64_t pcrVal);
    bool appendM2TSNullPacketToFile(uint64_t curFileSize, int counter, int* packetsWrited) const;
    int writeOutFile(const uint8_t* buffer, int len) const;

    void joinToMasterFile() override;
    void setSubMode(AbstractMuxer* mainMuxer, bool flushInterleavedBlock) override;
    void setMasterMode(AbstractMuxer* subMuxer, bool flushInterleavedBlock) override;

    AbstractOutputStream* getDstFile() const { return m_muxFile; }
    void flushTSBuffer();
    void finishFileBlock(uint64_t newPts, uint64_t newPCR, bool doChangeFile, bool recursive = true);
    void gotoNextFile(uint64_t newPts);

    AbstractOutputStream* m_muxFile;
    bool m_isExternalFile;

    int64_t m_fixed_pcr_offset;
    bool m_pcrOnVideo;
    int m_cbrBitrate;
    int m_minBitrate;
    int m_pcr_delta;    // how often write PCR
    int m_patPmtDelta;  // how often write PAT/PMT
    bool m_m2tsMode;
    int m_curFileNum;
    bool m_bluRayMode;
    bool m_hdmvDescriptors;
    uint64_t m_splitSize;
    uint64_t m_splitDuration;

    bool m_useNewStyleAudioPES;

    int64_t m_lastPESDTS;
    int64_t m_fullPesDTS;
    int64_t m_fullPesPTS;
    std::vector<std::pair<uint8_t*, int>>
        m_m2tsDelayBlocks;  // postpone M2TS PCR processing (fill previous data on next PCR after several PES packets)
    int m_prevM2TSPCROffset;
    int64_t m_prevM2TSPCR;
    int64_t m_endStreamDTS;
    int m_lastTSIndex;
    int m_lastPesLen;
    int m_pcrBits;
    std::vector<int64_t> m_lastPts;
    std::vector<int64_t> m_firstPts;

    struct StreamInfo
    {
        StreamInfo()
        {
            m_pts = m_dts = ULLONG_MAX;
            m_tsCnt = 0;
        }
        int64_t m_pts;
        int64_t m_dts;
        int m_tsCnt;
    };

    int64_t m_minDts;
    bool m_beforePCRDataWrited;
    std::map<int, int> m_extIndexToTSIndex;
    int m_videoTrackCnt;
    int m_DVvideoTrackCnt;
    int m_videoSecondTrackCnt;
    int m_audioTrackCnt;
    int m_secondaryAudioTrackCnt;
    int m_pgsTrackCnt;
    int64_t m_lastPCR;
    std::map<int, StreamInfo> m_streamInfo;
    uint64_t m_lastPMTPCR;
    uint8_t* m_outBuf;
    uint32_t m_outBufLen;
    int m_nullCnt;
    int m_pmtCnt;
    int m_patCnt;
    int m_sitCnt;
    uint32_t m_lastGopNullCnt;

    uint8_t m_pmtBuffer[4096];
    uint8_t m_patBuffer[TS_FRAME_SIZE];
    uint8_t m_nullBuffer[TS_FRAME_SIZE];
    TS_program_map_section m_pmt;
    TS_program_association_section m_pat;
    std::map<int, uint8_t> m_pesType;
    bool m_needTruncate;
    int64_t m_lastMuxedDts;
    MemoryBlock m_pesData;
    int m_pesPID;
    std::vector<uint32_t> m_muxedPacketCnt;
    bool m_pesIFrame;
    bool m_pesSpsPps;
    bool m_computeMuxStats;
    int64_t m_pmtFrames;
    uint64_t m_curFileStartPts;
    int64_t m_vbvLen;
    int m_mainStreamIndex;

    std::string m_outFileName;
    unsigned m_writeBlockSize;
    int m_frameSize;
    int64_t m_processedBlockSize;
    TSMuxer* m_sublingMuxer;
    std::vector<std::vector<int32_t>> m_interleaveInfo;  // ssif interliave info, should be written to CLPI
    bool m_masterMode;
    bool m_subMode;
    PriorityDataInfo m_priorityData;
    int64_t m_timeOffset;
    int64_t m_lastSITPCR;
    bool m_canSwithBlock;
    int64_t m_additionCLPISize;
    std::vector<std::string> m_fileNames;
#ifdef _DEBUG
    int64_t m_lastProcessedDts;
    int m_lastStreamIndex;
#endif
};

class TSMuxerFactory final : public AbstractMuxerFactory
{
   public:
    AbstractMuxer* newInstance(MuxerManager* owner) const override { return new TSMuxer(owner); }
};

#endif
