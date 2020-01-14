#include "matroskaDemuxer.h"

#include <math.h>
#include <types/types.h>

#include <algorithm>

#include "abstractDemuxer.h"
#include "abstractStreamReader.h"
#include "avPacket.h"
#include "bitStream.h"
#include "limits.h"
#include "subTrackFilter.h"
#include "vodCoreException.h"

extern "C"
{
#include "zlib.h"
}

typedef uint64_t offset_t;

int64_t AV_NOPTS_VALUE = int64_t(0x8000000000000000ull);

const static int PKT_FLAG_KEY = 1;
const static int AVERROR_INVALIDDATA = -1;

static const int COMPRESSION_STRIP_HEADERS = 3;
static const int COMPRESSION_ZLIB = 0;

#define AV_RL32(x) \
    ((((uint8_t *)(x))[3] << 24) | (((uint8_t *)(x))[2] << 16) | (((uint8_t *)(x))[1] << 8) | ((uint8_t *)(x))[0])

#define MAX(a, b) (a > b ? a : b)
#define min(a, b) (((a) < (b)) ? (a) : (b))

const static int MAX_TRACK_SIZE =
    (MAX(MAX(sizeof(MatroskaVideoTrack), sizeof(MatroskaAudioTrack)), sizeof(MatroskaSubtitleTrack)));

enum AVDiscard
{
    /* We leave some space between them for extensions (drop some
     * keyframes for intra-only or drop just some bidir frames). */
    AVDISCARD_NONE = -16,   ///< discard nothing
    AVDISCARD_DEFAULT = 0,  ///< discard useless packets like 0 size packets in avi
    AVDISCARD_NONREF = 8,   ///< discard all non reference
    AVDISCARD_BIDIR = 16,   ///< discard all bidirectional frames
    AVDISCARD_NONKEY = 32,  ///< discard all frames except keyframes
    AVDISCARD_ALL = 48,     ///< discard allle
};

int MatroskaDemuxer::matroska_parse_index()
{
    int res = 0;
    uint32_t id;
    MatroskaDemuxIndex idx;

    // return 0;

    // av_log(matroska->ctx, AV_LOG_DEBUG, "parsing index...\n");

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        /* one single index entry ('point') */
        case MATROSKA_ID_POINTENTRY:
            if ((res = ebml_read_master(&id)) < 0)
                break;

            /* in the end, we hope to fill one entry with a
             * timestamp, a file position and a tracknum */
            idx.pos = (uint64_t)-1;
            idx.time = (uint64_t)-1;
            idx.track = (uint16_t)-1;

            while (res == 0)
            {
                if (!(id = ebml_peek_id(&level_up)))
                {
                    res = -BufferedReader::DATA_EOF;
                    break;
                }
                else if (level_up)
                {
                    level_up--;
                    break;
                }

                switch (id)
                {
                /* one single index entry ('point') */
                case MATROSKA_ID_CUETIME:
                {
                    uint64_t time;
                    if ((res = ebml_read_uint(&id, &time)) < 0)
                        break;
                    idx.time = time * time_scale;
                    break;
                }

                /* position in the file + track to which it
                 * belongs */
                case MATROSKA_ID_CUETRACKPOSITION:
                    if ((res = ebml_read_master(&id)) < 0)
                        break;

                    while (res == 0)
                    {
                        if (!(id = ebml_peek_id(&level_up)))
                        {
                            res = -BufferedReader::DATA_EOF;
                            break;
                        }
                        else if (level_up)
                        {
                            level_up--;
                            break;
                        }

                        switch (id)
                        {
                        /* track number */
                        case MATROSKA_ID_CUETRACK:
                        {
                            uint64_t num;
                            if ((res = ebml_read_uint(&id, &num)) < 0)
                                break;
                            idx.track = num;
                            break;
                        }

                            /* position in file */
                        case MATROSKA_ID_CUECLUSTERPOSITION:
                        {
                            uint64_t num;
                            if ((res = ebml_read_uint(&id, &num)) < 0)
                                break;
                            idx.pos = num + segment_start;
                            break;
                        }

                        default:
                            LTRACE(LT_INFO, 0, "Unknown entry " << id << " in CuesTrackPositions");
                            /* fall-through */

                        case EBML_ID_VOID:
                            res = ebml_read_skip();
                            break;
                        }

                        if (level_up)
                        {
                            level_up--;
                            break;
                        }
                    }

                    break;

                default:
                    LTRACE(LT_INFO, 0, "Unknown entry " << id << " in cuespoint index");
                    /* fall-through */

                case EBML_ID_VOID:
                    res = ebml_read_skip();
                    break;
                }

                if (level_up)
                {
                    level_up--;
                    break;
                }
            }

            /* so let's see if we got what we wanted */
            if (idx.pos != (uint64_t)-1 && idx.time != (uint64_t)-1 && idx.track != (uint16_t)-1)
            {
                indexes.push_back(idx);
            }
            break;

        default:
            LTRACE(LT_INFO, 0, "Unknown entry " << id << " in cues header");
            /* fall-through */

        case EBML_ID_VOID:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    return res;
}

MatroskaDemuxer::MatroskaDemuxer(const BufferedReaderManager &readManager) : IOContextDemuxer(readManager)
{
    m_lastDeliveryPacket = 0;
    num_levels = 0;
    level_up = 0;
    peek_id = 0;
    done = false;
    num_streams = 0;
    segment_start = 0;
    time_scale = 0;
    index_parsed = false;
    metadata_parsed = false;
    writing_app = 0;
    muxing_app = 0;
}

int MatroskaDemuxer::ebml_read_ascii(uint32_t *id, char **str)
{
    int size, res;
    uint64_t rlength;

    if ((res = ebml_read_element_id(id, NULL)) < 0 || (res = ebml_read_element_length(&rlength)) < 0)
        return res;
    size = rlength;

    // ebml strings are usually not 0-terminated, so we allocate one
    // byte more, read the string and NULL-terminate it ourselves.
    *str = new char[size + 1];
    if (size < 0 || *str == 0)
    {
        THROW(ERR_MATROSKA_PARSE, "Memory allocation failed");
    }
    if (get_buffer((uint8_t *)*str, size) != size)
    {
        offset_t pos = m_processedBytes;
        THROW(ERR_MATROSKA_PARSE, "Read error at pos. " << pos);
        return -BufferedReader::DATA_EOF;
    }
    (*str)[size] = '\0';
    return 0;
}

