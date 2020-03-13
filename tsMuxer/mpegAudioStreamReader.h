#ifndef __MPEG_AUDIO_STREAM_READER
#define __MPEG_AUDIO_STREAM_READER

#include "mp3Codec.h"
#include "simplePacketizerReader.h"

class MpegAudioStreamReader : public SimplePacketizerReader, MP3Codec
{
   public:
    const static uint32_t DTS_HD_PREFIX = 0x64582025;
    MpegAudioStreamReader() : SimplePacketizerReader() {}
    int getTSDescriptor(uint8_t* dstBuff, bool blurayMode) override;
    int getLayer() { return m_layer; }
    int getFreq() override { return m_sample_rate; }
    int getChannels() override { return 2; }

   protected:
    unsigned getHeaderLen() override { return MPEG_AUDIO_HEADER_SIZE; };
    virtual uint8_t* findFrame(uint8_t* buff, uint8_t* end) override;
    int decodeFrame(uint8_t* buff, uint8_t* end, int& skipBytes, int& skipBeforeBytes) override;
    double getFrameDurationNano() override;
    const CodecInfo& getCodecInfo() override { return mpegAudioCodecInfo; }
    const std::string getStreamInfo() override;

   private:
    static const int MPEG_AUDIO_HEADER_SIZE = 4;
};

#endif
