#ifndef WAVE_FORMAT_H_
#define WAVE_FORMAT_H_

#include <types/types.h>

#include "abstractDemuxer.h"

namespace wave_format
{
static constexpr uint32_t SPEAKER_FRONT_LEFT = 0x1;
static constexpr uint32_t SPEAKER_FRONT_RIGHT = 0x2;
static constexpr uint32_t SPEAKER_FRONT_CENTER = 0x4;
static constexpr uint32_t SPEAKER_LOW_FREQUENCY = 0x8;
static constexpr uint32_t SPEAKER_BACK_LEFT = 0x10;
static constexpr uint32_t SPEAKER_BACK_RIGHT = 0x20;
static constexpr uint32_t SPEAKER_FRONT_LEFT_OF_CENTER = 0x40;
static constexpr uint32_t SPEAKER_FRONT_RIGHT_OF_CENTER = 0x80;
static constexpr uint32_t SPEAKER_BACK_CENTER = 0x100;
static constexpr uint32_t SPEAKER_SIDE_LEFT = 0x200;
static constexpr uint32_t SPEAKER_SIDE_RIGHT = 0x400;
static constexpr uint32_t SPEAKER_TOP_CENTER = 0x800;
static constexpr uint32_t SPEAKER_TOP_FRONT_LEFT = 0x1000;
static constexpr uint32_t SPEAKER_TOP_FRONT_CENTER = 0x2000;
static constexpr uint32_t SPEAKER_TOP_FRONT_RIGHT = 0x4000;
static constexpr uint32_t SPEAKER_TOP_BACK_LEFT = 0x8000;
static constexpr uint32_t SPEAKER_TOP_BACK_CENTER = 0x10000;
static constexpr uint32_t SPEAKER_TOP_BACK_RIGHT = 0x20000;
static constexpr uint32_t SPEAKER_RESERVED = 0x80000000;

struct GUID
{
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
    GUID(const uint32_t param1, const uint16_t param2, const uint16_t param3, const char* param4)
    {
        data1 = param1;
        data2 = param2;
        data3 = param3;
        memcpy(data4, param4, 8);
    }
    bool operator==(const GUID& other) const
    {
        if (data1 != other.data1 || data2 != other.data2 || data3 != other.data3)
            return false;
        for (int i = 0; i < 8; i++)
            if (data4[i] != other.data4[i])
                return false;
        return true;
    }
};

typedef struct
{
    // WAVEFORMATEX  Format;
    uint16_t wFormatTag;
    uint16_t nChannels;
    int32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;

    union {
        uint16_t wValidBitsPerSample; /* bits of precision */
        uint16_t wSamplesPerBlock;    /* valid if wBitsPerSample==0 */
        uint16_t wReserved;           /* If neither applies, set to zero. */
    } Samples;
    uint32_t dwChannelMask; /* which channels are present in stream */
    GUID SubFormat;
} WAVEFORMATPCMEX;

constexpr uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;

static GUID KSDATAFORMAT_SUBTYPE_PCM(0x00000001, 0x0000, 0x0010, "\x80\x00\x00\xaa\x00\x38\x9b\x71");
static GUID WAVE64GUID(0x66666972, 0x912E, 0x11CF, "\xA5\xD6\x28\xDB\x04\xC1\x00\x00");

void buildWaveHeader(MemoryBlock& waveBuffer, int samplerate, uint16_t channels, bool lfeExist, uint16_t bitdepth);
uint32_t getWaveChannelMask(int channels, bool lfeExists);
void toLittleEndian(uint8_t* dstData, uint8_t* srcData, int size, int bitdepth);
}  // namespace wave_format

#endif
