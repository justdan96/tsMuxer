#ifndef __VVC_STREAM_READER_H__
#define __VVC_STREAM_READER_H__

#include <map>

#include "abstractDemuxer.h"
#include "mpegStreamReader.h"
#include "vvc.h"

class VVCStreamReader : public MPEGStreamReader
{
   public:
    VVCStreamReader();
    ~VVCStreamReader() override;
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) override;
    virtual CheckStreamRez checkStream(uint8_t* buffer, int len);
    bool needSPSForSplit() const override { return false; }

   protected:
    const CodecInfo& getCodecInfo() override { return vvcCodecInfo; }
    virtual int intDecodeNAL(uint8_t* buff) override;

    double getStreamFPS(void* curNalUnit) override;
    int getStreamWidth() const override;
    int getStreamHeight() const override;
    bool getInterlaced() override { return false; }
    bool isIFrame() override { return m_lastIFrame; }

    void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen) override;
    int getFrameDepth() override { return m_frameDepth; }
    virtual int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                                  PriorityDataInfo* priorityData) override;
    void onSplitEvent() override { m_firstFileFrame = true; }

   private:
    bool isSlice(VvcUnit::NalType nalType) const;
    bool isSuffix(VvcUnit::NalType nalType) const;
    void incTimings();
    int toFullPicOrder(VvcSliceHeader* slice, int pic_bits);
    void storeBuffer(MemoryBlock& dst, const uint8_t* data, const uint8_t* dataEnd);
    uint8_t* writeBuffer(MemoryBlock& srcData, uint8_t* dstBuffer, uint8_t* dstEnd);
    uint8_t* writeNalPrefix(uint8_t* curPos);

   private:
    typedef std::map<int, VvcVpsUnit*> VPSMap;

    VvcVpsUnit* m_vps;
    VvcSpsUnit* m_sps;
    VvcPpsUnit* m_pps;
    VvcSliceHeader* m_slice;
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

#endif  // __VVC_STREAM_READER_H__
