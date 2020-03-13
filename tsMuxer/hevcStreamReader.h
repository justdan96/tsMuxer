#ifndef __HEVC_STREAM_READER_H__
#define __HEVC_STREAM_READER_H__

#include <map>

#include "abstractDemuxer.h"
#include "hevc.h"
#include "mpegStreamReader.h"

class HEVCStreamReader : public MPEGStreamReader
{
   public:
    HEVCStreamReader();
    ~HEVCStreamReader() override;
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode) override;
    int setDoViDescriptor(uint8_t* dstBuff);
    virtual CheckStreamRez checkStream(uint8_t* buffer, int len);
    bool needSPSForSplit() const override { return false; }

   protected:
    const CodecInfo& getCodecInfo() override { return hevcCodecInfo; }
    virtual int intDecodeNAL(uint8_t* buff) override;

    double getStreamFPS(void* curNalUnit) override;
    int getStreamWidth() const override;
    int getStreamHeight() const override;
    int getStreamHDR() const override;
    bool getInterlaced() override { return false; }
    bool isIFrame() override { return m_lastIFrame; }

    void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen) override;
    int getFrameDepth() override { return m_frameDepth; }
    virtual int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                                  PriorityDataInfo* priorityData) override;
    void onSplitEvent() override { m_firstFileFrame = true; }

   private:
    bool isSlice(int nalType) const;
    bool isSuffix(int nalType) const;
    void incTimings();
    int toFullPicOrder(HevcSliceHeader* slice, int pic_bits);
    void storeBuffer(MemoryBlock& dst, const uint8_t* data, const uint8_t* dataEnd);
    uint8_t* writeBuffer(MemoryBlock& srcData, uint8_t* dstBuffer, uint8_t* dstEnd);
    uint8_t* writeNalPrefix(uint8_t* curPos);

   private:
    typedef std::map<int, HevcVpsUnit*> VPSMap;

    HevcVpsUnit* m_vps;
    HevcSpsUnit* m_sps;
    HevcPpsUnit* m_pps;
    HevcHdrUnit* m_hdr;
    bool m_firstFrame;

    int m_frameNum;
    int m_fullPicOrder;
    int m_picOrderBase;
    int m_frameDepth;

    int m_picOrderMsb;
    int m_prevPicOrder;
    bool m_lastIFrame;

    MemoryBlock m_vpsBuffer;
    MemoryBlock m_spsBuffer;
    MemoryBlock m_ppsBuffer;
    bool m_firstFileFrame;
    int m_vpsCounter;
    int m_vpsSizeDiff;
};

#endif  // __HEVC_STREAM_READER_H__
