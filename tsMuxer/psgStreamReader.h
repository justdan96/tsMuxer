#ifndef __PGS_STREAM_READER_H
#define __PGS_STREAM_READER_H

#include <vector>

#include "abstractStreamReader.h"
#include "avCodecs.h"
#include "avPacket.h"
#include "bitStream.h"
#include "textSubtitles.h"
#include "textSubtitlesRender.h"

class PGSStreamReader : public AbstractStreamReader
{
   public:
    struct BitmapInfo
    {
        int Width;
        int Height;
#ifdef _WIN32
        RGBQUAD* buffer;  // rgb triple buffer
#else
        text_subtitles::RGBQUAD* buffer;
#endif
    };

    PGSStreamReader();
    ~PGSStreamReader() override
    {
        delete[] m_imgBuffer;
        delete[] m_rgbBuffer;
        delete[] m_scaledRgbBuffer;
        delete[] m_renderedData;
        delete m_render;
    }
    int readPacket(AVPacket& avPacket) override;
    int flushPacket(AVPacket& avPacket) override;
    void setBuffer(uint8_t* data, int dataLen, bool lastBlock = false) override;
    uint64_t getProcessedSize() override;
    CheckStreamRez checkStream(uint8_t* buffer, int len, ContainerType containerType, int containerDataType,
                               int containerStreamIndex);
    const CodecInfo& getCodecInfo() override { return pgsCodecInfo; }
    // void setDemuxMode(bool value) {m_demuxMode = value;}
    static int calcFpsIndex(double fps);

    // void setVideoWidth(int value);
    // void setVideoHeight(int value);
    // void setFPS(double value);
    void setVideoInfo(int width, int height, double fps);
    void setFontBorder(int value) { m_fontBorder = value; }
    void setBottomOffset(int value) { m_render->setBottomOffset(value); }
    void setOffsetId(int value) { m_offsetId = value; }
    int getOffsetId() const { return m_offsetId; }

    // SS PG data
    bool isSSPG;
    int leftEyeSubStreamIdx;
    int rightEyeSubStreamIdx;
    int ssPGOffset;

   protected:
    int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                          PriorityDataInfo* priorityData) override;

   private:
    struct PGSRenderedBlock
    {
        PGSRenderedBlock(int64_t _pts, int64_t _dts, int _len, uint8_t* _data)
            : pts(_pts), dts(_dts), len(_len), data(_data)
        {
        }
        int64_t pts;
        int64_t dts;
        int len;
        uint8_t* data;
    };

    enum State
    {
        stParsePES,
        stParsePGS,
        stAVPacketFragmented
    };
    enum CompositionState
    {
        csNormalCase,
        csAcquisitionPoint,
        csEpochStart,
        csEpochContinue
    };
    State m_state;
    uint8_t* m_curPos;
    uint8_t* m_buffer;
    int m_tmpBufferLen;
    std::vector<uint8_t> m_tmpBuffer;
    int64_t m_lastPTS;
    int64_t m_maxPTS;
    int64_t m_lastDTS;
    uint64_t m_processedSize;
    uint8_t* m_avFragmentEnd;
    int m_afterPesByte;
    int m_fontBorder;

    int m_video_width;
    int m_video_height;
    double m_frame_rate;
    double m_newFps;
    double m_scale;
    std::vector<uint8_t> m_dstRle;
    bool m_needRescale;

    uint16_t object_width;
    uint16_t object_height;
    uint8_t* m_imgBuffer;
    uint8_t* m_rgbBuffer;
    uint8_t* m_scaledRgbBuffer;
    uint16_t window_horizontal_position;
    uint16_t window_vertical_position;
    std::map<uint8_t, text_subtitles::YUVQuad> m_palette;
    int m_scaled_width;
    int m_scaled_height;
    int64_t m_ptsStart;
    bool m_firstRenderedPacket;

    text_subtitles::TextToPGSConverter* m_render;
    uint32_t m_renderedLen;
    uint8_t* m_renderedData;
    std::vector<PGSRenderedBlock> m_renderedBlocks;

    std::map<int, int> composition_object_horizontal_position;
    std::map<int, int> composition_object_vertical_position;
    void renderTextShow(int64_t startTime);
    void renderTextHide(int64_t outTime);

    void video_descriptor(BitStreamReader& bitReader);
    void composition_descriptor(BitStreamReader& bitReader);
    void composition_object(BitStreamReader& bitReader);
    void pgs_window(BitStreamReader& bitReader);
    void readPalette(uint8_t* pos, uint8_t* end);
    int readObjectDef(uint8_t* pos, uint8_t* end);
    void decodeRleData(int xOffset, int yOffset);
    void yuvToRgb(int minY);
    void rescaleRGB(BitmapInfo* bmpDest, BitmapInfo* bmpRef);
    void intDecodeStream(uint8_t* buffer, int len);

    int m_palleteID;
    int m_paletteVersion;
    bool m_isNewFrame;
    int m_objectWindowHeight;
    int m_objectWindowTop;
    uint8_t m_offsetId;
    int m_forced_on_flag;

    CompositionState composition_state;
    int composition_number;
};

#endif
