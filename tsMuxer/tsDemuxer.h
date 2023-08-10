#ifndef TS_DEMUXER_H
#define TS_DEMUXER_H

#include <map>
#include <set>
#include <string>

#include "BufferedReader.h"
#include "aac.h"
#include "abstractDemuxer.h"
#include "bufferedReaderManager.h"
#include "tsPacket.h"

// typedef StreamReaderMap std::map<int, AbstractStreamReader*>;
class TSDemuxer : public AbstractDemuxer
{
   public:
    TSDemuxer(BufferedReaderManager& readManager, const char* streamName);
    ~TSDemuxer() override;
    void openFile(const std::string& streamName) override;
    void readClose() override;
    uint64_t getDemuxedSize() override;
    int simpleDemuxBlock(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, int64_t& discardSize) override;
    void getTrackList(std::map<uint32_t, TrackInfo>& trackList) override;
    int getLastReadRez() override { return m_lastReadRez; };
    void setFileIterator(FileNameIterator* itr) override;
    int64_t getTrackDelay(const uint32_t pid) override
    {
        if (m_firstPtsTime.find(pid) != m_firstPtsTime.end())
        {
            const int64_t clockTicks = m_firstPtsTime[pid] - (m_firstVideoPTS != -1 ? m_firstVideoPTS : m_firstPTS);
            return static_cast<int64_t>(clockTicks / 90.0 + (clockTicks >= 0 ? 0.5 : -0.5));  // convert to ms
        }

        return 0;
    }
    void setMPLSInfo(const std::vector<MPLSPlayItem>& mplsInfo) { m_mplsInfo = mplsInfo; }
    int64_t getFileDurationNano() const override;

   private:
    bool mvcContinueExpected() const;

   private:
    int64_t m_firstPCRTime;
    bool m_m2tsHdrDiscarded;
    int m_lastReadRez;
    bool m_m2tsMode;
    int m_scale;
    int m_nptPos;
    BufferedReaderManager& m_readManager;
    std::string m_streamName;
    std::string m_streamNameLow;
    std::map<int, int64_t> m_lastPesPts;
    std::map<int, int64_t> m_lastPesDts;
    std::map<int, int64_t> m_firstPtsTime;
    std::map<int, int64_t> m_firstDtsTime;
    std::map<int, int64_t> m_fullPtsTime;
    std::map<int, int64_t> m_fullDtsTime;
    AbstractReader* m_bufferedReader;
    int m_readerID;
    uint8_t* m_curPos;
    int m_pmtPid;
    bool m_codecReady;
    bool m_firstCall;
    uint64_t m_readCnt;
    uint64_t m_dataProcessed;
    bool m_notificated;
    TS_program_map_section m_pmt;
    uint8_t m_tmpBuffer[TS_FRAME_SIZE + 4];
    int64_t m_tmpBufferLen;
    // int64_t m_firstDTS;
    int64_t m_firstPTS;
    int64_t m_lastPTS;
    int64_t m_prevFileLen;
    uint32_t m_curFileNum;
    int64_t m_firstVideoPTS;
    int64_t m_lastVideoPTS;
    int64_t m_lastVideoDTS;
    int64_t m_videoDtsGap;
    std::vector<MPLSPlayItem> m_mplsInfo;
    int64_t m_lastPCRVal;
    bool m_nonMVCVideoFound;

    // cache to improve speed
    uint8_t m_acceptedPidCache[8192];
    bool m_firstDemuxCall;

    static bool isVideoPID(StreamType streamType);
    bool checkForRealM2ts(uint8_t* buffer, uint8_t* end) const;
};

#endif
