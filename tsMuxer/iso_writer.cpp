#include "iso_writer.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "convertUTF.h"
#include "utf8Converter.h"
#include "vod_common.h"

// ----------- routines --------------

namespace
{
/*
  Name  : CRC-16 CCITT
  Poly  : 0x1021    x^16 + x^12 + x^5 + 1
*/

const uint16_t Crc16Table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD,
    0xE1CE, 0xF1EF, 0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A,
    0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B,
    0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861,
    0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96,
    0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87,
    0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
    0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3,
    0x5004, 0x4025, 0x7046, 0x6067, 0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290,
    0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E,
    0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F,
    0x99C8, 0x89E9, 0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3, 0xCB7D, 0xDB5C,
    0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83,
    0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
    0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};

unsigned short crc16(unsigned char* pcBlock, unsigned short len)
{
    unsigned short crc = 0;

    while (len--) crc = (crc << 8) ^ Crc16Table[(crc >> 8) ^ *pcBlock++];

    return crc;
}

void writeDescriptorTag(uint8_t* buffer, uint16_t tag, uint32_t tagLocation)
{
    uint16_t* buff16 = (uint16_t*)buffer;
    uint32_t* buff32 = (uint32_t*)buffer;
    buff16[0] = tag;
    buff16[1] = 0x03;  // version
    // skip check sum and reserved byte here
    buff16[3] = 0x03;  // Tag Serial Number
    // skip Descriptor CRC
    // Descriptor CRC Length

    buff32[3] = tagLocation;  // tag location
}

std::string toIsoSeparator(const std::string& path)
{
    std::string result = path;
    for (auto& i : result)
    {
        if (i == '\\')
            i = '/';
    }
    return result;
}

void calcDescriptorCRC(uint8_t* buffer, uint16_t len)
{
    uint16_t* buff16 = (uint16_t*)buffer;

    // calc crc
    buff16[4] = crc16(buffer + 16, len - 16);
    buff16[5] = len - 16;

    // calc tag checksum
    uint8_t sum = 0;
    for (int i = 0; i < 16; ++i) sum += buffer[i];
    buffer[4] = sum;
}

void writeTimestamp(uint8_t* buffer, time_t time)
{
    uint16_t* buff16 = (uint16_t*)buffer;

    const tm* parts = localtime(&time);

    time_t lt = mktime(localtime(&time));
    time_t gt = mktime(gmtime(&time));
    int16_t timeZone = (lt - gt) / 60;

    buff16[0] = (1 << 12) + (timeZone & 0x0fff);
    buff16[1] = parts->tm_year + 1900;
    buffer[4] = parts->tm_mon + 1;
    buffer[5] = parts->tm_mday;
    buffer[6] = parts->tm_hour;
    buffer[7] = parts->tm_min;
    buffer[8] = parts->tm_sec;
    buffer[9] = 0;  // ms parts
    buffer[10] = 0;
    buffer[11] = 0;
}

bool canUse8BitUnicode(const std::string& utf8Str)
{
    bool rv = true;
    convertUTF::IterateUTF8Chars(utf8Str, [&](auto c) {
        rv = (c < 0x100);
        return rv;
    });
    return rv;
}

std::vector<std::uint8_t> serializeDString(const std::string& str, int fieldLen)
{
    if (str.empty())
    {
        return std::vector<std::uint8_t>(fieldLen, 0);
    }
    std::vector<std::uint8_t> rv;
#ifdef _WIN32
    auto str_u8 = reinterpret_cast<const std::uint8_t*>(str.c_str());
    auto utf8Str = convertUTF::isLegalUTF8String(str_u8, str.length())
                       ? str
                       : UtfConverter::toUtf8(str_u8, str.length(), UtfConverter::sfANSI);
#else
    auto& utf8Str = str;
#endif
    using namespace convertUTF;
    const unsigned maxHeaderAndContentLength = fieldLen - 1;
    rv.reserve(fieldLen);
    if (canUse8BitUnicode(utf8Str))
    {
        rv.push_back(8);
        IterateUTF8Chars(utf8Str, [&](auto c) {
            rv.push_back(c);
            return rv.size() < maxHeaderAndContentLength;
        });
    }
    else
    {
        rv.push_back(16);
        IterateUTF8Chars(utf8Str, [&](auto c) {
            UTF16 high_surrogate, low_surrogate;
            std::tie(high_surrogate, low_surrogate) = ConvertUTF32toUTF16(c);
            auto spaceLeft = maxHeaderAndContentLength - rv.size();
            if ((spaceLeft < 2) || (low_surrogate && spaceLeft < 4))
            {
                return false;
            }
            rv.push_back(high_surrogate >> 8);
            rv.push_back(high_surrogate);
            if (low_surrogate)
            {
                rv.push_back(low_surrogate >> 8);
                rv.push_back(low_surrogate);
            }
            return true;
        });
    }
    auto contentLength = rv.size();
    auto paddingSize = maxHeaderAndContentLength - rv.size();
    std::fill_n(std::back_inserter(rv), paddingSize, 0);
    rv.push_back(contentLength);
    return rv;
}

void writeDString(uint8_t* buffer, const char* value, int fieldLen)
{
    auto content = serializeDString(value, fieldLen);
    assert(content.size() == fieldLen);
    std::copy(std::begin(content), std::end(content), buffer);
}

void writeUDFString(uint8_t* buffer, const char* str, int len)
{
    strcpy((char*)buffer + 1, str);
    buffer[len - 8] = 0x50;  // UDF suffix
    buffer[len - 7] = 0x02;  // UDF suffix
}

void writeLongAD(uint8_t* buffer, uint32_t lenBytes, uint32_t pos, uint16_t partition, uint32_t id)
{
    uint32_t* buff32 = (uint32_t*)buffer;
    uint16_t* buff16 = (uint16_t*)buffer;

    buff32[0] = lenBytes;   // len
    buff32[1] = pos;        // location, block number
    buff16[4] = partition;  // location, partition number
    buff32[3] = id;
}

}  // namespace

// --------------------- ByteFileWriter ---------------------

ByteFileWriter::ByteFileWriter() : m_buffer(0), m_bufferEnd(0), m_curPos(0), m_tagPos(0) {}

void ByteFileWriter::setBuffer(uint8_t* buffer, int len)
{
    m_buffer = buffer;
    m_bufferEnd = buffer + len;
    m_curPos = buffer;
}

void ByteFileWriter::writeLE8(uint8_t value) { *m_curPos++ = value; }

void ByteFileWriter::writeLE16(uint16_t value)
{
    uint16_t* pos16 = (uint16_t*)m_curPos;
    *pos16 = value;
    m_curPos += 2;
}

