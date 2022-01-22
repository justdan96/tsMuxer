
#include "psgStreamReader.h"

#include <fs/systemlog.h>

#include <sstream>
#include <string>

#include "avCodecs.h"
#include "ioContextDemuxer.h"
#include "math.h"
#include "tsMuxer.h"
#include "tsPacket.h"
#include "vodCoreException.h"
#include "vod_common.h"

using namespace text_subtitles;
using namespace std;

#define fabs(a) ((a) >= 0 ? (a) : -(a))

double pgs_frame_rates[16] = {
    0, 23.97602397602397, 24, 25, 29.97002997002997, 30, 50, 59.94005994005994, 60, 0, 0, 0, 0, 0, 0, 0};

PGSStreamReader::PGSStreamReader()
    : m_avFragmentEnd(0),
      object_width(0),
      object_height(0),
      m_firstRenderedPacket(true),
      m_palleteID(0),
      m_paletteVersion(0),
      m_objectWindowHeight(0),
      m_objectWindowTop(0)
{
    m_curPos = m_buffer = 0;
    m_tmpBufferLen = 0;
    m_state = State::stParsePES;
    composition_state = CompositionState::csEpochStart;
    m_processedSize = 0;
    m_demuxMode = false;
    m_afterPesByte = 0;
    // m_supWritedPts = m_supWritedDts = -1;
    m_maxPTS = m_lastDTS = m_lastPTS = -1;
    m_video_width = 0;
    m_video_height = 0;
    m_frame_rate = 0;
    m_newFps = 0;
    m_scale = 1.0;
    m_isNewFrame = false;
    m_needRescale = false;
    m_imgBuffer = 0;
    m_rgbBuffer = 0;
    m_scaledRgbBuffer = 0;
    m_scaled_width = 0;
    m_scaled_height = 0;
    m_render = new text_subtitles::TextToPGSConverter(false);
    m_renderedData = 0;
    m_fontBorder = 0;
    m_offsetId = 0xff;
    m_forced_on_flag = false;

    // SS PG data
    isSSPG = false;
    leftEyeSubStreamIdx = -1;
    rightEyeSubStreamIdx = -1;
    ssPGOffset = 0xff;
}

void PGSStreamReader::video_descriptor(BitStreamReader& bitReader)
{
    m_video_width = bitReader.getBits(16);
    m_video_height = bitReader.getBits(16);
    int frame_rate_index = bitReader.getBits(4);
    m_frame_rate = pgs_frame_rates[frame_rate_index];
    bitReader.skipBits(4);
}

void PGSStreamReader::composition_descriptor(BitStreamReader& bitReader)
{
    bitReader.skipBits(16);  // composition_number
    composition_state = (CompositionState)bitReader.getBits(2);
    bitReader.skipBits(6);
}

void PGSStreamReader::composition_object(BitStreamReader& bitReader)
{
    int object_id_ref = bitReader.getBits(16);
    bitReader.skipBits(8);  // window_id_ref
    bool object_cropped_flag = bitReader.getBit();
    m_forced_on_flag = bitReader.getBit();
    bitReader.skipBits(6);
    composition_object_horizontal_position[object_id_ref] = bitReader.getBits(16);
    composition_object_vertical_position[object_id_ref] = bitReader.getBits(16);
    if (object_cropped_flag)
    {
        bitReader.skipBits(32);  // object_cropping_horizontal_position, object_cropping_vertical_position
        bitReader.skipBits(32);  // object_cropping_width, object_cropping_height
    }
}

void PGSStreamReader::pgs_window(BitStreamReader& bitReader)
{
    bitReader.skipBits(8);   // window_id
    bitReader.skipBits(32);  // window_horizontal_position, window_vertical_position
    bitReader.skipBits(32);  // window_width, window_height
}

int PGSStreamReader::calcFpsIndex(double fps)
{
    for (int i = 0; i < 16; i++)
        if (fabs(pgs_frame_rates[i] - fps) < 1e-4)
            return i;
    THROW(ERR_COMMON, "Non standard fps value are not supported for PGS streams");
}

void PGSStreamReader::readPalette(uint8_t* pos, uint8_t* end)
{
    m_palette.clear();
    m_palleteID = *pos++;
    m_paletteVersion = *pos++;
    while (pos < end)
    {
        YUVQuad color;
        int index = *pos++;
        color.Y = *pos++;
        color.Cr = *pos++;
        color.Cb = *pos++;
        color.alpha = *pos++;
        if (color.alpha != 0)
            m_palette.insert(make_pair(index, color));
    }
}

