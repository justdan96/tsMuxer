#include "wave.h"

#include "vodCoreException.h"

namespace wave_format
{
uint32_t getWaveChannelMask(int channels, bool lfeExists)
{
    switch (channels)
    {
    case 1:
        return SPEAKER_FRONT_CENTER;
    case 2:
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT;
    case 3:
        if (lfeExists)
            return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_LOW_FREQUENCY;
        else
            return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_CENTER;
    case 4:
        if (lfeExists)
            return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_CENTER + SPEAKER_LOW_FREQUENCY;
        else
            return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_BACK_LEFT + SPEAKER_BACK_RIGHT;
    case 5:
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_CENTER + SPEAKER_SIDE_LEFT + SPEAKER_SIDE_RIGHT;
    case 6:
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_CENTER + SPEAKER_SIDE_LEFT +
               SPEAKER_SIDE_RIGHT + SPEAKER_LOW_FREQUENCY;
    case 7:
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_CENTER + SPEAKER_BACK_LEFT +
               SPEAKER_BACK_RIGHT + SPEAKER_SIDE_LEFT + SPEAKER_SIDE_RIGHT;
    case 8:
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_CENTER + SPEAKER_BACK_LEFT +
               SPEAKER_BACK_RIGHT + SPEAKER_SIDE_LEFT + SPEAKER_SIDE_RIGHT + SPEAKER_LOW_FREQUENCY;
    }
    return 0;  // unknown value
}

void buildWaveHeader(MemoryBlock& waveBuffer, int samplerate, int channels, bool lfeExist, int bitdepth)
{
    waveBuffer.clear();
    waveBuffer.grow(40 + 28);
    uint8_t* curPos = waveBuffer.data();
    memcpy(curPos, "RIFF\x00\x00\x00\x00WAVEfmt ", 16);
    curPos += 16;
    uint32_t* fmtSize = (uint32_t*)curPos;
    *fmtSize = sizeof(WAVEFORMATPCMEX);
    curPos += 4;
    WAVEFORMATPCMEX* waveFormatPCMEx = (WAVEFORMATPCMEX*)curPos;

    waveFormatPCMEx->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    waveFormatPCMEx->nChannels = channels;
    waveFormatPCMEx->nSamplesPerSec = samplerate;
    waveFormatPCMEx->nAvgBytesPerSec = channels * samplerate * ((bitdepth + 4) >> 3);
    waveFormatPCMEx->nBlockAlign = channels * waveFormatPCMEx->wBitsPerSample / 8;
    waveFormatPCMEx->wBitsPerSample = bitdepth == 20 ? 24 : bitdepth;
    waveFormatPCMEx->cbSize = 22;  // After this to GUID
    waveFormatPCMEx->Samples.wValidBitsPerSample = bitdepth;
    waveFormatPCMEx->dwChannelMask = getWaveChannelMask(channels, lfeExist);  // Specify PCM
    waveFormatPCMEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    curPos += sizeof(WAVEFORMATPCMEX);
    memcpy(curPos, "data\x00\x00\x00\x0", 8);
}

void toLittleEndian(uint8_t* dstData, const uint8_t* srcData, int size, int bitdepth)
{
    if (bitdepth == 16)
    {
        uint16_t* dst = (uint16_t*)dstData;
        const uint16_t* src = (const uint16_t*)srcData;
        const uint16_t* srcEnd = (const uint16_t*)(srcData + size);
        while (src < srcEnd) *dst++ = my_ntohs(*src++);
    }
    else if (bitdepth > 16)
    {
        uint8_t* dst = dstData;
        const uint8_t* src = srcData;
        const uint8_t* srcEnd = srcData + size;
        while (src < srcEnd)
        {
            uint8_t tmp = src[0];
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = tmp;
            dst += 3;
            src += 3;
        }
    }
    else
    {
        THROW(ERR_WAV_PARSE, "Unsupported LPCM big depth " << bitdepth << " for /LIT codec");
    }
}

}  // namespace wave_format