void ByteFileWriter::writeLE32(uint16_t value)
{
    uint32_t* pos32 = (uint32_t*)m_curPos;
    *pos32 = value;
    m_curPos += 4;
}

void ByteFileWriter::writeDescriptorTag(DescriptorTag tag, uint32_t tagLocation)
{
    ::writeDescriptorTag(m_curPos, tag, tagLocation);
    m_tagPos = m_curPos;
    m_curPos += 16;
}

void ByteFileWriter::closeDescriptorTag(int dataSize)
{
    if (dataSize == -1)
        dataSize = m_curPos - m_tagPos;
    calcDescriptorCRC(m_tagPos, dataSize);
}

void ByteFileWriter::writeIcbTag(uint8_t fileType)
{
    uint32_t* buff32 = (uint32_t*)m_curPos;
    uint16_t* buff16 = (uint16_t*)m_curPos;

    // icb tag
    buff32[0] = 0;  // Prior Recorded Number of Direct Entries
    buff16[2] = 4;  // Strategy Type
    buff16[3] = 0;  // Strategy parameters
    buff16[4] = 1;  // Maximum Number of Entries
    // skip reserved byte
    m_curPos[11] = fileType;  // metadata file type
    // skip 6 byte zero Parent ICB Location
    buff16[18 / 2] = 0x20;  // flags. Archive: This bit shall be set to ONE when the file is created or is written.

    m_curPos += 20;
}

void ByteFileWriter::writeLongAD(uint32_t lenBytes, uint32_t pos, uint16_t partition, uint32_t id)
{
    ::writeLongAD(m_curPos, lenBytes, pos, partition, id);
    m_curPos += 16;
}

void ByteFileWriter::writeDString(const char* value, int len)
{
    int writeLen = len;
    if (writeLen == -1)
        writeLen = strlen(value) + 2;
    ::writeDString(m_curPos, value, writeLen);
    m_curPos += writeLen;
}

void ByteFileWriter::writeDString(const std::string& value, int len) { writeDString(value.c_str(), len); }

void ByteFileWriter::doPadding(int padSize)
{
    m_curPos--;
    int rest = (m_curPos - m_buffer) % padSize;
    if (rest)
    {
        rest = padSize - rest;
        for (int i = 0; i < rest; ++i) *m_curPos++ = 0;
    }
}

void ByteFileWriter::writeCharSpecString(const char* value, int len)
{
    strcpy((char*)m_curPos + 1, value);
    m_curPos += len;
}

void ByteFileWriter::writeUDFString(const char* value, int len)
{
    ::writeUDFString(m_curPos, value, len);
    m_curPos += len;
}

void ByteFileWriter::skipBytes(int value) { m_curPos += value; }

void ByteFileWriter::writeTimestamp(time_t time)
{
    ::writeTimestamp(m_curPos, time);
    m_curPos += 12;
};

int ByteFileWriter::size() const { return m_curPos - m_buffer; }

// ------------------------------ FileEntryInfo ------------------------------------

FileEntryInfo::FileEntryInfo(IsoWriter* owner, FileEntryInfo* parent, uint32_t objectId, FileTypes fileType)
    : m_owner(owner),
      m_parent(parent),
      m_sectorNum(0),
      m_sectorsUsed(0),
      m_objectId(objectId),
      m_fileSize(0),
      m_sectorBufferSize(0),
      m_fileType(fileType),
      m_subMode(false)
{
    if (isFile())
        m_sectorBuffer = new uint8_t[SECTOR_SIZE];
    else
        m_sectorBuffer = 0;
}

bool FileEntryInfo::isFile() const { return m_fileType == FileType_File || m_fileType == FileType_RealtimeFile; }

FileEntryInfo::~FileEntryInfo()
{
    delete[] m_sectorBuffer;
    for (int i = 0; i < m_files.size(); ++i) delete m_files[i];
    for (int i = 0; i < m_subDirs.size(); ++i) delete m_subDirs[i];
}

bool FileEntryInfo::setName(const std::string& name)
{
    m_name = name;
    return true;
}

void FileEntryInfo::addSubDir(FileEntryInfo* dir) { m_subDirs.push_back(dir); }

void FileEntryInfo::addFile(FileEntryInfo* file) { m_files.push_back(file); }

void FileEntryInfo::writeEntity(ByteFileWriter& writer, FileEntryInfo* subDir)
{
    bool isSystemFile = (m_objectId == 0);

    writer.writeDescriptorTag(DESC_TYPE_FileId, m_owner->absoluteSectorNum() + 1);
    writer.writeLE16(0x01);  // File Version Number
    writer.writeLE8(!subDir->isFile() ? 0x02
                                      : (isSystemFile ? 0x10 : 0));  // File Characteristics, 'directory' bit (1-th)
    writer.writeLE8(subDir->m_name.length() + 1);                    // Length of File Identifier (=L_FI)
    writer.writeLongAD(0x800, subDir->m_sectorNum, 0x01, subDir->m_objectId);  // ICB
    writer.writeLE16(0);                                                       // Length of Implementation Use
    writer.writeDString(subDir->m_name);
    writer.doPadding(4);
    writer.closeDescriptorTag();
}

void FileEntryInfo::serialize()
{
    m_owner->sectorSeek(IsoWriter::MetadataPartition, m_sectorNum);

    if (isFile())
        serializeFile();
    else
        serializeDir();
}

int FileEntryInfo::allocateEntity(int sectorNum)
{
    m_sectorNum = sectorNum;
    if (!isFile())
        m_sectorsUsed = 2;  // restriction here: amount of files/subdirs is limited by sector size.
    else if (m_extents.size() <= MAX_EXTENTS_IN_EXTFILE)
        m_sectorsUsed = 1;
    else
    {
        int sz = m_extents.size() - 1 - MAX_EXTENTS_IN_EXTFILE;
        m_sectorsUsed = 1 + (sz + MAX_EXTENTS_IN_EXTCONT - 1) / MAX_EXTENTS_IN_EXTCONT;
    }
    return m_sectorsUsed;
}

void FileEntryInfo::serializeFile()
{
    uint8_t dataSize = 0;
    uint8_t buffer[SECTOR_SIZE];

    memset(buffer, 0, sizeof(buffer));

    int writed = m_owner->writeExtentFileDescriptor(m_name == "*UDF Unique ID Mapping Data", m_fileType, m_fileSize,
                                                    m_sectorNum, 1, &m_extents);
    assert(writed == m_sectorsUsed);
}

