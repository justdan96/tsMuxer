#ifndef H264_STREAM_READER_H_
#define H264_STREAM_READER_H_

#include <map>

#include "avPacket.h"
#include "mpegStreamReader.h"
#include "nalUnits.h"
#include "vod_common.h"

class H264StreamReader : public MPEGStreamReader
{
   public:
    enum class SeiMethod
    {
        SEI_DoNotInsert,
        SEI_InsertAuto,
        SEI_InsertForce,
        SEI_NotDefined
    };

    H264StreamReader();
    ~H264StreamReader() override;
    void setForceLevel(const uint8_t value) { m_forcedLevel = value; }
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode, bool hdmvDescriptors) override;
    virtual CheckStreamRez checkStream(uint8_t* buffer, int len);
    void setH264SPSCont(const bool val) { m_h264SPSCont = val; }

    void setInsertSEI(const SeiMethod value) { m_insertSEIMethod = value; }
    SeiMethod getInsertSEI() const { return m_insertSEIMethod; }

    void setIsSubStream(const bool value) { m_mvcSubStream = value; }
    bool isSubStream() const { return m_mvcSubStream; }

    int getOffsetSeqCnt() const { return number_of_offset_sequences; }

    // used for correction offset metadata
    virtual void setStartPTS(const int64_t pts) { m_startPts = pts; }
    bool needSPSForSplit() const override { return true; }

   protected:
    void onSplitEvent() override { m_firstFileFrame = true; }
    const CodecInfo& getCodecInfo() override;
    int intDecodeNAL(uint8_t* buff) override;
    void updateStreamFps(void* nalUnit, uint8_t* buff, uint8_t* nextNal, int oldSpsLen) override;
    double getStreamFPS(void* curNalUnit) override
    {
        const auto sps = static_cast<SPSUnit*>(curNalUnit);
        return sps->getFPS();
    }

    int writeAdditionData(uint8_t* dstBuffer, uint8_t* dstEnd, AVPacket& avPacket,
                          PriorityDataInfo* priorityData) override;
    int getFrameDepth() override { return m_frameDepth; }
    int getStreamWidth() const override;
    int getStreamHeight() const override;
    int getStreamHDR() const override { return 0; }
    bool getInterlaced() override;
    bool isIFrame() override { return m_lastIFrame; }
    // virtual bool isIFrame() { return m_lastSliceIDR; }

    bool isPriorityData(AVPacket* packet) override;
    void onShiftBuffer(int offset) override;
    bool skipNal(uint8_t* nal) override;

   private:
    bool replaceToOwnSPS() const;
    int deserializeSliceHeader(SliceUnit& slice, const uint8_t* buff, const uint8_t* sliceEnd);
    void checkPyramid(int frameNum, int* fullPicOrder, bool nextFrameFound);

   private:
    bool m_nextFrameFound;
    bool m_nextFrameIdr;

    // bool m_idrSliceFound;
    uint32_t m_idrSliceCnt;
    bool m_bSliceFound;
    int m_picOrderOffset;
    int m_frameDepth;

    int m_spsCounter;
    bool m_h264SPSCont;
    bool m_lastSliceIDR;
    int m_lastSliceSPS;
    int m_lastSlicePPS;
    bool m_firstAUDWarn;
    bool m_firstSPSWarn;
    bool m_firstSEIWarn;
    bool m_delimiterFound;
    uint8_t m_forcedLevel;
    int m_pict_type;
    int m_iFramePtsOffset;
    int prevPicOrderCntMsb;
    int prevPicOrderCntLsb;
    int m_forceLsbDiv;
    int m_lastMessageLen;
    bool m_isFirstFrame;
    SeiMethod m_insertSEIMethod;
    bool m_needSeiCorrection;
    int number_of_offset_sequences;
    int m_frameNum;
    bool m_lastIFrame;
    bool m_firstDecodeNal;
    int m_lastPictStruct;
    // bool m_openGOP;
    bool m_firstFileFrame;
    std::map<uint32_t, SPSUnit*> m_spsMap;
    std::map<uint32_t, PPSUnit*> m_ppsMap;
    std::set<uint32_t> updatedSPSList;
    std::vector<uint8_t> m_lastSeiMvcHeader;
    int m_lastPicStruct;
    int64_t m_lastDtsInc;
    bool orig_hrd_parameters_present_flag;
    bool orig_vcl_parameters_present_flag;
    bool m_mvcSubStream;
    bool m_mvcPrimaryStream;
    bool m_blurayMode;
    int64_t m_startPts;

    void additionalStreamCheck(uint8_t* buff, uint8_t* end);
    int calcPicOrder(const SliceUnit& slice);
    int getIdrPrevFrames(uint8_t* buff, const uint8_t* bufEnd);
    int processSliceNal(uint8_t* buff);
    int processSPS(uint8_t* buff);
    int processPPS(uint8_t* buff);
    int detectPrimaryPicType(const SliceUnit& firstSlice, uint8_t* buff);
    int sliceTypeToPictType(int slice_type) const;
    uint8_t* writeNalPrefix(uint8_t* curPos) const;
    bool findPPSForward(uint8_t* buff);
    void updateHRDParam(SPSUnit* sps) const;
    int processSEI(uint8_t* buff);
    int getNalHrdLen(const uint8_t* nal) const;
    int writeSEIMessage(uint8_t* dstBuffer, const uint8_t* dstEnd, const SEIUnit& sei, uint8_t payloadType) const;

    std::vector<uint8_t> m_bdRomMetaDataMsg;  // copy metadata to buffer, then isert during sei rebuild process
    int m_bdRomMetaDataMsgPtsPos;
    uint8_t* m_priorityNalAddr;  // just correct pts, keep other data unchanged
    uint8_t* m_OffsetMetadataPtsAddr;
    std::vector<uint8_t> m_decodedSliceHeader;
    int m_removalDelay;
    bool m_spsChangeWarned;
};

#endif
