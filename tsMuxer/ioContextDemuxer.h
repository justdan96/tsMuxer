#ifndef IO_CONTEXT_DEMUXER_H_
#define IO_CONTEXT_DEMUXER_H_

#include <vector>

#include "BufferedReader.h"
#include "abstractDemuxer.h"
#include "bufferedReaderManager.h"

static constexpr int TRACKTYPE_PCM = 0x080;
static constexpr int TRACKTYPE_PGS = 0x090;
static constexpr int TRACKTYPE_SRT = 0x190;
static constexpr int TRACKTYPE_WAV = 0x180;

class ParsedTrackPrivData
{
   public:
    ParsedTrackPrivData(uint8_t* buff, int size) {}
    ParsedTrackPrivData() = default;
    virtual ~ParsedTrackPrivData() = default;

    virtual void setPrivData(uint8_t* buff, int size) {}
    virtual void extractData(AVPacket* pkt, uint8_t* buff, int size) = 0;
    virtual unsigned newBufferSize(uint8_t* buff, unsigned size) { return 0; }
};

enum class IOContextTrackType
{
    UNDEFINED = 0x0,
    VIDEO = 0x1,
    AUDIO = 0x2,
    COMPLEX = 0x3,
    LOGO = 0x10,
    SUBTITLE = 0x11,
    CONTROL = 0x20,
    DATA = 0x40
};

double av_int2dbl(uint64_t v);
float av_int2flt(uint32_t v);

struct Track
{
    Track()
        : num(0), uid(0), stream_index(0), codec_priv_size(0), flags(0)
    {
        name = codec_id = codec_name = nullptr;
        parsed_priv_data = nullptr;
        codec_priv = nullptr;
        memset(language, 0, sizeof(language));
        default_duration = 0;
        encodingAlgo = 0;
        type = IOContextTrackType::UNDEFINED;
    }

    virtual ~Track()
    {
        delete[] name;
        delete[] codec_id;
        delete[] codec_name;
        delete parsed_priv_data;
    }
    IOContextTrackType type;
    /* Unique track number and track ID. stream_index is the index that
     * the calling app uses for this track. */
    int64_t num;
    uint64_t uid;
    int stream_index;

    char* name;
    char language[4];

    char* codec_id;
    char* codec_name;

    unsigned char* codec_priv;
    int codec_priv_size;
    ParsedTrackPrivData* parsed_priv_data;
    uint64_t default_duration;

    uint32_t encodingAlgo;                  // compression algorithm
    std::vector<uint8_t> encodingAlgoPriv;  // compression parameters
    // MatroskaTrackFlags flags;
    int flags;
};

class IOContextDemuxer : public AbstractDemuxer
{
   public:
    IOContextDemuxer(BufferedReaderManager& readManager);
    ~IOContextDemuxer() override;
    void setFileIterator(FileNameIterator* itr) override;
    uint64_t getDemuxedSize() override;
    int getLastReadRez() override { return m_lastReadRez; }
    int64_t getProcessedBytes() const { return m_processedBytes; }

   protected:
    static constexpr int MAX_STREAMS = 64;
    Track* tracks[MAX_STREAMS];
    int num_tracks;

    BufferedReaderManager m_readManager;
    AbstractReader* m_bufferedReader;
    int m_readerID;
    int m_lastReadRez;
    uint8_t* m_curPos;
    uint8_t* m_bufEnd;
    bool m_isEOF;
    int64_t m_processedBytes;
    int64_t m_lastProcessedBytes;

    void skip_bytes(uint64_t size);
    unsigned get_buffer(uint8_t* binary, unsigned size);
    bool url_fseek(uint64_t offset);
    int64_t get_be64();
    unsigned int get_be32();
    int get_be16();
    int get_be24();
    int get_byte();

    unsigned int get_le16();
    unsigned int get_le24();
    unsigned int get_le32();
};

#endif