void FileEntryInfo::serializeDir()
{
    uint8_t dataSize = 0;
    uint8_t buffer[SECTOR_SIZE];

    memset(buffer, 0, sizeof(buffer));
    ByteFileWriter writer;
    writer.setBuffer(buffer, sizeof(buffer));

    // ------------ 1 (parent entry) ---------------

    writer.writeDescriptorTag(DESC_TYPE_FileId, m_owner->absoluteSectorNum() + 1);
    writer.writeLE16(0x01);  // File Version Number
    writer.writeLE8(0x0A);   // File Characteristics, parent flag (3-th bit) and  'directory' bit (1-th)
    writer.writeLE8(0x00);   // Length of File Identifier (=L_FI)

    writer.writeLongAD(0x800, m_parent ? m_parent->m_sectorNum : m_owner->absoluteSectorNum(), 0x01,
                       0);  // parent entry ICB
    writer.writeLE16(0);    // Length of Implementation Use
    writer.writeLE16(0);    // zero d-string for parent FID
    writer.closeDescriptorTag();

    // ------------ 2 (entries) ---------------
    for (auto& i : m_files) writeEntity(writer, i);
    for (auto& i : m_subDirs) writeEntity(writer, i);
    assert(writer.size() < SECTOR_SIZE);  // not supported

    m_owner->writeExtentFileDescriptor(0, m_fileType, writer.size(), m_sectorNum + 1, m_subDirs.size() + 1);
    m_owner->writeSector(buffer);
}

void FileEntryInfo::addExtent(const Extent& extent)
{
    if (m_extents.empty())
    {
        m_extents.push_back(extent);
    }
    else
    {
        int lastLBN = m_extents.rbegin()->lbnPos + m_extents.rbegin()->size / SECTOR_SIZE;
        if (lastLBN == extent.lbnPos && m_extents.rbegin()->size + extent.size < MAX_EXTENT_SIZE)
            m_extents.rbegin()->size += extent.size;
        else
            m_extents.push_back(extent);
    }
    m_fileSize += extent.size;
}

int FileEntryInfo::write(const uint8_t* data, uint32_t len)
{
    if (m_owner->m_lastWritedObjectID != m_objectId)
    {
#if 0
        if (m_fileType == FileType_RealtimeFile) {
            if (m_subMode)
                m_owner->checkLayerBreakPoint((SUB_INTERLEAVE_BLOCKSIZE + MAIN_INTERLEAVE_BLOCKSIZE) / SECTOR_SIZE);
        }
#endif
        m_extents.push_back(Extent(m_owner->absoluteSectorNum(), len));
    }
    else
    {
        if (m_extents.rbegin()->size + len < MAX_EXTENT_SIZE)
            m_extents.rbegin()->size += len;
        else
        {
            assert(m_extents.rbegin()->size % SECTOR_SIZE == 0);
            m_extents.push_back(Extent(m_owner->absoluteSectorNum(), len));
        }
    }
    m_owner->m_lastWritedObjectID = m_objectId;
    m_fileSize += len;
    int writeLen = len;

    if (m_sectorBufferSize)
    {
        int toCopy = FFMIN(SECTOR_SIZE - m_sectorBufferSize, writeLen);
        memcpy(m_sectorBuffer + m_sectorBufferSize, data, toCopy);
        m_sectorBufferSize += toCopy;
        if (m_sectorBufferSize == SECTOR_SIZE)
        {
            m_owner->writeRawData(m_sectorBuffer, SECTOR_SIZE);
            m_sectorBufferSize = 0;
            data += toCopy;
            writeLen -= toCopy;
        }
        else
        {
            return len;
        }
    }
    int dataRest = writeLen % SECTOR_SIZE;
    if (writeLen - dataRest > 0)
        m_owner->writeRawData(data, writeLen - dataRest);
    if (dataRest)
    {
        memcpy(m_sectorBuffer, data + writeLen - dataRest, dataRest);
        m_sectorBufferSize += dataRest;
    }

    return len;
}

void FileEntryInfo::close()
{
    if (m_sectorBufferSize)
    {
        int delta = m_fileSize / SECTOR_SIZE;
        m_owner->sectorSeek(IsoWriter::MainPartition, m_extents.rbegin()->lbnPos + delta);
        memset(m_sectorBuffer + m_sectorBufferSize, 0, SECTOR_SIZE - m_sectorBufferSize);
        m_owner->writeRawData(m_sectorBuffer, SECTOR_SIZE);
        m_sectorBufferSize = 0;
    }
}

void FileEntryInfo::setSubMode(bool value) { m_subMode = value; }

FileEntryInfo* FileEntryInfo::subDirByName(const std::string& name) const
{
    for (auto& i : m_subDirs)
    {
        if (i->m_name == name)
            return i;
    }
    return 0;
}

FileEntryInfo* FileEntryInfo::fileByName(const std::string& name) const
{
    for (auto& i : m_files)
    {
        if (i->m_name == name)
            return i;
    }
    return 0;
}

// --------------------------------- ISOFile -----------------------------------

int ISOFile::write(const void* data, uint32_t len)
{
    if (m_entry)
        return m_entry->write((const uint8_t*)data, len);
    else
        return -1;
}

bool ISOFile::open(const char* name, unsigned int oflag, unsigned int systemDependentFlags)
{
    FileTypes fileType = FileType_File;
    if (strEndWith(name, ".m2ts") || strEndWith(name, ".ssif"))
        fileType = FileType_RealtimeFile;
    m_entry = m_owner->getEntryByName(toIsoSeparator(name), fileType);
    return true;
}

void ISOFile::sync() { m_owner->m_file.sync(); }

bool ISOFile::close()
{
    if (m_entry)
        m_entry->close();
    m_entry = 0;
    return true;
}

void ISOFile::setSubMode(bool value)
{
    if (m_entry)
        m_entry->setSubMode(value);
}

int64_t ISOFile::size() const { return m_entry ? m_entry->m_fileSize : -1; }

// ------------------------------ IsoWriter ----------------------------------

IsoWriter::IsoWriter(const IsoHeaderData& hdrData)
    : m_impId(hdrData.impId), m_appId(hdrData.appId), m_volumeId(hdrData.volumeId), m_currentTime(hdrData.fileTime)
{
    m_objectUniqId = 16;
    m_totalFiles = 0;
    m_totalDirectories = 0;
    m_volumeSize = 0;
    // m_sectorNum = 0;
    m_rootDirInfo = 0;
    m_systemStreamDir = 0;

    m_metadataFileLen = 0x30000;
    m_systemStreamLBN = 0;
    m_metadataMirrorLBN = 0;
    m_tagLocationBaseAddr = 0;
    m_lastWritedObjectID = -1;
    m_opened = false;
    m_volumeLabel = " ";
    m_layerBreakPoint = 0;
}

IsoWriter::~IsoWriter()
{
    close();
    delete m_rootDirInfo;
    delete m_systemStreamDir;
}

