#ifndef MOV_DEMUXER_H_
#define MOV_DEMUXER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "bufferedReaderManager.h"
#include "ioContextDemuxer.h"

class MovDemuxer final : public IOContextDemuxer
{
   public:
    MovDemuxer(const BufferedReaderManager& readManager);
    ~MovDemuxer() override { readClose(); }
    void openFile(const std::string& streamName) override;
    void readClose() override;
    int simpleDemuxBlock(DemuxedData& demuxedData, const PIDSet& acceptedPIDs, int64_t& discardSize) override;
    void getTrackList(std::map<int32_t, TrackInfo>& trackList) override;
    int64_t getTrackDelay(const int32_t pid) override
    {
        return (m_firstTimecode.find(pid) != m_firstTimecode.end()) ? m_firstTimecode[pid] : 0;
    }
    double getTrackFps(uint32_t trackId) override;
    static int readPacket(AVPacket&) { return 0; }
    void setFileIterator(FileNameIterator* itr) override;
    bool isPidFilterSupported() const override { return true; }
    int64_t getFileDurationNano() const override;

   private:
    struct MOVParseTableEntry;
    struct MOVAtom
    {
        MOVAtom() : type(0), offset(0), size(0) {}
        MOVAtom(const uint32_t _type, const int64_t _offset, const int64_t _size)
            : type(_type), offset(_offset), size(_size)
        {
        }
        uint32_t type;
        int64_t offset;
        int64_t size;  // total size (excluding the size and type fields)
    };

    struct MOVFragment
    {
        int track_id;
        int64_t base_data_offset;
        int64_t moof_offset;
        unsigned stsd_id;
        unsigned duration;
        unsigned size;
        unsigned flags;
    };

    struct MOVTrackExt
    {
        int track_id;
        unsigned stsd_id;
        unsigned duration;
        unsigned size;
        unsigned flags;
    };

    int found_moov;  // when both 'moov' and 'mdat' sections has been found
    bool found_moof;
    int64_t m_mdat_pos;
    int64_t m_mdat_size;
    int64_t m_fileSize;
    uint32_t m_timescale;
    std::map<int32_t, int64_t> m_firstTimecode;
    // List<chunk offset, chunk size>
    std::vector<std::pair<int64_t, int64_t>> m_mdat_data;
    int itunes_metadata;  ///< metadata are itunes style
    int64_t moof_offset;
    std::map<std::string, std::string> metaData;
    MOVFragment fragment;  ///< current fragment in moof atom
    std::vector<MOVTrackExt> trex_data;
    int64_t fileDuration;
    int isom;
    std::vector<std::pair<int64_t, int64_t>> chunks;
    size_t m_curChunk;
    AVPacket m_deliveredPacket;
    std::vector<uint8_t> m_tmpChunkBuffer;
    bool m_firstDemux;
    FileNameIterator* m_fileIterator;
    std::string m_fileName;
    MemoryBlock m_filterBuffer;
    int64_t m_firstHeaderSize;

    static const MOVParseTableEntry mov_default_parse_table[];

    void readHeaders();
    void buildIndex();

    int mov_read_default(MOVAtom atom);
    int mov_read_extradata(MOVAtom atom);
    int mov_read_mdat(MOVAtom atom);
    int mov_read_smi(MOVAtom atom);
    int mov_read_stss(MOVAtom atom);
    int mov_read_stsz(MOVAtom atom);
    int mov_read_stts(MOVAtom atom);
    int mov_read_tkhd(MOVAtom atom);
    int mov_read_tfhd(MOVAtom atom);
    int mov_read_trak(MOVAtom atom);
    int mov_read_trex(MOVAtom atom);
    int mov_read_trkn(MOVAtom atom);
    int mov_read_trun(MOVAtom atom);
    int mov_read_wide(MOVAtom atom);
    int mov_read_cmov(MOVAtom atom);
    int mov_read_udta_string(MOVAtom atom);
    int mov_read_moof(MOVAtom atom);
    int mov_read_moov(MOVAtom atom);
    int mov_read_mvhd(MOVAtom atom);
    int mov_read_mdhd(MOVAtom atom);
    int mov_read_stsd(MOVAtom atom);
    int mov_read_stco(MOVAtom atom);
    int mov_read_glbl(MOVAtom atom);
    int mov_read_ftyp(MOVAtom atom);
    int mov_read_hdlr(MOVAtom atom);
    int mov_read_esds(MOVAtom atom);
    int mov_read_dref(MOVAtom atom);
    int mov_read_stsc(MOVAtom atom);
    int mov_read_wave(MOVAtom atom);
    int mov_read_ctts(MOVAtom atom);
    int mov_read_elst(MOVAtom atom);

    // int mov_read_enda(MOVAtom atom);
    // int mov_read_ilst(MOVAtom atom);
    // int mov_read_meta(MOVAtom atom);
    // int mov_read_pasp(MOVAtom atom);
    int mp4_read_descr(int* tag);
};

#endif
