#ifndef __HEVC_STREAM_READER_H__
#define __HEVC_STREAM_READER_H__

#include <map>
#include "mpegStreamReader.h"
#include "hevc.h"
#include "abstractDemuxer.h"

class HEVCStreamReader: public MPEGStreamReader 
{
public:
    HEVCStreamReader();
    virtual ~HEVCStreamReader();
    virtual int getTSDescriptor(uint8_t* dstBuff);
    virtual CheckStreamRez checkStream(uint8_t* buffer, int len);
    virtual bool needSPSForSplit() const override { return false; }
protected:
    virtual const CodecInfo& getCodecInfo() override {return hevcCodecInfo;};
    virtual int intDecodeNAL(uint8_t* buff) override;

    virtual double getStreamFPS(void * curNalUnit) override;
    virtual int getStreamWidth() const  override;
    virtual int getStreamHeight() const  override;
    virtual bool getInterlaced()  override {return false;}
    virtual bool isIFrame() {return m_lastIFrame;}

    virtual void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen) override;
    virtual int getFrameDepth() override { return m_frameDepth; }
    virtual int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket, PriorityDataInfo* priorityData) override;
    virtual void onSplitEvent() { m_firstFileFrame = true; }
private:
    bool isSlice(int nalType) const;
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

#endif // __HEVC_STREAM_READER_H__