int MatroskaDemuxer::ebml_read_header(char **doctype, int *version)
{
    uint32_t id;
    int level_up, res = 0;

    /* default init */
    if (doctype)
        *doctype = NULL;
    if (version)
        *version = 1;

    if (!(id = ebml_peek_id(&level_up)) || level_up != 0 || id != EBML_ID_HEADER)
    {
        THROW(ERR_MATROSKA_PARSE, "This is not an EBML file (id=" << id << "/" << EBML_ID_HEADER);
    }
    if ((res = ebml_read_master(&id)) < 0)
        return res;

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
            return -BufferedReader::DATA_EOF;

        /* end-of-header */
        if (level_up)
            break;

        switch (id)
        {
        /* is our read version uptodate? */
        case EBML_ID_EBMLREADVERSION:
        {
            uint64_t num;

            if ((res = ebml_read_uint(&id, &num)) < 0)
                return res;
            if (num > EBML_VERSION)
            {
                THROW(ERR_MATROSKA_PARSE, "EBML version " << num << " > " << EBML_VERSION << " is not supported");
                return AVERROR_INVALIDDATA;
            }
            break;
        }

        /* we only handle 8 byte lengths at max */
        case EBML_ID_EBMLMAXSIZELENGTH:
        {
            uint64_t num;

            if ((res = ebml_read_uint(&id, &num)) < 0)
                return res;
            if (num > sizeof(uint64_t))
            {
                THROW(ERR_MATROSKA_PARSE,
                      "Integers of size " << num << " (> " << sizeof(uint64_t) << ") not supported");
            }
            break;
        }

        /* we handle 4 byte IDs at max */
        case EBML_ID_EBMLMAXIDLENGTH:
        {
            uint64_t num;

            if ((res = ebml_read_uint(&id, &num)) < 0)
                return res;
            if (num > sizeof(uint32_t))
            {
                THROW(ERR_MATROSKA_PARSE, "IDs of size " << num << " (> " << sizeof(uint32_t) << ") not supported");
            }
            break;
        }

        case EBML_ID_DOCTYPE:
        {
            char *text;

            if ((res = ebml_read_ascii(&id, &text)) < 0)
                return res;
            if (doctype)
            {
                if (*doctype)
                    delete[] doctype;
                *doctype = text;
            }
            else
                delete[] text;
            break;
        }

        case EBML_ID_DOCTYPEREADVERSION:
        {
            uint64_t num;

            if ((res = ebml_read_uint(&id, &num)) < 0)
                return res;
            if (version)
                *version = num;
            break;
        }

        default:
            LTRACE(LT_INFO, 0, "Unknown data type " << id << " in EBML header");
            /* pass-through */

        case EBML_ID_VOID:
        /* we ignore these two, as they don't tell us anything we
         * care about */
        case EBML_ID_EBMLVERSION:
        case EBML_ID_DOCTYPEVERSION:
            res = ebml_read_skip();
            break;
        }
    }

    return 0;
}

void MatroskaDemuxer::matroska_queue_packet(AVPacket *pkt)
{
    // packets = av_realloc(matroska->packets, (num_packets + 1) * sizeof(AVPacket *));
    // packets[matroska->num_packets] = pkt;
    // num_packets++;
    packets.push(pkt);
}

int MatroskaDemuxer::rv_offset(uint8_t *data, int slice, int slices)
{
    return AV_RL32(data + 8 * slice + 4) + 8 * slices;
}

/*
 * Read signed/unsigned "EBML" numbers.
 * Return: number of bytes processed, < 0 on error.
 * XXX: use ebml_read_num().
 */

int MatroskaDemuxer::matroska_find_track_by_num(int num)
{
    int i;
    for (i = 0; i < num_tracks; i++)
        if (tracks[i]->num == num)
            return i;
    return -1;
}

int MatroskaDemuxer::matroska_ebmlnum_uint(uint8_t *data, uint32_t size, uint64_t *num)
{
    int len_mask = 0x80, read = 1, n = 1, num_ffs = 0;
    uint64_t total;

    if (size <= 0)
        return AVERROR_INVALIDDATA;

    total = data[0];
    while (read <= 8 && !(total & len_mask))
    {
        read++;
        len_mask >>= 1;
    }
    if (read > 8)
        return AVERROR_INVALIDDATA;

    if ((total &= (len_mask - 1)) == len_mask - 1)
        num_ffs++;
    if (size < read)
        return AVERROR_INVALIDDATA;
    while (n < read)
    {
        if (data[n] == 0xff)
            num_ffs++;
        total = (total << 8) | data[n];
        n++;
    }

    if (read == num_ffs)
        *num = (uint64_t)-1;
    else
        *num = total;

    return read;
}

int MatroskaDemuxer::matroska_ebmlnum_sint(uint8_t *data, uint32_t size, int64_t *num)
{
    uint64_t unum;
    int res;

    /* read as unsigned number first */
    if ((res = matroska_ebmlnum_uint(data, size, &unum)) < 0)
        return res;

    /* make signed (weird way) */
    if (unum == (uint64_t)-1)
        *num = LLONG_MAX;
    else
        *num = unum - ((1LL << ((7 * res) - 1)) - 1);

    return res;
}

int MatroskaDemuxer::matroska_deliver_packet(AVPacket *&avPacket)
{
    if (packets.size() > 0)
    {
        /*
        avPacket.data = packets[0]->data;
        avPacket.size = packets[0]->size;
        avPacket.stream_index = packets[0]->stream_index;
        if (packets.size() > 0) {
                //memmove(&packets[0], &packets[1], packets.size()*sizeof(AVPacket));
                //packets.er
        }
        */
        avPacket = packets.front();
        packets.pop();
        return 0;
    }

    return -1;
}

int MatroskaDemuxer::ebml_read_sint(uint32_t *id, int64_t *num)
{
    int size, n = 1, negative = 0, res;
    uint64_t rlength;

    if ((res = ebml_read_element_id(id, NULL)) < 0 || (res = ebml_read_element_length(&rlength)) < 0)
        return res;
    size = rlength;
    if (size < 1 || size > 8)
    {
        offset_t pos = m_processedBytes;
        THROW(ERR_MATROSKA_PARSE, "Invalid sint element size " << size << " at position " << pos);
    }
    if ((*num = get_byte()) & 0x80)
    {
        negative = 1;
        *num &= ~0x80;
    }
    while (n++ < size) *num = (*num << 8) | get_byte();

    /* make signed */
    if (negative)
        *num = *num - (1LL << ((8 * size) - 1));

    return 0;
}

void MatroskaDemuxer::decompressData(const uint8_t *data, int size)
{
    z_stream zstream;
    memset(&zstream, 0, sizeof(zstream));

    int err = inflateInit(&zstream);
    if (err != Z_OK)
        return;

    zstream.avail_in = size;
    zstream.next_in = (Bytef *)data;
    int curSize = size;
    do
    {
        curSize *= 3;
        m_tmpBuffer.resize(curSize);

        zstream.avail_out = m_tmpBuffer.size() - zstream.total_out;
        zstream.next_out = m_tmpBuffer.data() + zstream.total_out;
        err = inflate(&zstream, Z_NO_FLUSH);
    } while (err == Z_OK && curSize < 10000000);

    m_tmpBuffer.resize(zstream.total_out);
    inflateEnd(&zstream);
    if (err != Z_STREAM_END)
        m_tmpBuffer.clear();
}