void PGSStreamReader::yuvToRgb(int minY)
{
    uint8_t* src = m_imgBuffer;
    uint8_t* end = src + m_video_width * m_video_height;
    // uint8_t* dst = m_rgbBuffer;
    RGBQUAD* dst = (RGBQUAD*)m_rgbBuffer;

    RGBQUAD rgbPal[256]{};
    text_subtitles::YUVQuad yuvPal[256];
    memset(&rgbPal[0], 0, sizeof(rgbPal));
    memset(&rgbPal[0], 0, sizeof(yuvPal));

    for (map<uint8_t, YUVQuad>::const_iterator itr = m_palette.begin(); itr != m_palette.end(); ++itr)
    {
        rgbPal[itr->first] = TextToPGSConverter::YUVAToRGBA(itr->second);
        yuvPal[itr->first] = itr->second;
    }
    RGBQUAD zeroRgb;
    memset(&zeroRgb, 0, sizeof(zeroRgb));
    for (; src < end; ++src)
    {
        if (yuvPal[*src].Y >= minY)
            *dst++ = rgbPal[*src];
        else
            *dst++ = zeroRgb;
    }
}

void PGSStreamReader::decodeRleData(int xOffset, int yOffset)
{
    if (m_dstRle.size() == 0)
        return;
    uint8_t* src = &m_dstRle[0];
    uint8_t* srcEnd = src + m_dstRle.size();

    uint8_t* dst = m_imgBuffer + (yOffset * m_video_width + xOffset);
    int dstLineStep = m_video_width - object_width;
    uint8_t color;
    int run_length;
    while (src < srcEnd)
    {
        if (*src != 0)
            *dst++ = *src++;
        else
        {
            src++;
            if (*src == 0)
            {  // end of line
                src++;
                dst += dstLineStep;
            }
            else
            {
                bool b1 = *src & 0x80;
                if (*src & 0x40)
                {
                    run_length = ((*src & 0x3f) << 8) + src[1];
                    src += 2;
                }
                else
                {
                    run_length = *src & 0x3f;
                    src++;
                }
                if (b1)
                    color = *src++;
                else
                    color = 0;
                for (int i = 0; i < run_length; ++i) *dst++ = color;
            }
        }
    }
}

const static int Y_THRESHOLD = 33;

int PGSStreamReader::readObjectDef(uint8_t* pos, uint8_t* end)
{
    pos += 4;  // skip object ID and version number
    uint32_t objectSize = AV_RB24(pos);
    pos += 3;
    uint8_t* objEnd = pos + objectSize;

    if (m_bufEnd < objEnd)
        return NEED_MORE_DATA;

    m_dstRle.clear();
    m_dstRle.reserve(objectSize);

    object_width = AV_RB16(pos);
    pos += 2;
    object_height = AV_RB16(pos);
    pos += 2;
    int object_id = 0;
    while (pos < objEnd)
    {
        if (pos >= end)
        {
            if (*pos != OBJECT_DEF_SEGMENT)
                THROW(ERR_COMMON, "Unexpected byte " << *pos << " during parsing Object definition segment");
            pos++;  // skip OBJECT_DEF_SEGMENT
            end = pos + AV_RB16(pos);
            pos += 2;
            object_id = AV_RB16(pos);
            pos += 4;  // skip object ID and version number
        }
        size_t oldSize = m_dstRle.size();
        m_dstRle.resize(oldSize + end - pos);
        memcpy(&m_dstRle[oldSize], pos, end - pos);
        pos += end - pos;
    }
    if (m_needRescale)
    {
        decodeRleData(composition_object_horizontal_position[object_id],
                      composition_object_vertical_position[object_id]);
        yuvToRgb(m_fontBorder ? Y_THRESHOLD : 0);
        BitmapInfo bmpDest{};
        BitmapInfo bmpRef{};

        bmpRef.buffer = (RGBQUAD*)m_rgbBuffer;
        bmpRef.Width = m_video_width;
        bmpRef.Height = m_video_height;
        bmpDest.buffer = (RGBQUAD*)m_scaledRgbBuffer;
        bmpDest.Width = m_scaled_width;
        bmpDest.Height = m_scaled_height;

        rescaleRGB(&bmpDest, &bmpRef);
        if (m_fontBorder)
            TextSubtitlesRender::addBorder(m_fontBorder, m_scaledRgbBuffer, m_scaled_width, m_scaled_height);
        // memcpy(bmpDest.buffer, bmpRef.buffer, bmpDest.Width * bmpDest.Height * 4);
    }
    return 0;
}

