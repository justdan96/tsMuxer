#ifndef __IO_CONTEXT_DEMUXER_H
#define __IO_CONTEXT_DEMUXER_H

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "BufferedReader.h"
#include "abstractDemuxer.h"
#include "bufferedReaderManager.h"

const static int TRACKTYPE_PCM = 0x080;
const static int TRACKTYPE_PGS = 0x090;
const static int TRACKTYPE_SRT = 0x190;
const static int TRACKTYPE_WAV = 0x180;

class ParsedTrackPrivData
{
   public:
    ParsedTrackPrivData(uint8_t* buff, int size) {}
    ParsedTrackPrivData() {}
    virtual void setPrivData(uint8_t* buff, int size) {}
    virtual ~ParsedTrackPrivData() {}
    virtual void extractData(AVPacket* pkt, uint8_t* buff, int size) = 0;
    virtual int newBufferSize(uint8_t* buff, int size) { return 0; }
};

enum class IOContextTrackType
{
    TRACK_TYPE_UNDEFINED = 0x0,
    TRACK_TYPE_VIDEO = 0x1,
    TRACK_TYPE_AUDIO = 0x2,
    TRACK_TYPE_COMPLEX = 0x3,
    TRACK_TYPE_LOGO = 0x10,
    TRACK_TYPE_SUBTITLE = 0x11,
    TRACK_TYPE_CONTROL = 0x20,
    TRACK_TYPE_DATA = 0x40
};

double av_int2dbl(int64_t v);
float av_int2flt(int32_t v);

struct Track
{
    Track()
    {
        name = codec_id = codec_name = 0;
        parsed_priv_data = 0;
        codec_priv = 0;
        memset(language, 0, sizeof(language));
        default_duration = 0;
        encodingAlgo = 0;
        type = IOContextTrackType::TRACK_TYPE_UNDEFINED;
    }
    ~Track()
    {
        delete[] name;
        delete[] codec_id;
        delete[] codec_name;
        delete parsed_priv_data;
    }
    IOContextTrackType type;
    /* Unique track number and track ID. stream_index is the index that
     * the calling app uses for this track. */
    uint32_t num;
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
    IOContextDemuxer(const BufferedReaderManager& readManager);
    ~IOContextDemuxer() override;
    void setFileIterator(FileNameIterator* itr) override;
    uint64_t getDemuxedSize() override;
    int getLastReadRez() override { return m_lastReadRez; };
    int64_t getProcessedBytes() { return m_processedBytes; }

   protected:
    const static int MAX_STREAMS = 64;
    Track* tracks[MAX_STREAMS];
    int num_tracks;

    const BufferedReaderManager& m_readManager;
    AbstractReader* m_bufferedReader;
    int m_readerID;
    int m_lastReadRez;
    uint8_t* m_curPos;
    uint8_t* m_bufEnd;
    bool m_isEOF;
    uint64_t m_processedBytes;
    uint64_t m_lastProcessedBytes;

    void skip_bytes(uint64_t size);
    uint32_t get_buffer(uint8_t* binary, int size);
    bool url_fseek(int64_t offset);
    uint64_t get_be64();
    unsigned int get_be32();
    unsigned int get_be16();
    unsigned int get_be24();
    int get_byte();

    unsigned int get_le16();
    unsigned int get_le24();
    unsigned int get_le32();
};

#endif