int MatroskaDemuxer::matroska_parse_block(uint8_t *data, int size, int64_t pos, uint64_t cluster_time,
                                          uint64_t duration, int is_keyframe, int is_bframe)
{
    int res = 0;
    int track;
    // AVStream *st;
    AVPacket *pkt;
    uint8_t *origdata = data;
    int16_t block_time;
    uint32_t *lace_size = NULL;
    int n, flags, laces = 0;
    uint64_t num;

    /* first byte(s): tracknum */
    if ((n = matroska_ebmlnum_uint(data, size, &num)) < 0)
    {
        LTRACE(LT_ERROR, 0, "EBML block data error");
        delete[] origdata;
        return res;
    }
    data += n;
    size -= n;

    /* fetch track from num */
    track = matroska_find_track_by_num(num);
    if (size <= 3 || track < 0 || track >= num_tracks)
    {
        LTRACE(LT_INFO, 0, "Invalid stream " << track << " or size " << size);
        delete[] origdata;
        return res;
    }
    if (tracks[track]->stream_index < 0)
        return res;
    /* block_time (relative to cluster time) */
    block_time = (int16_t)AV_RB16(data);
    data += 2;
    flags = *data++;
    size -= 3;
    if (is_keyframe == -1)
        is_keyframe = flags & 0x80 ? PKT_FLAG_KEY : 0;

    switch ((flags & 0x06) >> 1)
    {
    case 0x0: /* no lacing */
        laces = 1;
        lace_size = (uint32_t *)new uint8_t[sizeof(int)];
        lace_size[0] = size;
        break;

    case 0x1: /* xiph lacing */
    case 0x2: /* fixed-size lacing */
    case 0x3: /* EBML lacing */
        if (size == 0)
        {
            res = -1;
            break;
        }
        laces = (*data) + 1;
        data += 1;
        size -= 1;
        lace_size = (uint32_t *)new uint8_t[laces * sizeof(int)];
        memset(lace_size, 0, laces * sizeof(int));

        switch ((flags & 0x06) >> 1)
        {
        case 0x1: /* xiph lacing */
        {
            uint8_t temp;
            uint32_t total = 0;
            for (n = 0; res == 0 && n < laces - 1; n++)
            {
                while (1)
                {
                    if (size == 0)
                    {
                        res = -1;
                        break;
                    }
                    temp = *data;
                    lace_size[n] += temp;
                    data += 1;
                    size -= 1;
                    if (temp != 0xff)
                        break;
                }
                total += lace_size[n];
            }
            lace_size[n] = size - total;
            break;
        }

        case 0x2: /* fixed-size lacing */
            for (n = 0; n < laces; n++) lace_size[n] = size / laces;
            break;

        case 0x3: /* EBML lacing */
        {
            uint32_t total;
            n = matroska_ebmlnum_uint(data, size, &num);
            if (n < 0)
            {
                LTRACE(LT_INFO, 0, "EBML block data error");
                break;
            }
            data += n;
            size -= n;
            total = lace_size[0] = num;
            for (n = 1; res == 0 && n < laces - 1; n++)
            {
                int64_t snum;
                int r;
                r = matroska_ebmlnum_sint(data, size, &snum);
                if (r < 0)
                {
                    LTRACE(LT_INFO, 0, "EBML block data error");
                    break;
                }
                data += r;
                size -= r;
                lace_size[n] = lace_size[n - 1] + snum;
                total += lace_size[n];
            }
            lace_size[n] = size - total;
            break;
        }
        }
        break;
    }

    if (res == 0)
    {
        int real_v = tracks[track]->flags & MATROSKA_TRACK_REAL_V;
        uint64_t timecode = AV_NOPTS_VALUE;

        if (cluster_time != (uint64_t)-1 && (block_time >= 0 || cluster_time >= -block_time))
            timecode = cluster_time + block_time;

        for (n = 0; n < laces; n++)
        {
            int slice, slices = 1;

            if (real_v)
            {
                slices = *data++ + 1;
                lace_size[n]--;
            }

            for (slice = 0; slice < slices; slice++)
            {
                int slice_size, slice_offset = 0;
                if (real_v)
                    slice_offset = rv_offset(data, slice, slices);
                if (slice + 1 == slices)
                    slice_size = lace_size[n] - slice_offset;
                else
                    slice_size = rv_offset(data, slice + 1, slices) - slice_offset;

                pkt = new AVPacket();
                pkt->data = 0;
                pkt->size = 0;
                pkt->pts = timecode * 1000000ll;  // our AvPacket in nanoseconds
                pkt->pos = pos;
                pkt->duration = duration * 1000000ll;

                pkt->stream_index = track + 1;  // tracks[track]->stream_index;

                int offset = 0;
                uint8_t *curPtr = data + slice_offset;
                m_tmpBuffer.clear();
                if (tracks[track]->encodingAlgo == COMPRESSION_STRIP_HEADERS)
                {
                    offset = tracks[track]->encodingAlgoPriv.size();
                    if (offset)
                    {
                        curPtr -= offset;
                        m_tmpBuffer.append(curPtr, offset);  // save data
                        memcpy(curPtr, &tracks[track]->encodingAlgoPriv[0],
                               offset);  // place extra header direct to data
                    }
                }
                else if (tracks[track]->encodingAlgo == COMPRESSION_ZLIB)
                {
                    decompressData(curPtr, slice_size);
                    curPtr = m_tmpBuffer.data();
                    slice_size = m_tmpBuffer.size();
                }

                if (tracks[track]->parsed_priv_data != 0)
                {
                    tracks[track]->parsed_priv_data->extractData(pkt, curPtr, slice_size + offset);
                }
                else if (slice_size + offset > 0)
                {
                    pkt->data = new uint8_t[slice_size + offset];
                    pkt->size = slice_size + offset;
                    memcpy(pkt->data, curPtr, slice_size + offset);
                }
                if (offset)
                    memcpy(curPtr, m_tmpBuffer.data(), offset);  // restore data

                if (n == 0)
                    pkt->flags = is_keyframe;

                matroska_queue_packet(pkt);

                if (timecode != AV_NOPTS_VALUE)
                    timecode = duration ? timecode + duration : AV_NOPTS_VALUE;
            }
            data += lace_size[n];
        }
    }

    delete[] lace_size;
    delete[] origdata;
    return res;
}

int MatroskaDemuxer::matroska_parse_blockgroup(uint64_t cluster_time)
{
    int res = 0;
    uint32_t id;
    int is_bframe = 0;
    int is_keyframe = PKT_FLAG_KEY;
    int last_num_packets = packets.size();
    uint64_t duration = AV_NOPTS_VALUE;
    uint8_t *data;
    int size = 0;
    int64_t pos = 0;

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        /* one block inside the group. Note, block parsing is one
         * of the harder things, so this code is a bit complicated.
         * See http://www.matroska.org/ for documentation. */
        case MATROSKA_ID_BLOCK:
        {
            pos = m_processedBytes;
            res = ebml_read_binary(&id, &data, &size);
            break;
        }

        case MATROSKA_ID_BLOCKDURATION:
        {
            if ((res = ebml_read_uint(&id, &duration)) < 0)
                break;
            break;
        }

        case MATROSKA_ID_BLOCKREFERENCE:
        {
            int64_t num;
            /* We've found a reference, so not even the first frame in
             * the lace is a key frame. */
            is_keyframe = 0;
            if (last_num_packets != packets.size())
                packets.back()->flags = 0;
            if ((res = ebml_read_sint(&id, &num)) < 0)
                break;
            if (num > 0)
                is_bframe = 1;
            break;
        }

        default:
            LTRACE(LT_INFO, 0, "Unknown entry " << id << " in blockgroup data");
            /* fall-through */

        case EBML_ID_VOID:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    if (res)
        return res;

    if (size > 0)
        res = matroska_parse_block(data, size, pos, cluster_time, duration, is_keyframe, is_bframe);

    return res;
}

/*
 * Read the next element as an unsigned int.
 * 0 is success, < 0 is failure.
 */

int MatroskaDemuxer::ebml_read_uint(uint32_t *id, uint64_t *num)
{
    int n = 0, size, res;
    uint64_t rlength;

    if ((res = ebml_read_element_id(id, NULL)) < 0 || (res = ebml_read_element_length(&rlength)) < 0)
        return res;
    size = rlength;
    if (size < 1 || size > 8)
    {
        THROW(ERR_MATROSKA_PARSE, "Invalid uint element size " << size << " at position " << m_processedBytes);
    }

    /* big-endian ordening; build up number */
    *num = 0;
    while (n++ < size) *num = (*num << 8) | get_byte();
    return 0;
}

/*
 * Read: the element content data ID.
 * Return: the number of bytes read or < 0 on error.
 */