void PGSStreamReader::rescaleRGB(BitmapInfo* bmpDest, BitmapInfo* bmpRef)
{
    double xFactor = (double)bmpRef->Width / (double)bmpDest->Width;
    double yFactor = (double)bmpRef->Height / (double)bmpDest->Height;
    RGBQUAD* ImagePixels = bmpDest->buffer;
    RGBQUAD *c1, *c2, *c3, *c4;

    for (int yDest = 0; yDest < bmpDest->Height; yDest++)
    {
        for (int xDest = 0; xDest < bmpDest->Width; xDest++)
        {
            int floor_x = (int)floor(xDest * xFactor);
            int floor_y = (int)floor(yDest * yFactor);
            int ceil_x = FFMIN(bmpRef->Width - 1, floor_x + 1);
            int ceil_y = FFMIN(bmpRef->Height - 1, floor_y + 1);
            double fraction_x = xDest * xFactor - floor_x;
            double fraction_y = yDest * yFactor - floor_y;
            double one_minus_x = 1.0 - fraction_x;
            double one_minus_y = 1.0 - fraction_y;
            c2 = c1 = bmpRef->buffer + floor_y * bmpRef->Width;
            c1 += floor_x;
            c2 += ceil_x;
            c4 = c3 = bmpRef->buffer + ceil_y * bmpRef->Width;
            c3 += floor_x;
            c4 += ceil_x;
            double b1 = one_minus_x * c1->rgbRed + fraction_x * c2->rgbRed;
            double b2 = one_minus_x * c3->rgbRed + fraction_x * c4->rgbRed;
            ImagePixels->rgbRed = (uint8_t)(one_minus_y * b1 + fraction_y * b2);
            b1 = one_minus_x * c1->rgbGreen + fraction_x * c2->rgbGreen;
            b2 = one_minus_x * c3->rgbGreen + fraction_x * c4->rgbGreen;
            ImagePixels->rgbGreen = (uint8_t)(one_minus_y * b1 + fraction_y * b2);
            b1 = one_minus_x * c1->rgbBlue + fraction_x * c2->rgbBlue;
            b2 = one_minus_x * c3->rgbBlue + fraction_x * c4->rgbBlue;
            ImagePixels->rgbBlue = (uint8_t)(one_minus_y * b1 + fraction_y * b2);
            b1 = one_minus_x * c1->rgbReserved + fraction_x * c2->rgbReserved;
            b2 = one_minus_x * c3->rgbReserved + fraction_x * c4->rgbReserved;
            ImagePixels->rgbReserved = (uint8_t)(one_minus_y * b1 + fraction_y * b2);
            ImagePixels++;
        }
    }
}

