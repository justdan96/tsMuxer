#include "wave.h"

#include "vodCoreException.h"

namespace wave_format
{
uint32_t getWaveChannelMask(const int channels, const bool lfeExists)
{
    switch (channels)
    {
    case 1:
        return SPEAKER_FRONT_CENTER;
    case 2:
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT;
    case 3:
    {
        if (lfeExists)
            return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_LOW_FREQUENCY;
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_CENTER;
    }
    case 4:
    {
        if (lfeExists)
            return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_FRONT_CENTER + SPEAKER_LOW_FREQUENCY;
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT + SPEAKER_BACK_LEFT + SPEAKER_BACK_RIGHT;
    }
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
    default:;
    }
    return 0;  // unknown value
}

void buildWaveHeader(MemoryBlock& waveBuffer, const int samplerate, const uint16_t channels, const bool lfeExist,
                     const uint16_t bitdepth)
{
    waveBuffer.clear();
    waveBuffer.grow(40 + 28);
    uint8_t* curPos = waveBuffer.data();
    for (const char c : "RIFF\x00\x00\x00\x00WAVEfmt ") *curPos++ = c;
    const auto fmtSize = reinterpret_cast<uint32_t*>(curPos);
    *fmtSize = sizeof(WAVEFORMATPCMEX);
    curPos += 4;
    const auto waveFormatPCMEx = reinterpret_cast<WAVEFORMATPCMEX*>(curPos);

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
    for (const char c : "data\x00\x00\x00\x00") *curPos++ = c;
}

void toLittleEndian(uint8_t* dstData, uint8_t* srcData, const int size, const int bitdepth)
{
    if (bitdepth == 16)
    {
        auto dst = reinterpret_cast<uint16_t*>(dstData);
        auto src = reinterpret_cast<uint16_t*>(srcData);
        const auto srcEnd = reinterpret_cast<uint16_t*>(srcData + size);
        while (src < srcEnd) *dst++ = my_ntohs(*src++);
    }
    else if (bitdepth > 16)
    {
        uint8_t* dst = dstData;
        const uint8_t* src = srcData;
        const uint8_t* srcEnd = srcData + size;
        while (src < srcEnd)
        {
            const uint8_t tmp = src[0];
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = tmp;
            dst += 3;
            src += 3;
        }
    }
    else
    {
        THROW(ERR_WAV_PARSE, "Unsupported LPCM big depth " << bitdepth << " for /LIT codec")
    }
}

}  // namespace wave_format