int MatroskaDemuxer::ebml_read_element_id(uint32_t *id, int *level_up)
{
    int read;
    uint64_t total;

    /* if we re-call this, use our cached ID */
    if (peek_id != 0)
    {
        if (level_up)
            *level_up = 0;
        *id = peek_id;
        return 0;
    }

    /* read out the "EBML number", include tag in ID */
    if ((read = ebml_read_num(4, &total)) < 0)
        return read;
    *id = peek_id = total | (1 << (read * 7));

    /* level tracking */
    if (level_up)
        *level_up = ebml_read_element_level_up();

    return read;
}

/*
 * Skip the next element.
 * 0 is success, -1 is failure.
 */

int MatroskaDemuxer::ebml_read_skip()
{
    uint32_t id;
    uint64_t length;
    int res;

    if ((res = ebml_read_element_id(&id, NULL)) < 0 || (res = ebml_read_element_length(&length)) < 0)
        return res;
    skip_bytes(length);
    return 0;
}

/*
 * Read the next element, but only the header. The contents
 * are supposed to be sub-elements which can be read separately.
 * 0 is success, < 0 is failure.
 */

int MatroskaDemuxer::ebml_read_master(uint32_t *id)
{
    uint64_t length;
    MatroskaLevel *level;
    int res;

    if ((res = ebml_read_element_id(id, NULL)) < 0 || (res = ebml_read_element_length(&length)) < 0)
        return res;

    /* protect... (Heaven forbids that the '>' is true) */
    if (num_levels >= EBML_MAX_DEPTH)
    {
        THROW(ERR_MATROSKA_PARSE, "File moves beyond max. allowed depth (" << EBML_MAX_DEPTH << ")");
    }

    /* remember level */
    level = &levels[num_levels++];
    level->start = m_processedBytes;
    level->length = length;
    return 0;
}

/*
 * Read: element content length.
 * Return: the number of bytes read or < 0 on error.
 */

int MatroskaDemuxer::ebml_read_element_length(uint64_t *length)
{
    /* clear cache since we're now beyond that data point */
    peek_id = 0;

    /* read out the "EBML number", include tag in ID */
    return ebml_read_num(8, length);
}

int MatroskaDemuxer::ebml_read_binary(uint32_t *id, uint8_t **binary, int *size)
{
    uint64_t rlength;
    int res;

    if ((res = ebml_read_element_id(id, NULL)) < 0 || (res = ebml_read_element_length(&rlength)) < 0)
        return res;
    *size = rlength;

    *binary = new uint8_t[*size];
    if (!(*binary))
    {
        THROW(ERR_COMMON_MEMORY, "Memory allocation error");
    }

    if (get_buffer(*binary, *size) != *size)
    {
        THROW(ERR_MATROSKA_PARSE, "Matroska parser: read error at pos " << m_processedBytes);
    }
    return 0;
}

int MatroskaDemuxer::matroska_parse_cluster()
{
    int res = 0;
    uint32_t id;
    uint64_t cluster_time = 0;
    uint8_t *data;
    int64_t pos;
    int size;

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        /* cluster timecode */
        case MATROSKA_ID_CLUSTERTIMECODE:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            cluster_time = num;
            break;
        }

            /* a group of blocks inside a cluster */
        case MATROSKA_ID_BLOCKGROUP:
            if ((res = ebml_read_master(&id)) < 0)
                break;
            res = matroska_parse_blockgroup(cluster_time);
            break;

        case MATROSKA_ID_SIMPLEBLOCK:
            pos = m_processedBytes;
            res = ebml_read_binary(&id, &data, &size);
            if (res == 0)
                res = matroska_parse_block(data, size, pos, cluster_time, AV_NOPTS_VALUE, -1, 0);
            break;

        case EBML_ID_VOID:
            res = ebml_read_skip();
            break;
        case MATROSKA_ID_CLUSTER:
            return 0;  // I don't know why here is the next cluster without level up. Jim send me it. Probably file
                       // error
        default:
            LTRACE(LT_WARN, 0, "Unknown entry " << id << " in cluster data");
            /* fall-through */

            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    return res;
}

int MatroskaDemuxer::ebml_read_element_level_up()
{
    // offset_t pos = url_ftell(pb);
    int num = 0;

    while (num_levels > 0)
    {
        MatroskaLevel *level = &levels[num_levels - 1];

        if (m_processedBytes >= level->start + level->length)
        {
            num_levels--;
            num++;
        }
        else
        {
            break;
        }
    }

    return num;
}

void MatroskaDemuxer::openFile(const std::string &streamName)
{
    readClose();
    BufferedFileReader *fileReader = dynamic_cast<BufferedFileReader *>(m_bufferedReader);
    if (!m_bufferedReader->openStream(m_readerID, streamName.c_str()))
        THROW(ERR_FILE_NOT_FOUND, "Can't open stream " << streamName);
    m_curPos = m_bufEnd = 0;
    m_processedBytes = 0;
    m_isEOF = false;

    num_levels = 0;
    level_up = 0;
    peek_id = 0;
    done = false;
    num_streams = 0;

    segment_start = 0;
    time_scale = 0;
    index_parsed = false;
    metadata_parsed = false;

    writing_app = 0;
    muxing_app = 0;
    num_tracks = 0;
    matroska_read_header();
}

void MatroskaDemuxer::readClose()
{
    delete[] writing_app;
    delete[] muxing_app;
    while (packets.size() > 0)
    {
        AVPacket *pkt = packets.front();
        delete[] pkt->data;
        delete pkt;
        packets.pop();
    }
    for (int i = 0; i < num_tracks; i++) delete tracks[i];
}

// --------------------------- refactored from ffmpeg matroska decoder -----------------------

int MatroskaDemuxer::ebml_read_num(int max_size, uint64_t *number)
{
    // ByteIOContext *pb = &matroska->ctx->pb;
    int len_mask = 0x80, read = 1, n = 1;
    int64_t total = 0;

    /* the first byte tells us the length in bytes - get_byte() can normally
     * return 0, but since that's not a valid first ebmlID byte, we can
     * use it safely here to catch EOS. */
    if (!(total = get_byte()))
    {
        /* we might encounter EOS here */
        if (!m_isEOF)
            THROW(ERR_MATROSKA_PARSE,
                  "Matroska parse error: Invalid EBML number size " << total << " at pos " << m_processedBytes - 1);
        return -BufferedReader::DATA_EOF;
    }

    /* get the length of the EBML number */
    while (read <= max_size && !(total & len_mask))
    {
        read++;
        len_mask >>= 1;
    }
    if (read > max_size)
    {
        offset_t pos = m_processedBytes - 1;
        THROW(ERR_MATROSKA_PARSE, "Matroska parse error: Invalid EBML number size " << total << " at pos " << pos);
    }

    /* read out length */
    total &= ~len_mask;
    while (n++ < read) total = (total << 8) | get_byte();
    *number = total;
    return read;
}

uint32_t MatroskaDemuxer::ebml_peek_id(int *level_up)
{
    uint32_t id;
    if (ebml_read_element_id(&id, level_up) < 0)
        return 0;
    return id;
}