void PGSStreamReader::renderTextShow(int64_t inTime)
{
    m_renderedBlocks.clear();
    m_firstRenderedPacket = true;

    uint32_t mask = 0;
    int step = 0;
    while (!m_render->rlePack(mask))
    {
        // reduce colors
        uint8_t* tmp = (uint8_t*)&mask;
        int idx = step++ % 4;
        tmp[idx] <<= 1;
        tmp[idx]++;
    }
    inTime = (int64_t)(inTime / INT_FREQ_TO_TS_FREQ);

    double decodedObjectSize = m_render->renderedHeight() * m_scaled_width;
    int64_t compositionDecodeTime = (int64_t)(90000.0 * decodedObjectSize / PIXEL_DECODING_RATE + 0.999);
    int64_t windowsTransferTime = (int64_t)(90000.0 * decodedObjectSize / PIXEL_COMPOSITION_RATE + 0.999);
    const int64_t PLANEINITIALIZATIONTIME =
        (int64_t)(90000.0 * (m_scaled_width * m_scaled_height) / PIXEL_COMPOSITION_RATE + 0.999);
    const int64_t PRESENTATION_DTS_DELTA = PLANEINITIALIZATIONTIME + windowsTransferTime;

    m_objectWindowHeight = FFMAX(0, m_render->renderedHeight());
    m_objectWindowTop = m_render->maxLine();

    // show text
    uint8_t* curPos = m_renderedData;
    // composition segment. pts=x,  dts = x-0.0648 (pts alignment to video grid) (I get constant value from real PGS
    // track as example)
    int rLen =
        m_render->composePresentationSegment(curPos, CompositionMode::Start, inTime, inTime - PRESENTATION_DTS_DELTA,
                                             m_objectWindowTop, m_demuxMode, m_forced_on_flag);
    m_renderedBlocks.push_back(PGSRenderedBlock(inTime, inTime - PRESENTATION_DTS_DELTA, rLen, curPos));
    curPos += rLen;
    // window definition.   pts=x-0.001, dts = x-0.0648
    rLen = m_render->composeWindowDefinition(curPos, inTime - windowsTransferTime, inTime - PRESENTATION_DTS_DELTA,
                                             m_objectWindowTop, m_objectWindowHeight, m_demuxMode);
    m_renderedBlocks.push_back(
        PGSRenderedBlock(inTime - windowsTransferTime, inTime - PRESENTATION_DTS_DELTA, rLen, curPos));
    curPos += rLen;

    // palette.             pts=x-0.0648, dts = x-0.0648
    rLen = m_render->composePaletteDefinition(m_render->m_paletteByColor, curPos, inTime - PRESENTATION_DTS_DELTA,
                                              inTime - PRESENTATION_DTS_DELTA, m_demuxMode);
    m_renderedBlocks.push_back(
        PGSRenderedBlock(inTime - PRESENTATION_DTS_DELTA, inTime - PRESENTATION_DTS_DELTA, rLen, curPos));
    curPos += rLen;
    // object               pts=x-0.0627, dts = x-0.0648
    // inTime - 5643
    int64_t odfPTS = inTime - PRESENTATION_DTS_DELTA + compositionDecodeTime;
    rLen = m_render->composeObjectDefinition(curPos, odfPTS, inTime - PRESENTATION_DTS_DELTA, m_render->minLine(),
                                             m_render->maxLine(), m_demuxMode);
    m_renderedBlocks.push_back(PGSRenderedBlock(odfPTS, inTime - PRESENTATION_DTS_DELTA, rLen, curPos));
    curPos += rLen;
    // end                  pts=x-0.0627, dts = x-0.0627
    rLen = m_render->composeEnd(curPos, odfPTS, odfPTS, m_demuxMode);
    m_renderedBlocks.push_back(PGSRenderedBlock(odfPTS, odfPTS, rLen, curPos));
}

void PGSStreamReader::renderTextHide(int64_t outTime)
{
    double decodedObjectSize = m_render->renderedHeight() * m_scaled_width;
    int64_t windowsTransferTime = (int64_t)(90000.0 * decodedObjectSize / PIXEL_COMPOSITION_RATE + 0.999);

    m_firstRenderedPacket = true;
    outTime = (int64_t)(outTime / INT_FREQ_TO_TS_FREQ);
    m_renderedBlocks.clear();
    // hide text
    uint8_t* curPos = m_renderedData;
    // composition segment. pts=x,       dts = x-0.001 (pts alignment to video grid)
    int rLen =
        m_render->composePresentationSegment(curPos, CompositionMode::Finish, outTime,
                                             outTime - windowsTransferTime - 90, m_objectWindowTop, m_demuxMode, false);
    m_renderedBlocks.push_back(PGSRenderedBlock(outTime, outTime - windowsTransferTime - 90, rLen, curPos));
    curPos += rLen;
    // windows              pts=x-0.001, dts = x-0.001
    rLen = m_render->composeWindowDefinition(curPos, outTime - windowsTransferTime, outTime - windowsTransferTime - 90,
                                             m_objectWindowTop, m_objectWindowHeight, m_demuxMode);
    m_renderedBlocks.push_back(
        PGSRenderedBlock(outTime - windowsTransferTime, outTime - windowsTransferTime - 90, rLen, curPos));
    curPos += rLen;
    // end                  pts=x-0.001, dts = x-0.001
    rLen = m_render->composeEnd(curPos, outTime - 90, outTime - 90, m_demuxMode);
    m_renderedBlocks.push_back(PGSRenderedBlock(outTime - 90, outTime - 90, rLen, curPos));
}

int64_t getTimeValueNano(uint8_t* pos)
{
    int64_t pts = (int64_t)AV_RB32(pos);
    if (pts > 0xff000000u)
        return ptsToNanoClock(pts - 0x100000000ll);
    else
        return ptsToNanoClock(pts);
}

int64_t getTimeValueNano(int64_t pts)
{
    if (pts > 0x1ff000000ull)
        return ptsToNanoClock(pts - 0x200000000ll);
    else
        return ptsToNanoClock(pts);
}

