#ifndef PGS_STREAM_READER_H_
#define PGS_STREAM_READER_H_

#include <vector>

#include "abstractStreamReader.h"
#include "avCodecs.h"
#include "avPacket.h"
#include "bitStream.h"
#include "textSubtitles.h"
#include "textSubtitlesRender.h"

class PGSStreamReader final : public AbstractStreamReader
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
    void setBuffer(uint8_t* data, uint32_t dataLen, bool lastBlock = false) override;
    int64_t getProcessedSize() override;
    CheckStreamRez checkStream(uint8_t* buffer, int len, ContainerType containerType, int containerDataType,
                               int containerStreamIndex);
    const CodecInfo& getCodecInfo() override { return pgsCodecInfo; }
    // void setDemuxMode(bool value) {m_demuxMode = value;}
    static int calcFpsIndex(double fps);

    // void setVideoWidth(int value);
    // void setVideoHeight(int value);
    // void setFPS(double value);
    void setVideoInfo(uint16_t width, uint16_t height, double fps);
    void setFontBorder(const int value) { m_fontBorder = value; }
    void setBottomOffset(const int value) const { m_render->setBottomOffset(value); }
    void setOffsetId(const uint8_t value) { m_offsetId = value; }
    uint8_t getOffsetId() const { return m_offsetId; }

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
        unsigned len;
        uint8_t* data;
    };

    enum class State
    {
        stParsePES,
        stParsePGS,
        stAVPacketFragmented
    };
    enum class CompositionState
    {
        csNormalCase,
        csAcquisitionPoint,
        csEpochStart,
        csEpochContinue
    };
    State m_state;
    // JCDR : the three fields below overshadow the fileds in AbstractDemuxer
    // See whether they can be removed.
    // uint8_t* m_curPos;
    // uint8_t* m_buffer;
    // size_t m_tmpBufferLen;
    std::vector<uint8_t> m_tmpBuffer;
    int64_t m_lastPTS;
    int64_t m_maxPTS;
    int64_t m_lastDTS;
    int64_t m_processedSize;
    uint8_t* m_avFragmentEnd;
    int m_afterPesByte;
    int m_fontBorder;

    uint16_t m_video_width;
    uint16_t m_video_height;
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
    std::map<uint8_t, text_subtitles::YUVQuad> m_palette;
    uint16_t m_scaled_width;
    uint16_t m_scaled_height;
    bool m_firstRenderedPacket;

    text_subtitles::TextToPGSConverter* m_render;
    uint8_t* m_renderedData;
    std::vector<PGSRenderedBlock> m_renderedBlocks;

    std::map<uint16_t, uint16_t> composition_object_horizontal_position;
    std::map<uint16_t, uint16_t> composition_object_vertical_position;
    void renderTextShow(int64_t inTime);
    void renderTextHide(int64_t outTime);

    void video_descriptor(BitStreamReader& bitReader);
    void composition_descriptor(BitStreamReader& bitReader);
    void composition_object(BitStreamReader& bitReader);
    static void pgs_window(BitStreamReader& bitReader);
    void readPalette(const uint8_t* pos, const uint8_t* end);
    int readObjectDef(const uint8_t* pos, const uint8_t* end);
    void decodeRleData(int xOffset, int yOffset) const;
    void yuvToRgb(int minY) const;
    static void rescaleRGB(const BitmapInfo* bmpDest, const BitmapInfo* bmpRef);
    void intDecodeStream(uint8_t* buffer, size_t len);

    int m_palleteID;
    int m_paletteVersion;
    bool m_isNewFrame;
    uint16_t m_objectWindowHeight;
    uint16_t m_objectWindowTop;
    uint8_t m_offsetId;
    bool m_forced_on_flag;

    CompositionState composition_state;
};

#endif