int MatroskaDemuxer::readPacket(AVPacket &avPacket)
{
    int res;
    uint32_t id;
    if (m_lastDeliveryPacket)
    {
        delete[] m_lastDeliveryPacket->data;
        delete m_lastDeliveryPacket;
        m_lastDeliveryPacket = 0;
    }

    // Read stream until we have a packet queued.
    AVPacket *newPacket = 0;
    while (matroska_deliver_packet(newPacket) != 0)
    {
        // Have we already reached the end?
        if (done)
            return BufferedReader::DATA_EOF;

        res = 0;
        while (res == 0)
        {
            if (!(id = ebml_peek_id(&level_up)))
            {
                return BufferedReader::DATA_EOF;
            }
            else if (level_up)
            {
                level_up--;
                break;
            }

            switch (id)
            {
            case MATROSKA_ID_CLUSTER:
                if ((res = ebml_read_master(&id)) < 0)
                    break;
                if ((res = matroska_parse_cluster()) == 0)
                    res = 1;  // Parsed one cluster, let's get out.
                break;
            case EBML_ID_HEADER:
                matroska_read_header();
                break;
            default:
            case EBML_ID_VOID:
                res = ebml_read_skip();
                break;
            }

            if (level_up)
            {
                level_up--;
                break;
            }
        }

        if (res == -1)
            done = 1;
    }
    if (newPacket)
    {
        memcpy(&avPacket, newPacket, sizeof(AVPacket));
    }
    else
        memset(&avPacket, 0, sizeof(avPacket));
    m_lastDeliveryPacket = newPacket;
    return 0;
}

int MatroskaDemuxer::matroska_read_header()
{
    for (int i = 0; i < num_tracks; i++) delete tracks[i];
    num_tracks = 0;

    // MatroskaDemuxContext *matroska = s->priv_data;
    char *doctype = 0;
    int version, last_level, res = 0;
    uint32_t id;

    // matroska->ctx = s;

    /* First read the EBML header. */
    if ((res = ebml_read_header(&doctype, &version)) < 0)
        return res;
    if ((doctype == NULL) || strcmp(doctype, "matroska"))
    {
        if (doctype)
            delete[] doctype;
        THROW(ERR_MATROSKA_PARSE, "Wrong EBML doctype ('" << (doctype ? doctype : "(none)") << "' != 'matroska').");
    }
    delete[] doctype;
    if (version > 2)
    {
        THROW(ERR_MATROSKA_PARSE, "Matroska demuxer version 2 too old for file version " << version);
    }

    /* The next thing is a segment. */
    while (1)
    {
        if (!(id = ebml_peek_id(&last_level)))
            return -BufferedReader::DATA_EOF;
        if (id == MATROSKA_ID_SEGMENT)
            break;

        /* oi! */
        // av_log(matroska->ctx, AV_LOG_INFO, "Expected a Segment ID (0x%x), but received 0x%x!\n", MATROSKA_ID_SEGMENT,
        // id);
        if ((res = ebml_read_skip()) < 0)
            return res;
    }

    /* We now have a Matroska segment.
     * Seeks are from the beginning of the segment,
     * after the segment ID/length. */
    if ((res = ebml_read_master(&id)) < 0)
        return res;
    segment_start = m_processedBytes;

    time_scale = 1000000;
    /* we've found our segment, start reading the different contents in here */
    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        /* stream info */
        case MATROSKA_ID_INFO:
        {
            if ((res = ebml_read_master(&id)) < 0)
                break;
            res = matroska_parse_info();
            break;
        }

        /* track info headers */
        case MATROSKA_ID_TRACKS:
        {
            if ((res = ebml_read_master(&id)) < 0)
                break;
            res = matroska_parse_tracks();
            break;
        }

        case MATROSKA_ID_CHAPTERS:
            if ((res = ebml_read_master(&id)) < 0)
                break;
            res = matroska_parse_chapters();
            break;

        /* stream index */
        case MATROSKA_ID_CUES:
        {
            if (!index_parsed)
            {
                if ((res = ebml_read_master(&id)) < 0)
                    break;
                res = matroska_parse_index();
            }
            else
                res = ebml_read_skip();
            break;
        }

        /* metadata */
        case MATROSKA_ID_TAGS:
        {
            if (!metadata_parsed)
            {
                if ((res = ebml_read_master(&id)) < 0)
                    break;
                res = matroska_parse_metadata();
            }
            else
                res = ebml_read_skip();
            break;
        }

        /* file index (if seekable, seek to Cues/Tags to parse it) */
        case MATROSKA_ID_SEEKHEAD:
        {
            ebml_read_skip();
            /*
if ((res = ebml_read_master(&id)) < 0)
break;
res = matroska_parse_seekhead();
            */
            break;
        }

        case MATROSKA_ID_CLUSTER:
        {
            /* Do not read the master - this will be done in the next
             * call to matroska_read_packet. */
            res = 1;
            break;
        }

        default:
            LTRACE(LT_INFO, 0, "Unknown matroska file header ID " << id);
            /* fall-through */

        case EBML_ID_VOID:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    /* Have we found a cluster? */
    if (ebml_peek_id(NULL) == MATROSKA_ID_CLUSTER)
    {
        int i;
        MatroskaTrack *track;
        // AVStream *st;

        for (i = 0; i < num_tracks; i++)
        {
            int codec_id = -1;  // Enum CodecID codec_id = CODEC_ID_NONE;
            uint8_t *extradata = NULL;
            int extradata_size = 0;
            int extradata_offset = 0;
            track = tracks[i];
            track->stream_index = -1;
            if (track->codec_id == NULL)
                continue;

            track->stream_index = num_streams++;

            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_HEVC_FOURCC) && (track->codec_priv != NULL))
            {
                MatroskaVideoTrack *vtrack = (MatroskaVideoTrack *)track;
                track->parsed_priv_data = new ParsedH265TrackData(track->codec_priv, track->codec_priv_size);
            }
            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AVC_FOURCC) && (track->codec_priv != NULL))
            {
                MatroskaVideoTrack *vtrack = (MatroskaVideoTrack *)track;
                track->parsed_priv_data = new ParsedH264TrackData(track->codec_priv, track->codec_priv_size);
            }
            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_VIDEO_VFW_FOURCC) && (track->codec_priv != NULL))
            {
                MatroskaVideoTrack *vtrack = (MatroskaVideoTrack *)track;
                track->parsed_priv_data = new ParsedVC1TrackData(track->codec_priv, track->codec_priv_size);
            }

            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AUDIO_AC3))
            {
                track->parsed_priv_data = new ParsedAC3TrackData(track->codec_priv, track->codec_priv_size);
            }

            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AUDIO_AAC))
            {
                track->parsed_priv_data = new ParsedAACTrackData(track->codec_priv, track->codec_priv_size);
            }
            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AUDIO_PCM_BIG))
            {
                track->parsed_priv_data = new ParsedLPCMTrackData(track);
            }
            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AUDIO_PCM_LIT))
            {
                track->parsed_priv_data = new ParsedLPCMTrackData(track);
            }
            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AUDIO_ACM))
            {
                track->parsed_priv_data = new ParsedLPCMTrackData(track);
            }
            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_SRT))
            {
                track->parsed_priv_data = new ParsedSRTTrackData(track->codec_priv, track->codec_priv_size);
            }
            if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_SUBTITLE_PGS))
            {
                track->parsed_priv_data = new ParsedPGTrackData();
            }
        }
        res = 0;
    }

    return res;
}

/*
 * From here on, it's all XML-style DTD stuff... Needs no comments.
 */