int PGSStreamReader::readPacket(AVPacket& avPacket)
{
    avPacket.size = 0;
    avPacket.codec = this;
    avPacket.codecID = CODEC_S_PGS;
    avPacket.stream_index = m_streamIndex;
    avPacket.data = 0;
    avPacket.size = 0;
    avPacket.flags = 0;
    avPacket.duration = 0;

    if (m_renderedBlocks.size() > 0)
    {  // rendered data block (rescaled PGS). send it.
        if (m_firstRenderedPacket)
            avPacket.flags += AVPacket::FORCE_NEW_FRAME;
        m_firstRenderedPacket = false;
        PGSRenderedBlock& block = m_renderedBlocks[0];
        avPacket.data = block.data;
        avPacket.size = FFMIN(MAX_AV_PACKET_SIZE, block.len);
        avPacket.pts = (int64_t)(block.pts * INT_FREQ_TO_TS_FREQ);
        avPacket.dts = (int64_t)(block.dts * INT_FREQ_TO_TS_FREQ);
        block.data += avPacket.size;
        block.len -= avPacket.size;
        if (block.len == 0)
        {
            m_renderedBlocks.erase(m_renderedBlocks.begin());
            m_firstRenderedPacket = true;
        }
        return 0;
    }

    if (m_curPos == 0)
        return NEED_MORE_DATA;

    if (m_video_height == 0)
    {
        intDecodeStream(m_curPos, m_bufEnd - m_curPos);
        if (m_video_height == 0)
            return NEED_MORE_DATA;

        int tmpWidth;
        int tmpHeight;
        m_render->enlargeCrop(m_video_width, m_video_height, &tmpWidth, &tmpHeight);
        m_needRescale = (m_scaled_width && m_scaled_width != tmpWidth) || (m_scaled_height && m_scaled_height != tmpHeight);
        // m_needRescale |= true;
        if (m_needRescale)
        {
            m_imgBuffer = new uint8_t[m_video_width * m_video_height];
            m_rgbBuffer = new uint8_t[m_video_width * m_video_height * 4];
            m_scaledRgbBuffer = new uint8_t[m_scaled_width * m_scaled_height * 4];
            m_renderedData = new uint8_t[(m_scaled_width + 16) * m_scaled_height + 16384];

            memset(m_imgBuffer, 0xff, (size_t)m_video_width * m_video_height);
            memset(m_rgbBuffer, 0x00, (size_t)m_video_width * m_video_height * 4);
            memset(m_scaledRgbBuffer, 0x00, (size_t)m_scaled_width * m_scaled_height * 4);
            m_render->setImageBuffer(m_scaledRgbBuffer);
        }
        LTRACE(LT_INFO, 2,
               "Decoding PGS stream (track " << m_streamIndex << "): "
                                             << " Resolution: " << m_video_width << ':' << m_video_height
                                             << "  Frame rate: " << m_frame_rate);
        if (m_newFps != 0 && fabs(m_newFps - m_frame_rate) > 1e-4)
        {
            LTRACE(LT_INFO, 2,
                   "Change FPS from " << m_frame_rate << " to " << m_newFps << " for PGS stream #" << m_streamIndex);
            m_scale = m_frame_rate / m_newFps;
        }
        if (m_needRescale)
            LTRACE(LT_INFO, 2,
                   "Change PGS resolution from " << m_video_width << ':' << m_video_height << " to " << m_scaled_width
                                                 << ':' << m_scaled_height
                                                 << ". Scaling method: Bilinear interpolation");
    }

    if (m_state == State::stAVPacketFragmented)
    {
        int avLen = (int)(m_avFragmentEnd - m_curPos);
        if (avLen > MAX_AV_PACKET_SIZE)
        {
            avLen = MAX_AV_PACKET_SIZE;
        }
        else
            m_state = State::stParsePES;
        avPacket.pts = m_lastPTS;
        avPacket.dts = m_lastDTS;
        avPacket.data = m_curPos;
        avPacket.size = m_needRescale ? 0 : avLen;
        m_curPos += avLen;
        m_processedSize += avLen;
        if (m_needRescale)
        {
            avPacket.dts = FFMAX(0, avPacket.dts);
            avPacket.pts = FFMAX(0, avPacket.pts);
        }
        return 0;
    }

    if (m_bufEnd - m_curPos < 3)
    {
        m_tmpBufferLen = m_bufEnd - m_curPos;
        memmove(&m_tmpBuffer[0], m_curPos, m_tmpBufferLen);
        return NEED_MORE_DATA;
    }
    bool pgStartCode = m_curPos[0] == 'P' && m_curPos[1] == 'G';
    if (pgStartCode)
    {
        if (m_bufEnd - m_curPos < 10)
        {
            m_tmpBufferLen = m_bufEnd - m_curPos;
            memmove(&m_tmpBuffer[0], m_curPos, m_tmpBufferLen);
            return NEED_MORE_DATA;
        }
        m_lastPTS = (int64_t)(getTimeValueNano(m_curPos + 2) * m_scale);
        m_maxPTS = FFMAX(m_maxPTS, m_lastPTS);
        m_lastDTS = (int64_t)(getTimeValueNano(m_curPos + 6) * m_scale);
        if (m_lastDTS == 0)
            m_lastDTS = m_lastPTS;
        m_curPos += 10;
        m_processedSize += 10;
        m_state = State::stParsePGS;
        avPacket.pts = m_lastPTS;
        avPacket.dts = m_lastDTS;
        m_isNewFrame = true;
        // LTRACE(LT_INFO, 2, "PGS PES#" << m_streamIndex << ". PTS=" << m_lastPTS/1e9 << " DTS=" << m_lastDTS/1e9);
        if (m_needRescale)
        {
            avPacket.dts = FFMAX(0, avPacket.dts);
            avPacket.pts = FFMAX(0, avPacket.pts);
        }
        return 0;
    }

    bool pesStartCode = m_curPos[0] == 0 && m_curPos[1] == 0 && m_curPos[2] == 01;

    if (pesStartCode)
    {
        if (m_bufEnd - m_curPos < 8)
        {
            m_tmpBufferLen = m_bufEnd - m_curPos;
            memmove(&m_tmpBuffer[0], m_curPos, m_tmpBufferLen);
            return NEED_MORE_DATA;
        }
        PESPacket* pesPacket = (PESPacket*)m_curPos;
        int pesHeaderLen = pesPacket->getHeaderLength();
        // if (m_bufEnd - m_curPos < pesHeaderLen+1) {
        if (m_bufEnd - m_curPos < pesHeaderLen)
        {
            m_tmpBufferLen = m_bufEnd - m_curPos;
            memmove(&m_tmpBuffer[0], m_curPos, m_tmpBufferLen);
            return NEED_MORE_DATA;
        }
        if (pesPacket->flagsLo & 0x80)
        {  // pts exists
            m_lastDTS = m_lastPTS = getTimeValueNano(pesPacket->getPts());
            m_maxPTS = FFMAX(m_lastPTS, m_maxPTS);
        }
        if ((pesPacket->flagsLo & 0xc0) == 0xc0)  // dts exists
            m_lastDTS = getTimeValueNano(pesPacket->getDts());
        m_curPos += pesHeaderLen;
        m_processedSize += pesHeaderLen;
        m_state = State::stParsePGS;
        // LTRACE(LT_INFO, 2, "PGS PES#" << m_streamIndex << ". PTS=" << m_lastPTS/1e9 << " DTS=" << m_lastDTS/1e9);
        avPacket.pts = m_lastPTS;
        avPacket.dts = m_lastDTS;
        m_isNewFrame = true;
        // m_afterPesByte = *m_curPos;
        if (m_needRescale)
        {
            avPacket.dts = FFMAX(0, avPacket.dts);
            avPacket.pts = FFMAX(0, avPacket.pts);
        }
        return 0;
    }

    avPacket.data = m_curPos;
    if (m_bufEnd - m_curPos < 3)
    {
        m_tmpBufferLen = m_bufEnd - m_curPos;
        memmove(&m_tmpBuffer[0], m_curPos, m_tmpBufferLen);
        return NEED_MORE_DATA;
    }
    uint8_t segment_type = *m_curPos;
    uint16_t segment_len = (uint16_t)AV_RB16(m_curPos + 1);
    if (m_bufEnd - m_curPos < 3ll + segment_len)
    {
        m_tmpBufferLen = m_bufEnd - m_curPos;
        memmove(&m_tmpBuffer[0], m_curPos, m_tmpBufferLen);
        return NEED_MORE_DATA;
    }
    m_curPos += 3;
    BitStreamReader bitReader{};
    try
    {
        int number_of_composition_objects, number_of_windows;
        switch (segment_type)
        {
        case PALETTE_DEF_SEGMENT:
            // pallete definition
            // m_palleteID = m_curPos[0];
            // m_paletteVersion = m_curPos[1];
            if (m_needRescale)
                readPalette(m_curPos, m_curPos + segment_len);
            // LTRACE(LT_INFO, 2, "PGS #" << m_streamIndex << " palette definition");
            break;
        case OBJECT_DEF_SEGMENT:
            // object definition
            // LTRACE(LT_INFO, 2, "PGS #" << m_streamIndex << " object definition size " << segment_len
            //	  << " objectID=" << (int)m_curPos[0] << " version=" << (int) m_curPos[2]);
            if (m_needRescale)
            {
                memset(m_imgBuffer, 0xff, (size_t)m_video_width * m_video_height);
                memset(m_rgbBuffer, 0x00, (size_t)m_video_width * m_video_height * 4);
                memset(m_scaledRgbBuffer, 0x00, (size_t)m_scaled_width * m_scaled_height * 4);
                if (readObjectDef(m_curPos, m_curPos + segment_len) == NEED_MORE_DATA)
                {
                    m_tmpBufferLen = m_bufEnd - m_curPos;
                    memmove(&m_tmpBuffer[0], m_curPos, m_tmpBufferLen);
                    return NEED_MORE_DATA;
                }
                renderTextShow(m_maxPTS);
            }
            break;
        case PCS_DEF_SEGMENT:
            // Presentation Composition Segment
            if (fabs(m_scale - 1.0) > 1e-4)
                m_curPos[4] = calcFpsIndex(m_newFps) << 4;
            bitReader.setBuffer(m_curPos, m_bufEnd);
            video_descriptor(bitReader);
            composition_descriptor(bitReader);

            bitReader.skipBits(16);  // palette_update_flag, palette_id_ref
            number_of_composition_objects = bitReader.getBits(8);
            for (int i = 0; i < number_of_composition_objects; i++)
            {
                composition_object(bitReader);
            }
            if (composition_state != CompositionState::csEpochStart && m_needRescale)
                renderTextHide(m_maxPTS);
            break;
        case WINDOWS_DEF_SEGMENT:
            // Window Definition Segment
            bitReader.setBuffer(m_curPos, m_bufEnd);
            number_of_windows = bitReader.getBits(8);
            for (int i = 0; i < number_of_windows; i++) pgs_window(bitReader);
            // LTRACE(LT_INFO, 2, "PGS #" << m_streamIndex << " Window Definition Segment");
            break;
        case 0x18:
            // Interactive Composition Segment
            // LTRACE(LT_INFO, 2, "PGS #" << m_streamIndex << " Interactive Composition Segment");
            break;
        case 0x80:
            // End of Display Set Segment
            // m_end_display_time = get_pts(curPos);
            // curPos += 5;
            // LTRACE(LT_INFO, 2, "PGS #" << m_streamIndex << " END");
            break;
        case 0x81:
        case 0x82:
            // Used by HDMV Text subtitle streams
            // LTRACE(LT_INFO, 2, "PGS #" << m_streamIndex << " TEXT DATA");
            break;
        default:
            // LTRACE(LT_INFO, 2, "PGS #" << m_streamIndex << " unknown type " << (int)segment_type);
            break;
        }
    }
    catch (BitStreamException& e)
    {
        (void)e;
        return NEED_MORE_DATA;
    }
    avPacket.pts = m_lastPTS;
    avPacket.dts = m_lastDTS;
    if (m_isNewFrame)
    {
        if (!m_needRescale)
            avPacket.flags += AVPacket::FORCE_NEW_FRAME;
        m_isNewFrame = false;
    }

    int avLen = segment_len;
    if (avLen > MAX_AV_PACKET_SIZE - 3)
    {
        avLen = MAX_AV_PACKET_SIZE - 3;
        m_state = State::stAVPacketFragmented;
        m_avFragmentEnd = m_curPos + segment_len;
    }
    m_curPos += avLen;
    m_processedSize += 3ll + avLen;
    avPacket.size = 3ll + avLen;
    if (m_needRescale)
    {
        if (m_renderedBlocks.size() > 0)
        {
            avPacket.pts = (int64_t)(m_renderedBlocks.begin()->pts * INT_FREQ_TO_TS_FREQ);
            avPacket.dts = (int64_t)(m_renderedBlocks.begin()->dts * INT_FREQ_TO_TS_FREQ);
        }
        else
        {
            avPacket.dts = FFMAX(0, avPacket.dts);
            avPacket.pts = FFMAX(0, avPacket.pts);
        }
        avPacket.size = 0;  // skip source palette object
    }
    return 0;
}