void IsoWriter::setMetaPartitionSize(int size) { m_metadataFileLen = roundUp(size, ALLOC_BLOCK_SIZE); }

void IsoWriter::setVolumeLabel(const std::string& value)
{
    m_volumeLabel = value;
    if (m_volumeLabel.empty())
        m_volumeLabel = " ";
}

bool IsoWriter::open(const std::string& fileName, int64_t diskSize, int extraISOBlocks)
{
    int systemFlags = 0;
    if (!m_file.open(fileName.c_str(), File::ofWrite, systemFlags))
        return false;

    if (diskSize > 0)
    {
        int blocks = 2 + extraISOBlocks + roundUp64(diskSize, META_BLOCK_PER_DATA) / META_BLOCK_PER_DATA;
        setMetaPartitionSize(ALLOC_BLOCK_SIZE * blocks);
    }

    // 1. write 32K empty space
    memset(m_buffer, 0, sizeof(m_buffer));
    for (int i = 0; i < 32768 / SECTOR_SIZE; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    // 2. write Beginning Extended Area Descriptor
    m_buffer[0] = 0;  // Structure Type
    memcpy(m_buffer + 1, "BEA01", 5);
    m_buffer[6] = 1;  // Structure Version
    m_file.write(m_buffer, SECTOR_SIZE);

    // 3. Volume recognition structures. NSR Descriptor
    m_buffer[0] = 0;  // Structure Type
    memcpy(m_buffer + 1, "NSR03", 5);
    m_buffer[6] = 1;  // Structure Version
    m_file.write(m_buffer, SECTOR_SIZE);

    // 4. Terminating Extended Area Descriptor
    m_buffer[0] = 0;  // Structure Type
    memcpy(m_buffer + 1, "TEA01", 5);
    m_buffer[6] = 1;  // Structure Version
    m_file.write(m_buffer, SECTOR_SIZE);

    // 576K align

    memset(m_buffer, 0, SECTOR_SIZE);
    while (m_file.size() < 1024 * 576) m_file.write(m_buffer, SECTOR_SIZE);

    m_partitionStartAddress = m_file.size() / SECTOR_SIZE;
    m_tagLocationBaseAddr = m_partitionStartAddress;
    m_partitionEndAddress = 0;

    // Align to 64K (640K total)
    memset(m_buffer, 0, sizeof(m_buffer));
    for (int i = 0; i < 64 / 2; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    // -------------- start main volume --------------------------
    m_metadataLBN = m_file.size() / SECTOR_SIZE;

    // create root
    m_rootDirInfo = new FileEntryInfo(this, 0, 0, FileType_Directory);

    // create system stream dir
    m_systemStreamDir = new FileEntryInfo(this, 0, 0, FileType_SystemStreamDirectory);
    m_metadataMappingFile = new FileEntryInfo(this, m_systemStreamDir, 0, FileType_File);
    m_metadataMappingFile->setName("*UDF Unique ID Mapping Data");
    m_systemStreamDir->addFile(m_metadataMappingFile);

    // reserve space for metadata area

    memset(m_buffer, 0, sizeof(m_buffer));
    int64_t requiredFileSizeLBN = METADATA_START_ADDR + m_metadataFileLen / SECTOR_SIZE;
    int64_t currentFileSizeLBN = m_file.size() / SECTOR_SIZE;
    for (int i = currentFileSizeLBN; i < requiredFileSizeLBN; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    // reserve space for metadata mapping file

    for (int i = 0; i < ALLOC_BLOCK_SIZE / SECTOR_SIZE; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    m_opened = true;
    return true;
}

// ----------- descriptors --------------

FileEntryInfo* IsoWriter::mkdir(const char* name, FileEntryInfo* parent)
{
    if (!m_rootDirInfo)
        return 0;
    if (parent == 0)
        parent = m_rootDirInfo;

    FileEntryInfo* dir = new FileEntryInfo(this, parent, m_objectUniqId++, FileType_Directory);
    dir->setName(name);
    parent->addSubDir(dir);
    // m_sectorNum += 2;
    // m_systemStreamLBN = m_sectorNum; // last allocated sector
    return dir;
}

bool IsoWriter::createDir(const std::string& dir)
{
    getEntryByName(toIsoSeparator(dir), FileType_Directory);
    return true;
}

ISOFile* IsoWriter::createFile() { return new ISOFile(this); }

bool IsoWriter::createInterleavedFile(const std::string& inFile1, const std::string& inFile2,
                                      const std::string& outFile)
{
    FileEntryInfo* inEntry1 = getEntryByName(toIsoSeparator(inFile1), FileType_RealtimeFile);
    if (!inEntry1)
        return false;

    FileEntryInfo* inEntry2 = getEntryByName(toIsoSeparator(inFile2), FileType_RealtimeFile);
    if (!inEntry2)
        return false;

    FileEntryInfo* outEntry = getEntryByName(toIsoSeparator(outFile), FileType_RealtimeFile);
    if (!outEntry)
        return false;

    assert(inEntry1->m_extents.size() == inEntry2->m_extents.size());
    for (size_t i = 0; i < inEntry1->m_extents.size(); ++i)
    {
        assert(inEntry1->m_extents[i].size % SECTOR_SIZE == 0);
        assert(inEntry2->m_extents[i].size % SECTOR_SIZE == 0);
        outEntry->addExtent(inEntry2->m_extents[i]);
        outEntry->addExtent(inEntry1->m_extents[i]);
        // outEntry->m_extents.push_back(inEntry2->m_extents[i]);
        // outEntry->m_extents.push_back(inEntry1->m_extents[i]);
    }
    // outEntry->m_fileSize = inEntry1->m_fileSize + inEntry2->m_fileSize;

    return true;
}

FileEntryInfo* IsoWriter::createFileEntry(FileEntryInfo* parent, FileTypes fileType)
{
    if (!m_rootDirInfo)
        return 0;
    if (parent == 0)
        parent = m_rootDirInfo;

    FileEntryInfo* file = new FileEntryInfo(this, parent, m_objectUniqId++, fileType);
    parent->addFile(file);
    return file;
}

FileEntryInfo* IsoWriter::getEntryByName(const std::string& name, FileTypes fileType)
{
    std::vector<std::string> parts = splitStr(name.c_str(), '/');
    FileEntryInfo* entry = m_rootDirInfo;
    if (!entry)
        return 0;
    bool isDir = fileType == FileType_Directory || fileType == FileType_SystemStreamDirectory;
    int idxMax = isDir ? parts.size() : parts.size() - 1;
    for (int i = 0; i < idxMax; ++i)
    {
        FileEntryInfo* nextEntry = entry->subDirByName(parts[i]);
        if (!nextEntry)
            nextEntry = mkdir(parts[i].c_str(), entry);
        entry = nextEntry;
    }
    if (isDir)
        return 0;

    std::string fileName = *parts.rbegin();
    if (fileName.empty())
        return entry;
    FileEntryInfo* fileEntry = entry->fileByName(fileName);
    if (!fileEntry)
    {
        fileEntry = createFileEntry(entry, fileType);
        if (!fileEntry)
            return 0;
        fileEntry->setName(fileName);
    }

    return fileEntry;
}

void IsoWriter::writeMetadata(int lbn)
{
    m_file.seek(int64_t(lbn) * SECTOR_SIZE);
    m_curMetadataPos = lbn;
    writeFileSetDescriptor();
    writeEntity(m_rootDirInfo);

    // write system stream file
    writeEntity(m_systemStreamDir);
}

void IsoWriter::allocateMetadata()
{
    int sectorNum = 1;  // reserve sector for file set descriptor
    m_systemStreamLBN = allocateEntity(m_rootDirInfo, sectorNum);
    allocateEntity(m_systemStreamDir, m_systemStreamLBN);

    if (m_systemStreamLBN + 3 > m_metadataFileLen / SECTOR_SIZE)
        throw std::runtime_error(
            "ISO error: Not enough space in metadata partition. It possible in a split mode if a lot of files. Please "
            "provide addition input parameter --extra-iso-space to increate metadata space. Default value for this "
            "parameter in split mode: 4");

    // write udf unique id mapping file
    m_file.seek((int64_t)METADATA_START_ADDR * SECTOR_SIZE + m_metadataFileLen);

    uint8_t* buffer = new uint8_t[ALLOC_BLOCK_SIZE];
    memset(buffer, 0, ALLOC_BLOCK_SIZE);
    ByteFileWriter writer;
    writer.setBuffer(buffer, ALLOC_BLOCK_SIZE);

    writer.writeCharSpecString(m_impId.c_str(), 32);
    writer.writeLE32(0);  // flags
    writer.writeLE32(m_mappingEntries.size());
    writer.writeLE32(0);  // reserved
    writer.writeLE32(0);  // reserved

    std::map<int, MappingEntry>::iterator itr;
    for (itr = m_mappingEntries.begin(); itr != m_mappingEntries.end(); ++itr)
    {
        writer.writeLE32(itr->first);  // unique ID
        writer.writeLE32(itr->second.parentLBN);
        writer.writeLE32(itr->second.LBN);
        writer.writeLE16(1);  // parent partition
        writer.writeLE16(1);  // object partition
    }

    m_metadataMappingFile->write(buffer, writer.size());
    m_metadataMappingFile->close();
    delete[] buffer;
}

void IsoWriter::close()
{
    if (!m_opened)
        return;

    memset(m_buffer, 0, sizeof(m_buffer));
    while (m_file.size() % ALLOC_BLOCK_SIZE != 62 * 1024) m_file.write(m_buffer, SECTOR_SIZE);

    // mirror metadata file location and length
    int64_t sz = m_file.size();
    m_metadataMirrorLBN = m_file.size() / SECTOR_SIZE + 1;
    m_tagLocationBaseAddr = m_partitionStartAddress;
    writeExtentFileDescriptor(0, FileType_MetadataMirror, m_metadataFileLen,
                              m_metadataMirrorLBN - m_partitionStartAddress, 0);

    // allocate space for metadata mirror file
    memset(m_buffer, 0, sizeof(m_buffer));
    for (int i = 0; i < m_metadataFileLen / SECTOR_SIZE; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    m_partitionEndAddress = m_file.size() / SECTOR_SIZE;

    // reserve 64K for EOF anchor volume
    memset(m_buffer, 0, sizeof(m_buffer));
    for (int i = 0; i < 32; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    allocateMetadata();

    m_tagLocationBaseAddr = m_metadataMirrorLBN;
    writeMetadata(m_metadataMirrorLBN);  // write metadata mirror file

    m_file.seek(1024 * 576);
    // metadata file location and length (located at 576K, point to 640K address)
    m_tagLocationBaseAddr = m_partitionStartAddress;  //(1024 * 576)/SECTOR_SIZE;
    writeExtentFileDescriptor(0, FileType_Metadata, m_metadataFileLen, m_metadataLBN - m_partitionStartAddress, 0);
    m_tagLocationBaseAddr = m_metadataLBN;  // I don't know why. I doing just as scenarist does
    writeMetadata(m_metadataLBN);

    writeDescriptors();
    m_opened = false;
}

void IsoWriter::writeDescriptors()
{
    m_tagLocationBaseAddr = 0;
    // descriptors in a beginning of a file
    m_file.seek(1024 * 64);

    writePrimaryVolumeDescriptor();
    writeImpUseDescriptor();
    writePartitionDescriptor();
    writeLogicalVolumeDescriptor();
    writeUnallocatedSpaceDescriptor();
    writeTerminationDescriptor();

    m_file.seek(1024 * 128);

    writeLogicalVolumeIntegrityDescriptor();
    writeTerminationDescriptor();

    m_file.seek(1024 * 512);

    writeAnchorVolumeDescriptor(m_partitionEndAddress +
                                ALLOC_BLOCK_SIZE / SECTOR_SIZE);  // add space for last 64K anchor volume

    // descriptors in a end of a file

    int64_t eofPos = m_partitionEndAddress * (int64_t)SECTOR_SIZE;
    m_file.seek(eofPos + ALLOC_BLOCK_SIZE - SECTOR_SIZE);
    writeAnchorVolumeDescriptor(m_partitionEndAddress + ALLOC_BLOCK_SIZE / SECTOR_SIZE);

    writePrimaryVolumeDescriptor();
    writeImpUseDescriptor();
    writePartitionDescriptor();
    writeLogicalVolumeDescriptor();
    writeUnallocatedSpaceDescriptor();
    writeTerminationDescriptor();

    memset(m_buffer, 0, sizeof(m_buffer));
    int64_t fullFileSize = eofPos + int64_t(1024 * 512) + ALLOC_BLOCK_SIZE;
    while (m_file.size() < fullFileSize - SECTOR_SIZE) m_file.write(m_buffer, SECTOR_SIZE);
    writeAnchorVolumeDescriptor(m_partitionEndAddress + ALLOC_BLOCK_SIZE / SECTOR_SIZE);
}

uint32_t IsoWriter::absoluteSectorNum() { return m_file.pos() / SECTOR_SIZE - m_tagLocationBaseAddr; }

void IsoWriter::sectorSeek(Partition partition, int pos)
{
    int64_t offset = (partition == MetadataPartition) ? m_curMetadataPos : m_partitionStartAddress;
    m_file.seek((offset + pos) * SECTOR_SIZE, File::smBegin);
}

void IsoWriter::writeEntity(FileEntryInfo* dir)
{
    dir->serialize();
    for (auto& i : dir->m_files) writeEntity(i);
    for (auto& i : dir->m_subDirs) writeEntity(i);
}

int IsoWriter::allocateEntity(FileEntryInfo* entity, int sectorNum)
{
    if ((entity->m_fileType == FileType_File || entity->m_fileType == FileType_RealtimeFile) && entity->m_objectId)
        m_totalFiles++;
    else if (entity->m_fileType == FileType_Directory)
        m_totalDirectories++;

    sectorNum += entity->allocateEntity(sectorNum);
    if (entity->m_objectId)
        m_mappingEntries[entity->m_objectId] = MappingEntry(entity->m_parent->m_sectorNum, entity->m_sectorNum);

    for (auto& i : entity->m_files) sectorNum = allocateEntity(i, sectorNum);
    for (auto& i : entity->m_subDirs) sectorNum = allocateEntity(i, sectorNum);
    return sectorNum;
}

void IsoWriter::writeAllocationExtentDescriptor(ExtentList* extents, size_t start, size_t indexEnd)
{
    uint32_t* buff32 = (uint32_t*)m_buffer;
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_AllocationExtent, absoluteSectorNum());

    uint8_t* curPos = m_buffer + 24;
    for (size_t i = start; i < indexEnd; ++i)
    {
        writeLongAD(curPos, extents->at(i).size, extents->at(i).lbnPos, 0, 0);
        curPos += 16;
    }
    if (indexEnd < extents->size())
    {
        buff32[212 / 4] += 0x10;
        writeLongAD(curPos, SECTOR_SIZE + NEXT_EXTENT, absoluteSectorNum() + 1, 1, 0);
        curPos += 16;
    }
    buff32[5] = curPos - m_buffer - 24;  // length
    calcDescriptorCRC(m_buffer, curPos - m_buffer);
    m_file.write(m_buffer, SECTOR_SIZE);
}

int IsoWriter::writeExtentFileDescriptor(bool namedStream, uint8_t fileType, uint64_t len, uint32_t pos, int linkCount,
                                         ExtentList* extents)
{
    int sectorsWrited = 0;

    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_ExtendedFile, absoluteSectorNum());

    uint32_t* buff32 = (uint32_t*)m_buffer;
    uint16_t* buff16 = (uint16_t*)m_buffer;
    uint64_t* buff64 = (uint64_t*)m_buffer;

    writeIcbTag(namedStream, m_buffer + 16, fileType);

    buff32[36 / 4] = 0xffffffff;  // uid
    buff32[40 / 4] = 0xffffffff;  // guid
    // zero Permissions, File Link Count, Record Format, Record Display Attributes
    if (fileType >= FileType_Metadata)
    {
        buff32[44 / 4] = 0x000000;  // Permissions
        assert(linkCount == 0);
        buff16[48 / 2] = linkCount;  // File Link Count
    }
    else if (fileType == FileType_File || fileType == FileType_RealtimeFile)
    {
        buff32[44 / 4] = 0x1084;  // Permissions
        assert(linkCount == 1);
        buff16[48 / 2] = linkCount;  // File Link Count
    }
    else
    {
        buff32[44 / 4] = 0x14A5;     // Permissions
        buff16[48 / 2] = linkCount;  // File Link Count
    }

    m_buffer[50] = 0x0;  // Record Format
    m_buffer[51] = 0x0;  // Record Display Attributes

    buff32[52 / 4] = 0x00;  // Record Length
    buff64[56 / 8] = len;   // Information Length
    buff64[64 / 8] = len;   // Object Size

    buff32[72 / 4] = roundUp64(len, SECTOR_SIZE) / SECTOR_SIZE;  // blocks recorded (matched to bytes)
    buff32[76 / 4] = 0x00;                                       // high part of blocks recorded

    writeTimestamp(m_buffer + 80, m_currentTime);   // access datetime
    writeTimestamp(m_buffer + 92, m_currentTime);   // modification datetime
    writeTimestamp(m_buffer + 104, m_currentTime);  // creation datetime
    writeTimestamp(m_buffer + 116, m_currentTime);  // attributes datetime
    buff32[128 / 4] = 0x01;                         // Checkpoint
    // skip Extended Attribute ICB
    // skip Stream Directory ICB

    strcpy((char*)m_buffer + 169, m_impId.c_str());  // Implementation Identifier

    // skip Length of Extended Attributes
    if (fileType != FileType_File && fileType != FileType_RealtimeFile)
    {
        // metadata object (metadata file, directory e.t.c). Using short extent
        buff32[212 / 4] = 0x08;  // Length of Allocation Descriptors
        buff32[216 / 4] = len;   // Allocation descriptors, data len in bytes
        buff32[220 / 4] = pos;   // Allocation descriptors, start logical block number inside volume
        calcDescriptorCRC(m_buffer, 224);
        m_file.write(m_buffer, SECTOR_SIZE);
        sectorsWrited++;
    }
    else if (extents == 0)
    {
        // file object. using long AD
        buff32[212 / 4] = 0x10;  // long AD size
        writeLongAD(m_buffer + 216, len, pos, 0, 0);
        calcDescriptorCRC(m_buffer, 232);
        m_file.write(m_buffer, SECTOR_SIZE);
        sectorsWrited++;
    }
    else
    {
        size_t indexEnd = FFMIN(MAX_EXTENTS_IN_EXTFILE, extents->size());
        if (extents->size() - indexEnd == 1)
            indexEnd++;  // continue record may be replaced by payload data
        buff32[212 / 4] = 0x10 * indexEnd;
        uint8_t* curPos = m_buffer + 216;
        for (size_t i = 0; i < indexEnd; ++i)
        {
            writeLongAD(curPos, extents->at(i).size, extents->at(i).lbnPos, 0, 0);
            curPos += 16;
        }
        if (indexEnd < extents->size())
        {
            buff32[212 / 4] += 0x10;
            writeLongAD(curPos, SECTOR_SIZE + NEXT_EXTENT, absoluteSectorNum() + 1, 1, 0);  // continue expected
            curPos += 16;
        }
        calcDescriptorCRC(m_buffer, curPos - m_buffer);
        m_file.write(m_buffer, SECTOR_SIZE);
        sectorsWrited++;

        size_t indexStart = indexEnd;
        while (indexStart < extents->size())
        {
            indexEnd = FFMIN(indexStart + MAX_EXTENTS_IN_EXTCONT, extents->size());
            if (extents->size() - indexEnd == 1)
                indexEnd++;  // continue record may be replaced by payload data
            writeAllocationExtentDescriptor(extents, indexStart, indexEnd);
            sectorsWrited++;
            indexStart = indexEnd;
        }
    }

    return sectorsWrited;
}

void IsoWriter::writeIcbTag(bool namedStream, uint8_t* buffer, uint8_t fileType)
{
    uint32_t* buff32 = (uint32_t*)buffer;
    uint16_t* buff16 = (uint16_t*)buffer;

    // icb tag
    buff32[0] = 0;  // Prior Recorded Number of Direct Entries
    buff16[2] = 4;  // Strategy Type
    buff16[3] = 0;  // Strategy parameters
    buff16[4] = 1;  // Maximum Number of Entries
    // skip reserved byte
    buffer[11] = fileType;  // metadata file type
    // skip 6 byte zero Parent ICB Location
    if (fileType == FileType_File || fileType == FileType_RealtimeFile)
        buff16[18 / 2] = 0x0021;  // flags: archive + long AD
    else
        buff16[18 / 2] = 0x0020;  // flags: archive
    if (namedStream)
        buff16[18 / 2] += 0x2000;  // flags: stream
}

void IsoWriter::writeFileSetDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));

    ByteFileWriter writer;
    writer.setBuffer(m_buffer, sizeof(m_buffer));
    writer.writeDescriptorTag(DESC_TYPE_FileSet, absoluteSectorNum());

    writer.writeTimestamp(m_currentTime);  // Volume Descriptor Sequence Number
    writer.writeLE16(0x03);                // Interchange Level
    writer.writeLE16(0x03);                // Maximum Interchange Level
    writer.writeLE32(0x01);                // Character Set List
    writer.writeLE32(0x01);                // Maximum Character Set List
    writer.writeLE32(0x00);                // File Set Number
    writer.writeLE32(0x00);                // File Set Descriptor Number
    writer.writeCharSpecString("OSTA Compressed Unicode", 64);
    writer.writeDString(m_volumeLabel, 128);                    // Logical Volume Identifier
    writer.writeCharSpecString("OSTA Compressed Unicode", 64);  // File Set Character Set
    writer.writeDString(m_volumeLabel, 32);                     // File Set Identifier
    writer.skipBytes(32);                                       // Copyright File Identifier
    writer.skipBytes(32);                                       // Abstract File Identifier
    writer.writeLongAD(0x0800, 1, 1, 0);                        // Root Directory ICB
    writer.writeUDFString("*OSTA UDF Compliant", 32);           // Domain Identifier
    writer.writeLongAD(0, 0, 0, 0);                             // Next Extent
    writer.writeLongAD(0x0800, m_systemStreamLBN, 1, 0);

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writePrimaryVolumeDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));

    ByteFileWriter writer;
    writer.setBuffer(m_buffer, sizeof(m_buffer));
    writer.writeDescriptorTag(DESC_TYPE_PrimVol, absoluteSectorNum());

    writer.writeLE32(0x01);  // Volume Descriptor Sequence Number
    writer.writeLE32(0x00);  // Primary Volume Descriptor Number
    writer.writeDString(m_volumeLabel, 32);

    writer.writeLE16(0x01);  // Volume Sequence Number
    writer.writeLE16(0x01);  // Maximum Volume Sequence Number
    writer.writeLE16(0x02);  // Interchange Level
    writer.writeLE16(0x02);  // Maximum Interchange Level
    writer.writeLE32(0x01);  // Character Set List
    writer.writeLE32(0x01);  // Maximum Character Set List

    std::string volId = strToUpperCase(int32ToHex(m_volumeId));
    volId = strPadLeft(volId, 8, '0');
    std::string volumeSetIdentifier = volId + std::string("        ") + m_volumeLabel;
    writer.writeDString(volumeSetIdentifier.c_str(), 128);

    // Descriptor Character Set
    m_buffer[200] = 0x00;  // CS0 coded character set
    strcpy((char*)m_buffer + 201, "OSTA Compressed Unicode");

    // Explanatory Character Set
    m_buffer[264] = 0x00;  // CS0 coded character set
    strcpy((char*)m_buffer + 265, "OSTA Compressed Unicode");

    // skip Volume Abstract
    // skip Volume Copyright Notice

    // Application Identifier
    m_buffer[344] = 0x00;  // CS0 coded character set
    strcpy((char*)m_buffer + 345, m_appId.c_str());

    writeTimestamp(m_buffer + 376, m_currentTime);  // timestamp

    // Implementation Identifier
    m_buffer[388] = 0x00;  // CS0 coded character set
    strcpy((char*)m_buffer + 389, m_impId.c_str());

    strcpy((char*)m_buffer + 420, m_appId.c_str());

    // skip Predecessor Volume Descriptor Sequence Location (BP 484) = 0L
    m_buffer[488] = 1;  // Flags (BP 488)

    // 490..511 Reserved

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeImpUseDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_ImplUseVol, absoluteSectorNum());

    uint32_t* buff32 = (uint32_t*)m_buffer;
    uint16_t* buff16 = (uint16_t*)m_buffer;
    buff32[4] = 0x02;  // Descriptor Sequence Number

    std::string impId = std::string("*UDF LV Info");
    m_buffer[20] = 0x00;
    writeUDFString(m_buffer + 20, impId.c_str(), 32);

    // Explanatory Character Set
    m_buffer[52] = 0x00;  // CS0 coded character set
    strcpy((char*)m_buffer + 53, "OSTA Compressed Unicode");

    // logical volume identifier
    writeDString(m_buffer + 116, m_volumeLabel.c_str(), 128);

    strcpy((char*)m_buffer + 0x161, m_impId.c_str());  // ImplementationID
    strcpy((char*)m_buffer + 0x180, m_appId.c_str());  // ImplementationUse

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writePartitionDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_Partition, absoluteSectorNum());

    uint32_t* buff32 = (uint32_t*)m_buffer;
    uint16_t* buff16 = (uint16_t*)m_buffer;
    buff32[4] = 0x03;                        // Descriptor Sequence Number
    buff16[10] = 0x01;                       // partition flags
    buff16[11] = 0x00;                       // Partition Number
    strcpy((char*)m_buffer + 25, "+NSR03");  // Partition Contents
    // skip Partition Header Descriptor (all zero)
    buff32[184 / 4] = 0x01;                                             // Access Type
    buff32[188 / 4] = m_partitionStartAddress;                          // Partition Starting Location (576K address )
    buff32[192 / 4] = m_partitionEndAddress - m_partitionStartAddress;  // Partition Length field

    strcpy((char*)m_buffer + 0xc5, m_impId.c_str());  // ImplementationID
    strcpy((char*)m_buffer + 0xe4, m_appId.c_str());  // ImplementationUse

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeLogicalVolumeDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_LogicalVol, absoluteSectorNum());
    uint32_t* buff32 = (uint32_t*)m_buffer;
    uint16_t* buff16 = (uint16_t*)m_buffer;

    buff32[4] = 0x04;                                         // Volume Descriptor Sequence Number
    strcpy((char*)m_buffer + 21, "OSTA Compressed Unicode");  // Descriptor Character Set
    // Logical Volume Identifier
    writeDString(m_buffer + 84, m_volumeLabel.c_str(), 128);
    buff32[212 / 4] = SECTOR_SIZE;                              // Logical Block Size
    writeUDFString(m_buffer + 216, "*OSTA UDF Compliant", 32);  // Domain Identifier

    // Logical Volume Contents Use
    m_buffer[249] = 0x08;
    m_buffer[256] = 0x01;

    m_buffer[264] = 0x46;  // partition Map Table Length ( in bytes)
    m_buffer[268] = 0x02;  // Number of Partition Maps.

    // Implementation Identifier
    strcpy((char*)m_buffer + 273, m_impId.c_str());
    strcpy((char*)m_buffer + 304, m_appId.c_str());

    // Integrity Sequence Extent
    buff32[432 / 4] = 0x8000;
    buff32[436 / 4] = 0x40;

    // Partition Maps
    m_buffer[440] = 0x01;    // Type 1 Partition Map
    m_buffer[441] = 0x06;    // map len
    buff16[442 / 2] = 0x01;  // Volume Sequence Number
    buff16[444 / 2] = 0x00;  // Partition Number

    m_buffer[446] = 0x02;  // Type 2 Partition Map
    m_buffer[447] = 64;    // map len
    writeUDFString(m_buffer + 450, "*UDF Metadata Partition", 32);

    buff16[482 / 2] = 0x01;  // Volume Sequence Number
    buff16[484 / 2] = 0x00;  // Partition Number
    buff16[486 / 2] = 0x00;  // Metadata File Location
    buff16[488 / 2] = 0x00;  // Metadata File Location
    // Metadata Mirror File Location, to    do: fill me. should be written here in future
    uint32_t mirrorHeaderLocation = (m_metadataMirrorLBN - 1 - m_partitionStartAddress);
    buff16[490 / 2] = mirrorHeaderLocation % 65536;
    buff16[492 / 2] = mirrorHeaderLocation / 65536;

    // Metadata Bitmap File Location
    buff16[494 / 2] = 0xffff;
    buff16[496 / 2] = 0xffff;

    buff16[498 / 2] = 0x20;  // Allocation Unit Size (blocks) (64K in bytes)
    buff16[500 / 2] = 0x00;  // Allocation Unit Size (blocks)
    buff16[502 / 2] = 0x20;  // Alignment Unit Size (blocks) (64K in bytes)
    m_buffer[504] = 0x01;    // Flags

    calcDescriptorCRC(m_buffer, 510);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeUnallocatedSpaceDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_UnallocSpace, absoluteSectorNum());

    uint32_t* buff32 = (uint32_t*)m_buffer;
    uint16_t* buff16 = (uint16_t*)m_buffer;

    buff32[4] = 0x05;  // sequence number

    calcDescriptorCRC(m_buffer, 24);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeTerminationDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_Terminating, absoluteSectorNum());

    uint32_t* buff32 = (uint32_t*)m_buffer;
    uint16_t* buff16 = (uint16_t*)m_buffer;

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeLogicalVolumeIntegrityDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_LogicalVolIntegrity, absoluteSectorNum());

    uint32_t* buff32 = (uint32_t*)m_buffer;
    uint16_t* buff16 = (uint16_t*)m_buffer;

    writeTimestamp(m_buffer + 16, m_currentTime);
    buff32[7] = 0x01;              // Integrity Type
    buff32[8] = buff32[9] = 0x00;  // Next Integrity Extent
    // Logical Volume Contents Use (32 bytes)
    buff32[10] = m_objectUniqId;
    buff32[11] = 0;  // uniq ID hi

    buff32[72 / 4] = 0x02;                                             // Number of Partitions
    buff32[76 / 4] = 46 + m_appId.size();                              // Length of Implementation Use
    buff32[80 / 4] = buff32[84 / 4] = 0;                               // Free Space Table
    buff32[88 / 4] = m_partitionEndAddress - m_partitionStartAddress;  // main partition size
    buff32[92 / 4] = m_metadataFileLen / SECTOR_SIZE;

    strcpy((char*)m_buffer + 97, m_impId.c_str());

    buff32[0x80 / 4] = m_totalFiles;
    buff32[0x84 / 4] = m_totalDirectories;
    buff16[0x88 / 2] = 0x0250;  // Minimum UDF Read Revision
    buff16[0x8a / 2] = 0x0250;  // Minimum UDF Write Revision
    buff16[0x8c / 2] = 0x0250;  // Maximum UDF Write Revision

    strcpy((char*)m_buffer + 142, m_appId.c_str());

    calcDescriptorCRC(m_buffer, 142 + m_appId.size());
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeAnchorVolumeDescriptor(uint32_t endPartitionAddr)
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DESC_TYPE_AnchorVolPtr, absoluteSectorNum());

    uint32_t* buff32 = (uint32_t*)m_buffer;
    buff32[4] = 0x8000;  // point of the main volume descriptor
    buff32[5] = 0x20;
    buff32[6] = 0x8000;
    buff32[7] = endPartitionAddr;  // set size to allocated sectors amount

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeSector(uint8_t* sectorData) { m_file.write(sectorData, SECTOR_SIZE); }

int IsoWriter::writeRawData(const uint8_t* data, int size) { return m_file.write(data, size); }

void IsoWriter::checkLayerBreakPoint(int maxExtentSize)
{
    int lbn = absoluteSectorNum();
    if (lbn < m_layerBreakPoint && lbn + maxExtentSize > m_layerBreakPoint)
    {
        int rest = m_layerBreakPoint - lbn;
        uint8_t* tmpBuffer = new uint8_t[rest * SECTOR_SIZE];
        memset(tmpBuffer, 0, rest * SECTOR_SIZE);
        m_file.write(tmpBuffer, rest * SECTOR_SIZE);
        delete[] tmpBuffer;
        m_lastWritedObjectID = -1;
    }
}

void IsoWriter::setLayerBreakPoint(int lbn) { m_layerBreakPoint = lbn; }

IsoHeaderData IsoHeaderData::normal()
{
    return IsoHeaderData{"*tsMuxeR " TSMUXER_VERSION, std::string("*tsMuxeR ") + int32ToHex(random32()), time(0),
                         random32()};
}

IsoHeaderData IsoHeaderData::reproducible() { return IsoHeaderData{"*tsMuxeR", "*tsMuxeR", 1, 1593630000}; }