int MatroskaDemuxer::matroska_parse_info()
{
    int res = 0;
    uint32_t id;

    // av_log(matroska->ctx, AV_LOG_DEBUG, "Parsing info...\n");

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        /* cluster timecode */
        case MATROSKA_ID_TIMECODESCALE:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            time_scale = num;
            break;
        }

        case MATROSKA_ID_DURATION:
        {
            double num;
            if ((res = ebml_read_float(&id, &num)) < 0)
                break;
            fileDuration = num * time_scale;
            break;
        }

        case MATROSKA_ID_TITLE:
        {
            char *text;
            if ((res = ebml_read_utf8(&id, &text)) < 0)
                break;
            strncpy(title, text, sizeof(title) - 1);
            delete[] text;
            break;
        }

        case MATROSKA_ID_WRITINGAPP:
        {
            char *text;
            if ((res = ebml_read_utf8(&id, &text)) < 0)
                break;
            writing_app = text;
            break;
        }

        case MATROSKA_ID_MUXINGAPP:
        {
            char *text;
            if ((res = ebml_read_utf8(&id, &text)) < 0)
                break;
            muxing_app = text;
            break;
        }

        case MATROSKA_ID_DATEUTC:
        {
            int64_t time;
            if ((res = ebml_read_date(&id, &time)) < 0)
                break;
            created = time;
            break;
        }

        default:
            LTRACE(LT_INFO, 0, "Unknown entry " << id << " in info header");
            /* fall-through */

        case EBML_ID_VOID:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    return res;
}

/*
 * Read the next element as a date (nanoseconds since 1/1/2000).
 * 0 is success, < 0 is failure.
 */

int MatroskaDemuxer::ebml_read_date(uint32_t *id, int64_t *date) { return ebml_read_sint(id, date); }

/*
 * Read the next element as a float.
 * 0 is success, < 0 is failure.
 */

int MatroskaDemuxer::ebml_read_float(uint32_t *id, double *num)
{
    int size, res;
    uint64_t rlength;

    if ((res = ebml_read_element_id(id, NULL)) < 0 || (res = ebml_read_element_length(&rlength)) < 0)
        return res;
    size = rlength;

    if (size == 4)
    {
        *num = av_int2flt(get_be32());
    }
    else if (size == 8)
    {
        *num = av_int2dbl(get_be64());
    }
    else
    {
        offset_t pos = m_processedBytes;
        THROW(ERR_MATROSKA_PARSE, "Invalid float element size " << size << " at position " << pos);
    }
    return 0;
}

/*
 * Read the next element as a UTF-8 string.
 * 0 is success, < 0 is failure.
 */

int MatroskaDemuxer::ebml_read_utf8(uint32_t *id, char **str) { return ebml_read_ascii(id, str); }

int MatroskaDemuxer::matroska_parse_metadata()
{
    int res = 0;
    uint32_t id;

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        /* Hm, this is unsupported... */
        default:
            LTRACE(LT_INFO, 0, "Unknown entry " << id << " in metadata header");
            /* fall-through */

        case EBML_ID_VOID:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    return res;
}

/*
 * Seek to a given offset.
 * 0 is success, -1 is failure.
 */

int MatroskaDemuxer::ebml_read_seek(int64_t offset)
{
    /* clear ID cache, if any */
    peek_id = 0;
    return (url_fseek(offset)) ? 0 : -1;
}

int MatroskaDemuxer::matroska_parse_chapters()
{
    int res = 0;
    uint32_t id;

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        case MATROSKA_ID_EDITIONENTRY:
        {
            uint64_t end = AV_NOPTS_VALUE, start = AV_NOPTS_VALUE;
            uint64_t uid = -1;
            char *title = NULL;
            // if there is more than one chapter edition we take only the first one
            if (chapters.size() > 0)
            {
                ebml_read_skip();
                break;
            }

            if ((res = ebml_read_master(&id)) < 0)
                break;

            while (res == 0)
            {
                if (!(id = ebml_peek_id(&level_up)))
                {
                    res = -BufferedReader::DATA_EOF;
                    break;
                }
                else if (level_up)
                {
                    level_up--;
                    break;
                }

                switch (id)
                {
                case MATROSKA_ID_CHAPTERATOM:
                    if ((res = ebml_read_master(&id)) < 0)
                        break;

                    while (res == 0)
                    {
                        if (!(id = ebml_peek_id(&level_up)))
                        {
                            res = -BufferedReader::DATA_EOF;
                            break;
                        }
                        else if (level_up)
                        {
                            level_up--;
                            break;
                        }

                        switch (id)
                        {
                        case MATROSKA_ID_CHAPTERTIMEEND:
                            res = ebml_read_uint(&id, &end);
                            break;

                        case MATROSKA_ID_CHAPTERTIMESTART:
                            res = ebml_read_uint(&id, &start);
                            break;

                        case MATROSKA_ID_CHAPTERDISPLAY:
                            if ((res = ebml_read_master(&id)) < 0)
                                break;

                            while (res == 0)
                            {
                                if (!(id = ebml_peek_id(&level_up)))
                                {
                                    res = -BufferedReader::DATA_EOF;
                                    break;
                                }
                                else if (level_up)
                                {
                                    level_up--;
                                    break;
                                }

                                switch (id)
                                {
                                case MATROSKA_ID_CHAPSTRING:
                                    res = ebml_read_utf8(&id, &title);
                                    break;

                                default:
                                    LTRACE(LT_INFO, 0, "Ignoring unknown Chapter display ID " << id);
                                case EBML_ID_VOID:
                                    res = ebml_read_skip();
                                    break;
                                }

                                if (level_up)
                                {
                                    level_up--;
                                    break;
                                }
                            }
                            break;

                        case MATROSKA_ID_CHAPTERUID:
                            res = ebml_read_uint(&id, &uid);
                            break;
                        default:
                            LTRACE(LT_INFO, 0, "Ignoring unknown Chapter atom ID " << id);
                        case MATROSKA_ID_CHAPTERFLAGHIDDEN:
                        case EBML_ID_VOID:
                            res = ebml_read_skip();
                            break;
                        }

                        if (level_up)
                        {
                            level_up--;
                            break;
                        }
                    }

                    if (start != AV_NOPTS_VALUE && uid != -1)
                    {
                        AVChapter chapter(start, title);
                        chapters[uid] = chapter;
                    }
                    delete[] title;
                    break;

                default:
                    LTRACE(LT_INFO, 0, "Ignoring unknown Edition entry ID " << id);
                case MATROSKA_ID_EDITIONUID:
                case MATROSKA_ID_EDITIONFLAGHIDDEN:
                case MATROSKA_ID_EDITIONFLAGDEFAULT:
                case EBML_ID_VOID:
                    res = ebml_read_skip();
                    break;
                }

                if (level_up)
                {
                    level_up--;
                    break;
                }
            }
            break;
        }

        default:
            LTRACE(LT_INFO, 0, "Expected an Edition entry (" << MATROSKA_ID_EDITIONENTRY << "), but found " << id);
        case EBML_ID_VOID:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    return res;
}

int MatroskaDemuxer::matroska_parse_tracks()
{
    int res = 0;
    uint32_t id;

    // av_log(matroska->ctx, AV_LOG_DEBUG, "parsing tracks...\n");

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        /* one track within the "all-tracks" header */
        case MATROSKA_ID_TRACKENTRY:
            res = matroska_add_stream();
            break;

        default:
            LTRACE(LT_INFO, 0, "Unknown entry " << id << " in track header");
            /* fall-through */

        case EBML_ID_VOID:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    return res;
}

int MatroskaDemuxer::readEncodingCompression(MatroskaTrack *track)
{
    int res = 0;
    uint32_t id;

    if ((res = ebml_read_master(&id)) < 0)
        return res;

    track->encodingAlgo = 0;

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up > 0)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        case MATROSKA_ID_ENCODINGCOMPALGO:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            track->encodingAlgo = num;
            break;
        }
        case MATROSKA_ID_ENCODINGCOMPSETTINGS:
        {
            uint8_t *data;
            int size;
            if ((res = ebml_read_binary(&id, &data, &size)) < 0)
                break;
            if (size > 0)
            {
                track->encodingAlgoPriv.resize(size);
                memcpy(&track->encodingAlgoPriv[0], data, size);
            }
            break;
        }
        default:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }
    return 0;
}