int PGSStreamReader::flushPacket(AVPacket& avPacket) { return 0; }

void PGSStreamReader::setBuffer(uint8_t* data, int dataLen, bool lastBlock)
{
    if ((size_t)(m_tmpBufferLen + dataLen) > m_tmpBuffer.size())
        m_tmpBuffer.resize(m_tmpBufferLen + dataLen);

    if (m_tmpBuffer.size() > 0)
        memmove(&m_tmpBuffer[0] + m_tmpBufferLen, data + MAX_AV_PACKET_SIZE, dataLen);
    m_tmpBufferLen += dataLen;

    if (m_tmpBuffer.size() > 0)
        m_curPos = m_buffer = &m_tmpBuffer[0];
    else
        m_curPos = m_buffer = 0;
    m_bufEnd = m_buffer + m_tmpBufferLen;
    m_tmpBufferLen = 0;
}

uint64_t PGSStreamReader::getProcessedSize() { return m_processedSize; }

CheckStreamRez PGSStreamReader::checkStream(uint8_t* buffer, int len, ContainerType containerType,
                                            int containerDataType, int containerStreamIndex)
{
    CheckStreamRez rez;
    if ((containerType == ContainerType::ctM2TS || containerType == ContainerType::ctTS) &&
        containerDataType == TRACKTYPE_PGS)
    {
        rez.codecInfo = pgsCodecInfo;
        rez.streamDescr = "Presentation Graphic Stream";
        if (containerStreamIndex >= 0x1200)
            rez.streamDescr +=
                std::string(" #") + int32ToStr(containerStreamIndex - (V3_flags & 0x1e ? 0x12A0 : 0x1200));
    }
    else if (containerType == ContainerType::ctMKV && containerDataType == TRACKTYPE_PGS)
    {
        rez.codecInfo = pgsCodecInfo;
        rez.streamDescr = "Presentation Graphic Stream";
        rez.streamDescr += std::string(" #") + int32ToStr(containerStreamIndex);
    }
    else if (containerType == ContainerType::ctSUP && len > 2 && buffer[0] == 'P' && buffer[1] == 'G')
    {
        rez.codecInfo = pgsCodecInfo;
        rez.streamDescr = std::string("Presentation Graphic Stream");
    }
    intDecodeStream(buffer, len);
    if (m_video_height != 0)
    {
        std::ostringstream ostr;
        ostr << " Resolution: " << m_video_width << ':' << m_video_height << " Frame rate: " << m_frame_rate;
        rez.streamDescr += ostr.str();
    }
    return rez;
}