int MatroskaDemuxer::readTrackEncoding(MatroskaTrack *track)
{
    int res = 0;
    uint32_t id;

    if ((res = ebml_read_master(&id)) < 0)
        return res;

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up > 0)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        case MATROSKA_ID_ENCODINGCOMPRESSION:
        {
            res = readEncodingCompression(track);
            break;
        }
        default:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }
    return 0;
}

int MatroskaDemuxer::readTrackEncodings(MatroskaTrack *track)
{
    int res = 0;
    uint32_t id;

    if ((res = ebml_read_master(&id)) < 0)
        return res;

    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up > 0)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        case MATROSKA_ID_TRACKCONTENTENCODING:
        {
            readTrackEncoding(track);
            break;
        }
        default:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }
    return 0;
}

int MatroskaDemuxer::matroska_add_stream()
{
    int res = 0;
    uint32_t id;
    MatroskaTrack *track;

    // av_log(matroska->ctx, AV_LOG_DEBUG, "parsing track, adding stream..,\n");

    /* Allocate a generic track. As soon as we know its type we'll realloc. */
    track = (MatroskaTrack *)new char[MAX_TRACK_SIZE];
    memset(track, 0, MAX_TRACK_SIZE);
    track->encodingAlgo = -1;
    num_tracks++;
    if (num_tracks > MAX_STREAMS)
        THROW(ERR_COMMON, "Too many tracks. Max supported tracks count: " << MAX_STREAMS);
    strcpy(track->language, "eng");

    /* start with the master */
    if ((res = ebml_read_master(&id)) < 0)
        return res;

    /* try reading the trackentry headers */
    while (res == 0)
    {
        if (!(id = ebml_peek_id(&level_up)))
        {
            res = -BufferedReader::DATA_EOF;
            break;
        }
        else if (level_up > 0)
        {
            level_up--;
            break;
        }

        switch (id)
        {
        /* track number (unique stream ID) */
        case MATROSKA_ID_TRACKNUMBER:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            track->num = num;
            break;
        }

        /* track UID (unique identifier) */
        case MATROSKA_ID_TRACKUID:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            track->uid = num;
            break;
        }

        /* track type (video, audio, combined, subtitle, etc.) */
        case MATROSKA_ID_TRACKTYPE:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            if (track->type && track->type != num)
            {
                LTRACE(LT_INFO, 0, "More than one tracktype in an entry - skip");
                break;
            }
            track->type = (MatroskaTrackType)num;

            switch (track->type)
            {
            case TRACK_TYPE_VIDEO:
            case TRACK_TYPE_AUDIO:
            case TRACK_TYPE_SUBTITLE:
                break;
            case TRACK_TYPE_COMPLEX:
            case TRACK_TYPE_LOGO:
            case TRACK_TYPE_CONTROL:
            default:
                LTRACE(LT_INFO, 0, "Unknown or unsupported track type " << track->type);
                track->type = (MatroskaTrackType)0;
                break;
            }
            tracks[num_tracks - 1] = track;
            break;
        }

        /* tracktype specific stuff for video */
        case MATROSKA_ID_TRACKVIDEO:
        {
            MatroskaVideoTrack *videotrack;
            if (!track->type)
                track->type = TRACK_TYPE_VIDEO;
            if (track->type != TRACK_TYPE_VIDEO)
            {
                LTRACE(LT_INFO, 0, "video data in non-video track - ignoring");
                res = AVERROR_INVALIDDATA;
                break;
            }
            else if ((res = ebml_read_master(&id)) < 0)
                break;
            videotrack = (MatroskaVideoTrack *)track;

            while (res == 0)
            {
                if (!(id = ebml_peek_id(&level_up)))
                {
                    res = -BufferedReader::DATA_EOF;
                    break;
                }
                else if (level_up > 0)
                {
                    level_up--;
                    break;
                }

                switch (id)
                {
                /* fixme, this should be one-up, but I get it here */
                case MATROSKA_ID_TRACKDEFAULTDURATION:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    track->default_duration = num;
                    break;
                }

                /* video framerate */
                case MATROSKA_ID_VIDEOFRAMERATE:
                {
                    double num;
                    if ((res = ebml_read_float(&id, &num)) < 0)
                        break;
                    if (!track->default_duration)
                        track->default_duration = 1000000000 / num;
                    break;
                }

                /* width of the size to display the video at */
                case MATROSKA_ID_VIDEODISPLAYWIDTH:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    videotrack->display_width = num;
                    break;
                }

                /* height of the size to display the video at */
                case MATROSKA_ID_VIDEODISPLAYHEIGHT:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    videotrack->display_height = num;
                    break;
                }

                /* width of the video in the file */
                case MATROSKA_ID_VIDEOPIXELWIDTH:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    videotrack->pixel_width = num;
                    break;
                }

                /* height of the video in the file */
                case MATROSKA_ID_VIDEOPIXELHEIGHT:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    videotrack->pixel_height = num;
                    break;
                }

                /* whether the video is interlaced */
                case MATROSKA_ID_VIDEOFLAGINTERLACED:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    if (num)
                        track->flags |= MATROSKA_VIDEOTRACK_INTERLACED;
                    else
                        track->flags &= ~MATROSKA_VIDEOTRACK_INTERLACED;
                    break;
                }

                /* stereo mode (whether the video has two streams,
                 * where one is for the left eye and the other for
                 * the right eye, which creates a 3D-like
                 * effect) */
                case MATROSKA_ID_VIDEOSTEREOMODE:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    if (num != MATROSKA_EYE_MODE_MONO && num != MATROSKA_EYE_MODE_LEFT &&
                        num != MATROSKA_EYE_MODE_RIGHT && num != MATROSKA_EYE_MODE_BOTH)
                    {
                        LTRACE(LT_INFO, 0, "Ignoring unknown eye mode " << (uint32_t)num);
                        break;
                    }
                    videotrack->eye_mode = (MatroskaEyeMode)num;
                    break;
                }

                /* aspect ratio behaviour */
                case MATROSKA_ID_VIDEOASPECTRATIO:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    if (num != MATROSKA_ASPECT_RATIO_MODE_FREE && num != MATROSKA_ASPECT_RATIO_MODE_KEEP &&
                        num != MATROSKA_ASPECT_RATIO_MODE_FIXED)
                    {
                        LTRACE(LT_INFO, 0, "Ignoring unknown aspect ratio " << (uint32_t)num);
                        break;
                    }
                    videotrack->ar_mode = (MatroskaAspectRatioMode)num;
                    break;
                }

                /* colourspace (only matters for raw video)
                 * fourcc */
                case MATROSKA_ID_VIDEOCOLOURSPACE:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    videotrack->fourcc = num;
                    break;
                }

                default:
                    LTRACE(LT_INFO, 0, "Unknown video track header entry " << id << " - ignoring\n");
                    /* pass-through */

                case EBML_ID_VOID:
                    res = ebml_read_skip();
                    break;
                }

                if (level_up)
                {
                    level_up--;
                    break;
                }
            }
            break;
        }

        /* tracktype specific stuff for audio */
        case MATROSKA_ID_TRACKAUDIO:
        {
            MatroskaAudioTrack *audiotrack;
            if (!track->type)
                track->type = TRACK_TYPE_AUDIO;
            if (track->type != TRACK_TYPE_AUDIO)
            {
                LTRACE(LT_INFO, 0, "audio data in non-audio track - ignoring");
                res = AVERROR_INVALIDDATA;
                break;
            }
            else if ((res = ebml_read_master(&id)) < 0)
                break;
            audiotrack = (MatroskaAudioTrack *)track;
            audiotrack->channels = 1;
            audiotrack->samplerate = 8000;

            while (res == 0)
            {
                if (!(id = ebml_peek_id(&level_up)))
                {
                    res = -BufferedReader::DATA_EOF;
                    break;
                }
                else if (level_up > 0)
                {
                    level_up--;
                    break;
                }

                switch (id)
                {
                /* samplerate */
                case MATROSKA_ID_AUDIOSAMPLINGFREQ:
                {
                    double num;
                    if ((res = ebml_read_float(&id, &num)) < 0)
                        break;
                    audiotrack->internal_samplerate = audiotrack->samplerate = num;
                    break;
                }

                case MATROSKA_ID_AUDIOOUTSAMPLINGFREQ:
                {
                    double num;
                    if ((res = ebml_read_float(&id, &num)) < 0)
                        break;
                    audiotrack->samplerate = num;
                    break;
                }

                    /* bitdepth */
                case MATROSKA_ID_AUDIOBITDEPTH:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    audiotrack->bitdepth = num;
                    break;
                }

                    /* channels */
                case MATROSKA_ID_AUDIOCHANNELS:
                {
                    uint64_t num;
                    if ((res = ebml_read_uint(&id, &num)) < 0)
                        break;
                    audiotrack->channels = num;
                    break;
                }

                default:
                    LTRACE(LT_INFO, 0, "Unknown audio track header entry " << id << " - ignoring\n");
                    /* pass-through */

                case EBML_ID_VOID:
                    res = ebml_read_skip();
                    break;
                }

                if (level_up)
                {
                    level_up--;
                    break;
                }
            }
            break;
        }

            /* codec identifier */
        case MATROSKA_ID_CODECID:
        {
            char *text;
            if ((res = ebml_read_ascii(&id, &text)) < 0)
                break;
            track->codec_id = text;
            break;
        }

            /* codec private data */
        case MATROSKA_ID_CODECPRIVATE:
        {
            uint8_t *data;
            int size;
            if ((res = ebml_read_binary(&id, &data, &size) < 0))
                break;
            track->codec_priv = data;
            track->codec_priv_size = size;
            break;
        }

            /* name of the codec */
        case MATROSKA_ID_CODECNAME:
        {
            char *text;
            if ((res = ebml_read_utf8(&id, &text)) < 0)
                break;
            track->codec_name = text;
            break;
        }

            /* name of this track */
        case MATROSKA_ID_TRACKNAME:
        {
            char *text;
            if ((res = ebml_read_utf8(&id, &text)) < 0)
                break;
            track->name = text;
            break;
        }

            /* language (matters for audio/subtitles, mostly) */
        case MATROSKA_ID_TRACKLANGUAGE:
        {
            char *text, *end;
            if ((res = ebml_read_utf8(&id, &text)) < 0)
                break;
            if ((end = strchr(text, '-')))
                *end = '\0';
            if (strlen(text) == 3)
                strcpy(track->language, text);
            delete[] text;
            break;
        }

            /* whether this is actually used */
        case MATROSKA_ID_TRACKFLAGENABLED:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            if (num)
                track->flags |= MATROSKA_TRACK_ENABLED;
            else
                track->flags &= ~MATROSKA_TRACK_ENABLED;
            break;
        }

            /* whether it's the default for this track type */
        case MATROSKA_ID_TRACKFLAGDEFAULT:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            if (num)
                track->flags |= MATROSKA_TRACK_DEFAULT;
            else
                track->flags &= ~MATROSKA_TRACK_DEFAULT;
            break;
        }

            /* lacing (like MPEG, where blocks don't end/start on frame
             * boundaries) */
        case MATROSKA_ID_TRACKFLAGLACING:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            if (num)
                track->flags |= MATROSKA_TRACK_LACING;
            else
                track->flags &= ~MATROSKA_TRACK_LACING;
            break;
        }

            /* default length (in time) of one data block in this track */
        case MATROSKA_ID_TRACKDEFAULTDURATION:
        {
            uint64_t num;
            if ((res = ebml_read_uint(&id, &num)) < 0)
                break;
            track->default_duration = num;
            break;
        }

        case MATROSKA_ID_TRACKCONTENTENCODINGS:
            readTrackEncodings(track);
            break;
        default:
            LTRACE(LT_INFO, 0, "Unknown track header entry " << id << " - ignoring");
            /* pass-through */

        case EBML_ID_VOID:
        /* we ignore these because they're nothing useful. */
        case MATROSKA_ID_CODECINFOURL:
        case MATROSKA_ID_CODECDOWNLOADURL:
        case MATROSKA_ID_TRACKMINCACHE:
        case MATROSKA_ID_TRACKMAXCACHE:
            res = ebml_read_skip();
            break;
        }

        if (level_up)
        {
            level_up--;
            break;
        }
    }

    return res;
}

// ------------- need to implemented --------------

int MatroskaDemuxer::simpleDemuxBlock(DemuxedData &demuxedData, const PIDSet &acceptedPIDs, int64_t &discardSize)
{
    for (std::set<uint32_t>::const_iterator itr = acceptedPIDs.begin(); itr != acceptedPIDs.end(); ++itr)
        demuxedData[*itr];

    AVPacket packet;
    uint32_t demuxedSize = 0;
    discardSize = 0;
    while (demuxedSize < m_fileBlockSize)
    {
        int readRez = readPacket(packet);
        if (readRez == BufferedReader::DATA_EOF)
        {
            discardSize = m_processedBytes - m_lastProcessedBytes - demuxedSize;
            m_lastProcessedBytes = m_processedBytes;
            return readRez;
        }

        PIDFilters::iterator itr = m_pidFilters.find(packet.stream_index);
        if (itr != m_pidFilters.end())
        {
            demuxedSize += itr->second->demuxPacket(demuxedData, acceptedPIDs, packet);
        }
        else
        {
            if (acceptedPIDs.find(packet.stream_index) != acceptedPIDs.end())
            {
                MemoryBlock &vect = demuxedData[packet.stream_index];
                if (packet.size > 0)
                    vect.append(packet.data, packet.size);
                demuxedSize += packet.size;
            }
        }
    }
    discardSize = m_processedBytes - m_lastProcessedBytes - demuxedSize;
    m_lastProcessedBytes = m_processedBytes;
    return 0;
}

void MatroskaDemuxer::getTrackList(std::map<uint32_t, TrackInfo> &trackList)
{
    for (int i = 0; i < num_tracks; i++)
        trackList.insert(std::make_pair(i + 1, TrackInfo(getTrackType(tracks[i]), tracks[i]->language, 0)));
}

int MatroskaDemuxer::getTrackType(MatroskaTrack *track)
{
    if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_SRT))
        return TRACKTYPE_SRT;
    else if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AUDIO_PCM_BIG))
        return TRACKTYPE_WAV;
    else if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AUDIO_PCM_LIT))
        return TRACKTYPE_WAV;
    else if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_AUDIO_ACM))
        return TRACKTYPE_WAV;
    else if (!strcmp(track->codec_id, MATROSKA_CODEC_ID_SUBTITLE_PGS))
        return STREAM_TYPE_SUB_PGS;
    else
        return 0;
}

std::vector<AVChapter> MatroskaDemuxer::getChapters()
{
    std::vector<AVChapter> rez;
    for (std::map<int, AVChapter>::const_iterator itr = chapters.begin(); itr != chapters.end(); ++itr)
        rez.push_back(itr->second);
    std::sort(rez.begin(), rez.end());
    return rez;
}