void PGSStreamReader::intDecodeStream(uint8_t* buffer, size_t len)
{
    uint8_t* bufEnd = buffer + len;
    uint8_t* curPos = buffer;
    while (curPos < bufEnd)
    {
        if (bufEnd - curPos < 10)
            return;
        if (curPos[0] == 'P' && curPos[1] == 'G')
            curPos += 10;
        else if (curPos[0] == 0 && curPos[1] == 0 && curPos[2] == 1)
        {
            PESPacket* pesPacket = (PESPacket*)curPos;
            curPos += pesPacket->getHeaderLength();
            if (curPos >= bufEnd)
                return;
        }

        uint8_t segment_type = *curPos;
        uint16_t segment_len = (uint16_t)AV_RB16(curPos + 1);
        if (bufEnd - curPos < 3ll + segment_len)
            return;

        curPos += 3;
        if (segment_type == 0x16)
        {
            BitStreamReader bitReader{};
            bitReader.setBuffer(curPos, bufEnd);
            video_descriptor(bitReader);
            return;
        }
        curPos += segment_len;
    }
}

int PGSStreamReader::writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                                       PriorityDataInfo* priorityData)
{
    if (!m_demuxMode)
        return 0;
    if (dstEnd - dstBuffer < 10)
        THROW(ERR_COMMON, "PGS stream error: Not enough buffer for write headers");
    if (!m_needRescale)
    {
        *dstBuffer++ = 'P';
        *dstBuffer++ = 'G';
        uint32_t* data = (uint32_t*)dstBuffer;
        *data++ = my_htonl((uint32_t)nanoClockToPts(m_lastPTS));
        if (m_lastDTS != m_lastPTS)
            *data = my_htonl((uint32_t)nanoClockToPts(m_lastDTS));
        else
            *data = 0;
        return 10;
    }
    else
        return 0;
}

void PGSStreamReader::setVideoInfo(int width, int height, double fps)
{
    m_scaled_width = width;
    m_scaled_height = height;
    m_newFps = fps;
    if (width && height)
        m_render->enlargeCrop(width, height, &m_scaled_width, &m_scaled_height);
    m_render->setVideoInfo(m_scaled_width, m_scaled_height, m_newFps);
}
