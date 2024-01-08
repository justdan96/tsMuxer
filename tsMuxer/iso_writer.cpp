// ReSharper disable CppExpressionWithoutSideEffects

#include "iso_writer.h"

#include <algorithm>
#include <cassert>
#include <climits>
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

unsigned short crc16(const unsigned char *pcBlock, unsigned short len)
{
    unsigned short crc = 0;

    while (len--) crc = static_cast<uint16_t>(crc << 8) ^ Crc16Table[(crc >> 8) ^ *pcBlock++];

    return crc;
}

void writeDescriptorTag(uint8_t *buffer, DescriptorTag tag, const uint32_t tagLocation)
{
    const auto buff16 = reinterpret_cast<uint16_t *>(buffer);
    const auto buff32 = reinterpret_cast<uint32_t *>(buffer);
    buff16[0] = static_cast<uint16_t>(tag);
    buff16[1] = 0x03;  // version
    // skip check sum and reserved byte here
    buff16[3] = 0x01;  // Tag Serial Number
    // skip Descriptor CRC
    // Descriptor CRC Length

    buff32[3] = tagLocation;  // tag location
}

std::string toIsoSeparator(const std::string &path)
{
    std::string result = path;
    for (auto &i : result)
    {
        if (i == '\\')
            i = '/';
    }
    return result;
}

void calcDescriptorCRC(uint8_t *buffer, const uint16_t len)
{
    const auto buff16 = reinterpret_cast<uint16_t *>(buffer);

    // calc crc
    buff16[4] = crc16(buffer + 16, len - 16);
    buff16[5] = len - 16;

    // calc tag checksum of bytes 0-3 and 5-15 of the tag
    uint8_t sum = 0;
    for (int i = 0; i < 4; ++i) sum += buffer[i];
    for (int i = 5; i < 16; ++i) sum += buffer[i];
    buffer[4] = sum;
}

void writeTimestamp(uint8_t *buffer, const time_t time)
{
    const auto buff16 = reinterpret_cast<uint16_t *>(buffer);

    const tm *parts = localtime(&time);

    const time_t lt = mktime(localtime(&time));
    const time_t gt = mktime(gmtime(&time));
    const auto timeZone = static_cast<int16_t>((lt - gt) / 60);

    buff16[0] = (1 << 12) + (timeZone & 0x0fff);
    buff16[1] = static_cast<int16_t>(parts->tm_year + 1900);
    buffer[4] = static_cast<uint8_t>(parts->tm_mon + 1);
    buffer[5] = static_cast<uint8_t>(parts->tm_mday);
    buffer[6] = static_cast<uint8_t>(parts->tm_hour);
    buffer[7] = static_cast<uint8_t>(parts->tm_min);
    buffer[8] = static_cast<uint8_t>(parts->tm_sec);
    buffer[9] = 0;  // ms parts
    buffer[10] = 0;
    buffer[11] = 0;
}

bool canUse8BitUnicode(const std::string &utf8Str)
{
    bool rv = true;
    convertUTF::IterateUTF8Chars(utf8Str, [&](auto c) {
        rv = (c < 0x100);
        return rv;
    });
    return rv;
}

std::vector<std::uint8_t> serializeDString(const std::string &str, const size_t fieldLen)
{
    if (str.empty())
    {
        return {};
    }
    std::vector<std::uint8_t> rv;
#ifdef _WIN32
    const auto str_u8 = reinterpret_cast<const std::uint8_t *>(str.c_str());
    std::string utf8Str = convertUTF::isLegalUTF8String(str_u8, static_cast<int>(str.length()))
                              ? str
                              : UtfConverter::toUtf8(str_u8, str.length(), UtfConverter::SourceFormat::sfANSI);
#else
    auto &utf8Str = str;
#endif
    using namespace convertUTF;
    const size_t maxHeaderAndContentLength = fieldLen - 1;
    rv.reserve(fieldLen);
    if (canUse8BitUnicode(utf8Str))
    {
        rv.push_back(8);
        IterateUTF8Chars(utf8Str, [&](const uint8_t c) {
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
            const auto spaceLeft = maxHeaderAndContentLength - rv.size();
            if ((spaceLeft < 2) || (low_surrogate && spaceLeft < 4))
            {
                return false;
            }
            rv.push_back(static_cast<uint8_t>(high_surrogate >> 8));
            rv.push_back(static_cast<uint8_t>(high_surrogate));
            if (low_surrogate)
            {
                rv.push_back(static_cast<uint8_t>(low_surrogate >> 8));
                rv.push_back(static_cast<uint8_t>(low_surrogate));
            }
            return true;
        });
    }
    const auto contentLength = static_cast<uint8_t>(rv.size());
    const auto paddingSize = maxHeaderAndContentLength - rv.size();
    std::fill_n(std::back_inserter(rv), paddingSize, 0);
    rv.push_back(contentLength);
    return rv;
}

void writeDString(uint8_t *buffer, const char *value, const size_t fieldLen)
{
    auto content = serializeDString(value, fieldLen);
    assert(content.size() == fieldLen);
    std::copy(std::begin(content), std::end(content), buffer);
}

void writeUDFString(uint8_t *buffer, const char *str, const int len)
{
    strcpy(reinterpret_cast<char *>(buffer) + 1, str);
    buffer[len - 8] = 0x50;  // UDF suffix
    buffer[len - 7] = 0x02;  // UDF suffix
}

void writeLongAD(uint8_t *buffer, const uint32_t lenBytes, const uint32_t pos, const uint16_t partition,
                 const uint32_t id)
{
    const auto buff32 = reinterpret_cast<uint32_t *>(buffer);
    const auto buff16 = reinterpret_cast<uint16_t *>(buffer);

    buff32[0] = lenBytes;   // len
    buff32[1] = pos;        // location, block number
    buff16[4] = partition;  // location, partition number
    buff32[3] = id;
}

}  // namespace

// --------------------- ByteFileWriter ---------------------

ByteFileWriter::ByteFileWriter() : m_buffer(nullptr), m_bufferEnd(nullptr), m_curPos(nullptr), m_tagPos(nullptr) {}

void ByteFileWriter::setBuffer(uint8_t *buffer, const int len)
{
    m_buffer = buffer;
    m_bufferEnd = buffer + len;
    m_curPos = buffer;
}

void ByteFileWriter::writeLE8(const uint8_t value) { *m_curPos++ = value; }

void ByteFileWriter::writeLE16(const uint16_t value)
{
    const auto pos16 = reinterpret_cast<uint16_t *>(m_curPos);
    *pos16 = value;
    m_curPos += 2;
}

void ByteFileWriter::writeLE32(const uint32_t value)
{
    const auto pos32 = reinterpret_cast<uint32_t *>(m_curPos);
    *pos32 = value;
    m_curPos += 4;
}

void ByteFileWriter::writeDescriptorTag(const DescriptorTag tag, const uint32_t tagLocation)
{
    ::writeDescriptorTag(m_curPos, tag, tagLocation);
    m_tagPos = m_curPos;
    m_curPos += 16;
}

void ByteFileWriter::closeDescriptorTag(const int dataSize) const
{
    const uint16_t size = dataSize == -1 ? static_cast<uint16_t>(m_curPos - m_tagPos) : static_cast<uint16_t>(dataSize);
    calcDescriptorCRC(m_tagPos, size);
}

void ByteFileWriter::writeIcbTag(const uint8_t fileType)
{
    const auto buff32 = reinterpret_cast<uint32_t *>(m_curPos);
    const auto buff16 = reinterpret_cast<uint16_t *>(m_curPos);

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

void ByteFileWriter::writeLongAD(const uint32_t lenBytes, const uint32_t pos, const uint16_t partition,
                                 const uint32_t id)
{
    ::writeLongAD(m_curPos, lenBytes, pos, partition, id);
    m_curPos += 16;
}

void ByteFileWriter::writeDString(const char *value, const int64_t len)
{
    const size_t writeLen = (len == -1) ? strlen(value) + 2 : len;
    ::writeDString(m_curPos, value, writeLen);
    m_curPos += writeLen;
}

void ByteFileWriter::writeDString(const std::string &value, const int64_t len) { writeDString(value.c_str(), len); }

void ByteFileWriter::doPadding(const int padSize)
{
    m_curPos--;
    auto rest = static_cast<int>((m_curPos - m_buffer) % padSize);
    if (rest)
    {
        rest = padSize - rest;
        for (int i = 0; i < rest; ++i) *m_curPos++ = 0;
    }
}

void ByteFileWriter::writeCharSpecString(const char *value, const int len)
{
    strcpy(reinterpret_cast<char *>(m_curPos) + 1, value);
    m_curPos += len;
}

void ByteFileWriter::writeUDFString(const char *value, const int len)
{
    ::writeUDFString(m_curPos, value, len);
    m_curPos += len;
}

void ByteFileWriter::skipBytes(const int value) { m_curPos += value; }

void ByteFileWriter::writeTimestamp(const time_t time)
{
    ::writeTimestamp(m_curPos, time);
    m_curPos += 12;
};

int64_t ByteFileWriter::size() const { return m_curPos - m_buffer; }

// ------------------------------ FileEntryInfo ------------------------------------

FileEntryInfo::FileEntryInfo(IsoWriter *owner, FileEntryInfo *parent, const uint8_t objectId, const FileTypes fileType)
    : m_owner(owner),
      m_parent(parent),
      m_sectorNum(0),
      m_sectorsUsed(0),
      m_objectId(objectId),
      m_fileType(fileType),
      m_fileSize(0),
      m_sectorBufferSize(0),
      m_subMode(false)
{
    if (isFile())
        m_sectorBuffer = new uint8_t[SECTOR_SIZE];
    else
        m_sectorBuffer = nullptr;
}

bool FileEntryInfo::isFile() const { return m_fileType == FileTypes::File || m_fileType == FileTypes::RealtimeFile; }

FileEntryInfo::~FileEntryInfo()
{
    delete[] m_sectorBuffer;
    for (const auto &m_file : m_files) delete m_file;
    for (const auto &m_subDir : m_subDirs) delete m_subDir;
}

bool FileEntryInfo::setName(const std::string &name)
{
    m_name = name;
    return true;
}

void FileEntryInfo::addSubDir(FileEntryInfo *dir) { m_subDirs.push_back(dir); }

void FileEntryInfo::addFile(FileEntryInfo *file) { m_files.push_back(file); }

void FileEntryInfo::writeEntity(ByteFileWriter &writer, const FileEntryInfo *subDir) const
{
    const bool isSystemFile = (m_objectId == 0);

    writer.writeDescriptorTag(DescriptorTag::FileId, m_owner->absoluteSectorNum() + 1);
    writer.writeLE16(0x01);  // File Version Number
    writer.writeLE8(!subDir->isFile() ? 0x02
                                      : (isSystemFile ? 0x10 : 0));      // File Characteristics, 'directory' bit (1-th)
    writer.writeLE8(static_cast<uint8_t>(subDir->m_name.length()) + 1);  // Length of File Identifier (=L_FI)
    writer.writeLongAD(0x800, subDir->m_sectorNum, 0x01, subDir->m_objectId);  // ICB
    writer.writeLE16(0);                                                       // Length of Implementation Use
    writer.writeDString(subDir->m_name);
    writer.doPadding(4);
    writer.closeDescriptorTag();
}

void FileEntryInfo::serialize() const
{
    m_owner->sectorSeek(IsoWriter::Partition::MetadataPartition, m_sectorNum);

    if (isFile())
        serializeFile();
    else
        serializeDir();
}

int FileEntryInfo::allocateEntity(const int sectorNum)
{
    m_sectorNum = sectorNum;
    if (!isFile())
        m_sectorsUsed = 2;  // restriction here: amount of files/subdirs is limited by sector size.
    else if (m_extents.size() <= MAX_EXTENTS_IN_EXTFILE)
        m_sectorsUsed = 1;
    else
    {
        const int sz = static_cast<int>(m_extents.size() - 1 - MAX_EXTENTS_IN_EXTFILE);
        m_sectorsUsed = 1 + (sz + MAX_EXTENTS_IN_EXTCONT - 1) / MAX_EXTENTS_IN_EXTCONT;
    }
    return m_sectorsUsed;
}

void FileEntryInfo::serializeFile() const
{
    const int writed = m_owner->writeExtendedFileEntryDescriptor(m_name == "*UDF Unique ID Mapping Data", m_objectId,
                                                                 m_fileType, m_fileSize, m_sectorNum, 1, &m_extents);
    assert(writed == m_sectorsUsed);
}

void FileEntryInfo::serializeDir() const
{
    uint8_t buffer[SECTOR_SIZE] = {};

    ByteFileWriter writer;
    writer.setBuffer(buffer, sizeof(buffer));

    // ------------ 1 (parent entry) ---------------

    writer.writeDescriptorTag(DescriptorTag::FileId, m_owner->absoluteSectorNum() + 1);
    writer.writeLE16(0x01);  // File Version Number
    writer.writeLE8(0x0A);   // File Characteristics, parent flag (3-th bit) and  'directory' bit (1-th)
    writer.writeLE8(0x00);   // Length of File Identifier (=L_FI)

    const uint8_t parentId = m_parent ? m_parent->m_objectId : 0;
    writer.writeLongAD(0x800, m_parent ? m_parent->m_sectorNum : m_owner->absoluteSectorNum(), 0x01,
                       parentId);  // parent entry ICB
    writer.writeLE16(0);           // Length of Implementation Use
    writer.writeLE16(0);           // zero d-string for parent FID
    writer.closeDescriptorTag();

    // ------------ 2 (entries) ---------------
    for (const auto &i : m_files) writeEntity(writer, i);
    for (const auto &i : m_subDirs) writeEntity(writer, i);
    assert(writer.size() < SECTOR_SIZE);  // not supported

    m_owner->writeExtendedFileEntryDescriptor(false, m_objectId, m_fileType, writer.size(), m_sectorNum + 1,
                                              static_cast<uint16_t>(m_subDirs.size() + 1));
    m_owner->writeSector(buffer);
}

void FileEntryInfo::addExtent(const Extent &extent)
{
    if (m_extents.empty())
    {
        m_extents.push_back(extent);
    }
    else
    {
        const int lastLBN = m_extents.rbegin()->lbnPos + static_cast<int>(m_extents.rbegin()->size / SECTOR_SIZE);
        if (lastLBN == extent.lbnPos && m_extents.rbegin()->size + extent.size < MAX_EXTENT_SIZE)
            m_extents.rbegin()->size += extent.size;
        else
            m_extents.push_back(extent);
    }
    m_fileSize += extent.size;
}

int32_t FileEntryInfo::write(const uint8_t *data, const int32_t len)
{
    if (m_owner->m_lastWritedObjectID != m_objectId)
    {
        m_extents.emplace_back(m_owner->absoluteSectorNum(), len);
    }
    else
    {
        if (m_extents.rbegin()->size + len < MAX_EXTENT_SIZE)
            m_extents.rbegin()->size += len;
        else
        {
            assert(m_extents.rbegin()->size % SECTOR_SIZE == 0);
            m_extents.emplace_back(m_owner->absoluteSectorNum(), len);
        }
    }
    m_owner->m_lastWritedObjectID = m_objectId;
    m_fileSize += len;
    int32_t writeLen = len;

    if (m_sectorBufferSize)
    {
        const int toCopy = FFMIN(SECTOR_SIZE - m_sectorBufferSize, writeLen);
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
    const int dataRest = writeLen % SECTOR_SIZE;
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
        const auto delta = static_cast<int>(m_fileSize / SECTOR_SIZE);
        m_owner->sectorSeek(IsoWriter::Partition::MainPartition, m_extents.rbegin()->lbnPos + delta);
        memset(m_sectorBuffer + m_sectorBufferSize, 0, SECTOR_SIZE - m_sectorBufferSize);
        m_owner->writeRawData(m_sectorBuffer, SECTOR_SIZE);
        m_sectorBufferSize = 0;
    }
}

void FileEntryInfo::setSubMode(const bool value) { m_subMode = value; }

FileEntryInfo *FileEntryInfo::subDirByName(const std::string &name) const
{
    for (auto &i : m_subDirs)
    {
        if (i->m_name == name)
            return i;
    }
    return nullptr;
}

FileEntryInfo *FileEntryInfo::fileByName(const std::string &name) const
{
    for (auto &i : m_files)
    {
        if (i->m_name == name)
            return i;
    }
    return nullptr;
}

// --------------------------------- ISOFile -----------------------------------

int ISOFile::write(const void *data, const uint32_t len)
{
    if (m_entry)
        return m_entry->write(static_cast<const uint8_t *>(data), static_cast<int32_t>(len));
    return -1;
}

bool ISOFile::open(const char *name, unsigned int oflag, unsigned int systemDependentFlags)
{
    FileTypes fileType = FileTypes::File;
    if (strEndWith(name, ".m2ts") || strEndWith(name, ".ssif"))
        fileType = FileTypes::RealtimeFile;
    m_entry = m_owner->getEntryByName(toIsoSeparator(name), fileType);
    return true;
}

void ISOFile::sync() { m_owner->m_file.sync(); }

bool ISOFile::close()
{
    if (m_entry)
        m_entry->close();
    m_entry = nullptr;
    return true;
}

void ISOFile::setSubMode(const bool value) const
{
    if (m_entry)
        m_entry->setSubMode(value);
}

int64_t ISOFile::size() const { return m_entry ? m_entry->m_fileSize : -1; }

// ------------------------------ IsoWriter ----------------------------------

IsoWriter::IsoWriter(const IsoHeaderData &hdrData)
    : m_impId(hdrData.impId),
      m_appId(hdrData.appId),
      m_volumeId(hdrData.volumeId),
      m_buffer{},
      m_currentTime(hdrData.fileTime),
      m_metadataMappingFile(nullptr),
      m_partitionStartAddress(0),
      m_partitionEndAddress(0),
      m_metadataLBN(0),
      m_curMetadataPos(0)
{
    m_objectUniqId = 16;
    m_totalFiles = 0;
    m_totalDirectories = 0;
    m_volumeSize = 0;
    // m_sectorNum = 0;
    m_rootDirInfo = nullptr;
    m_systemStreamDir = nullptr;

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

void IsoWriter::setMetaPartitionSize(const int size) { m_metadataFileLen = roundUp(size, ALLOC_BLOCK_SIZE); }

void IsoWriter::setVolumeLabel(const std::string &value)
{
    m_volumeLabel = value;
    if (m_volumeLabel.empty())
        m_volumeLabel = "Blu-Ray";
}

bool IsoWriter::open(const std::string &fileName, const int64_t diskSize, const int extraISOBlocks)
{
    constexpr int systemFlags = 0;
    if (!m_file.open(fileName.c_str(), File::ofWrite, systemFlags))
        return false;

    if (diskSize > 0)
    {
        const int blocks =
            2 + extraISOBlocks + static_cast<int>(roundUp64(diskSize, META_BLOCK_PER_DATA) / META_BLOCK_PER_DATA);
        setMetaPartitionSize(ALLOC_BLOCK_SIZE * blocks);
    }

    // 1. write 32K empty space
    memset(m_buffer, 0, sizeof(m_buffer));
    for (int i = 0; i < 32768 / SECTOR_SIZE; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    // 2. write Beginning Extended Area Descriptor
    m_buffer[0] = 0;  // Structure Type
    m_buffer[1] = 'B';
    m_buffer[2] = 'E';
    m_buffer[3] = 'A';
    m_buffer[4] = '0';
    m_buffer[5] = '1';
    m_buffer[6] = 1;  // Structure Version
    m_file.write(m_buffer, SECTOR_SIZE);

    // 3. Volume recognition structures. NSR Descriptor
    m_buffer[0] = 0;  // Structure Type
    m_buffer[1] = 'N';
    m_buffer[2] = 'S';
    m_buffer[3] = 'R';
    m_buffer[4] = '0';
    m_buffer[5] = '3';
    m_buffer[6] = 1;  // Structure Version
    m_file.write(m_buffer, SECTOR_SIZE);

    // 4. Terminating Extended Area Descriptor
    m_buffer[0] = 0;  // Structure Type
    m_buffer[1] = 'T';
    m_buffer[2] = 'E';
    m_buffer[3] = 'A';
    m_buffer[4] = '0';
    m_buffer[5] = '1';
    m_buffer[6] = 1;  // Structure Version
    m_file.write(m_buffer, SECTOR_SIZE);

    // 576K align

    memset(m_buffer, 0, SECTOR_SIZE);
    while (m_file.size() < 1024LL * 576) m_file.write(m_buffer, SECTOR_SIZE);

    m_partitionStartAddress = static_cast<int>(m_file.size() / SECTOR_SIZE);
    m_tagLocationBaseAddr = m_partitionStartAddress;
    m_partitionEndAddress = 0;

    // Align to 64K (640K total)
    memset(m_buffer, 0, sizeof(m_buffer));
    for (int i = 0; i < 64 / 2; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    // -------------- start main volume --------------------------
    m_metadataLBN = static_cast<int>(m_file.size() / SECTOR_SIZE);

    // create root
    m_rootDirInfo = new FileEntryInfo(this, nullptr, 0, FileTypes::Directory);

    // create system stream dir
    m_systemStreamDir = new FileEntryInfo(this, nullptr, 0, FileTypes::SystemStreamDirectory);
    m_metadataMappingFile = new FileEntryInfo(this, m_systemStreamDir, 0, FileTypes::File);
    m_metadataMappingFile->setName("*UDF Unique ID Mapping Data");
    m_systemStreamDir->addFile(m_metadataMappingFile);

    // reserve space for metadata area

    memset(m_buffer, 0, sizeof(m_buffer));
    const int64_t requiredFileSizeLBN = METADATA_START_ADDR + m_metadataFileLen / SECTOR_SIZE;
    const int64_t currentFileSizeLBN = m_file.size() / SECTOR_SIZE;
    for (int64_t i = currentFileSizeLBN; i < requiredFileSizeLBN; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    // reserve space for metadata mapping file

    for (int i = 0; i < ALLOC_BLOCK_SIZE / SECTOR_SIZE; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    m_opened = true;
    return true;
}

// ----------- descriptors --------------

FileEntryInfo *IsoWriter::mkdir(const char *name, FileEntryInfo *parent)
{
    if (!m_rootDirInfo)
        return nullptr;
    if (parent == nullptr)
        parent = m_rootDirInfo;

    const auto dir = new FileEntryInfo(this, parent, m_objectUniqId++, FileTypes::Directory);
    dir->setName(name);
    parent->addSubDir(dir);
    // m_sectorNum += 2;
    // m_systemStreamLBN = m_sectorNum; // last allocated sector
    return dir;
}

bool IsoWriter::createDir(const std::string &dir)
{
    getEntryByName(toIsoSeparator(dir), FileTypes::Directory);
    return true;
}

ISOFile *IsoWriter::createFile() { return new ISOFile(this); }

bool IsoWriter::createInterleavedFile(const std::string &inFile1, const std::string &inFile2,
                                      const std::string &outFile)
{
    const FileEntryInfo *inEntry1 = getEntryByName(toIsoSeparator(inFile1), FileTypes::RealtimeFile);
    if (!inEntry1)
        return false;

    const FileEntryInfo *inEntry2 = getEntryByName(toIsoSeparator(inFile2), FileTypes::RealtimeFile);
    if (!inEntry2)
        return false;

    FileEntryInfo *outEntry = getEntryByName(toIsoSeparator(outFile), FileTypes::RealtimeFile);
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

FileEntryInfo *IsoWriter::createFileEntry(FileEntryInfo *parent, const FileTypes fileType)
{
    if (!m_rootDirInfo)
        return nullptr;
    if (parent == nullptr)
        parent = m_rootDirInfo;

    const auto file = new FileEntryInfo(this, parent, m_objectUniqId++, fileType);
    parent->addFile(file);
    return file;
}

FileEntryInfo *IsoWriter::getEntryByName(const std::string &name, const FileTypes fileType)
{
    const std::vector<std::string> parts = splitStr(name.c_str(), '/');
    FileEntryInfo *entry = m_rootDirInfo;
    if (!entry)
        return nullptr;
    const bool isDir = fileType == FileTypes::Directory || fileType == FileTypes::SystemStreamDirectory;
    const size_t idxMax = isDir ? parts.size() : parts.size() - 1;
    for (size_t i = 0; i < idxMax; ++i)
    {
        FileEntryInfo *nextEntry = entry->subDirByName(parts[i]);
        if (!nextEntry)
            nextEntry = mkdir(parts[i].c_str(), entry);
        entry = nextEntry;
    }
    if (isDir)
        return nullptr;

    const std::string fileName = *parts.rbegin();
    if (fileName.empty())
        return entry;
    FileEntryInfo *fileEntry = entry->fileByName(fileName);
    if (!fileEntry)
    {
        fileEntry = createFileEntry(entry, fileType);
        if (!fileEntry)
            return nullptr;
        fileEntry->setName(fileName);
    }

    return fileEntry;
}

void IsoWriter::writeMetadata(const int lbn)
{
    m_file.seek(static_cast<int64_t>(lbn) * SECTOR_SIZE);
    m_curMetadataPos = lbn;
    writeFileSetDescriptor();
    writeTerminationDescriptor();
    writeEntity(m_rootDirInfo);

    // write system stream file
    writeEntity(m_systemStreamDir);
}

void IsoWriter::allocateMetadata()
{
    constexpr int sectorNum = 2;  // reserve sector for file set descriptor and terminating descriptor
    m_systemStreamLBN = allocateEntity(m_rootDirInfo, sectorNum);
    allocateEntity(m_systemStreamDir, m_systemStreamLBN);

    if (m_systemStreamLBN + 3 > m_metadataFileLen / SECTOR_SIZE)
        throw std::runtime_error(
            "ISO error: Not enough space in metadata partition. It possible in a split mode if a lot of files. Please "
            "provide addition input parameter --extra-iso-space to increate metadata space. Default value for this "
            "parameter in split mode: 4");

    // write udf unique id mapping file
    m_file.seek(static_cast<int64_t>(METADATA_START_ADDR) * SECTOR_SIZE + m_metadataFileLen);

    const auto buffer = new uint8_t[ALLOC_BLOCK_SIZE];
    memset(buffer, 0, ALLOC_BLOCK_SIZE);
    ByteFileWriter writer;
    writer.setBuffer(buffer, ALLOC_BLOCK_SIZE);

    writer.writeCharSpecString(m_impId.c_str(), 32);
    writer.writeLE32(0);  // flags
    writer.writeLE32(static_cast<uint16_t>(m_mappingEntries.size()));
    writer.writeLE32(0);  // reserved
    writer.writeLE32(0);  // reserved

    for (const auto &mappingEntry : m_mappingEntries)
    {
        writer.writeLE32(mappingEntry.first);  // unique ID
        writer.writeLE32(mappingEntry.second.parentLBN);
        writer.writeLE32(mappingEntry.second.LBN);
        writer.writeLE16(1);  // parent partition
        writer.writeLE16(1);  // object partition
    }

    m_metadataMappingFile->write(buffer, static_cast<int32_t>(writer.size()));
    m_metadataMappingFile->close();
    delete[] buffer;
}

void IsoWriter::close()
{
    if (!m_opened)
        return;

    memset(m_buffer, 0, sizeof(m_buffer));
    while (m_file.size() % ALLOC_BLOCK_SIZE != 1024LL * 62) m_file.write(m_buffer, SECTOR_SIZE);

    // mirror metadata file location and length
    m_metadataMirrorLBN = static_cast<int>(m_file.size() / SECTOR_SIZE + 1);
    m_tagLocationBaseAddr = m_partitionStartAddress;
    writeExtendedFileEntryDescriptor(false, 0, FileTypes::MetadataMirror, m_metadataFileLen,
                                     m_metadataMirrorLBN - m_partitionStartAddress, 0);

    // allocate space for metadata mirror file
    memset(m_buffer, 0, sizeof(m_buffer));
    for (int i = 0; i < m_metadataFileLen / SECTOR_SIZE; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    m_partitionEndAddress = static_cast<int>(m_file.size() / SECTOR_SIZE);

    // reserve 64K for EOF anchor volume
    memset(m_buffer, 0, sizeof(m_buffer));
    for (int i = 0; i < 32; ++i) m_file.write(m_buffer, SECTOR_SIZE);

    allocateMetadata();

    m_tagLocationBaseAddr = m_metadataMirrorLBN;
    writeMetadata(m_metadataMirrorLBN);  // write metadata mirror file

    m_file.seek(1024LL * 576);
    // metadata file location and length (located at 576K, point to 640K address)
    m_tagLocationBaseAddr = m_partitionStartAddress;
    writeExtendedFileEntryDescriptor(false, 0, FileTypes::Metadata, m_metadataFileLen,
                                     m_metadataLBN - m_partitionStartAddress, 0);
    m_tagLocationBaseAddr = m_metadataLBN;  // Don't know why. Doing just as scenarist does
    writeMetadata(m_metadataLBN);

    writeDescriptors();
    m_opened = false;
}

void IsoWriter::writeDescriptors()
{
    m_tagLocationBaseAddr = 0;
    // descriptors in a beginning of a file
    m_file.seek(1024LL * 64);

    writePrimaryVolumeDescriptor();
    writeImpUseDescriptor();
    writePartitionDescriptor();
    writeLogicalVolumeDescriptor();
    writeUnallocatedSpaceDescriptor();
    writeTerminationDescriptor();

    m_file.seek(1024LL * 128);

    writeLogicalVolumeIntegrityDescriptor();
    writeTerminationDescriptor();

    m_file.seek(1024LL * 512);

    writeAnchorVolumeDescriptor(m_partitionEndAddress +
                                ALLOC_BLOCK_SIZE / SECTOR_SIZE);  // add space for last 64K anchor volume

    // descriptors in a end of a file

    const int64_t eofPos = m_partitionEndAddress * static_cast<int64_t>(SECTOR_SIZE);
    m_file.seek(eofPos + ALLOC_BLOCK_SIZE - SECTOR_SIZE);
    // TODO: It may be preferable not to include the AVDP at (N - 256) for Rewritable media (ditto DVDFab and ImgBurn)
    writeAnchorVolumeDescriptor(m_partitionEndAddress + ALLOC_BLOCK_SIZE / SECTOR_SIZE);

    writePrimaryVolumeDescriptor();
    writeImpUseDescriptor();
    writePartitionDescriptor();
    writeLogicalVolumeDescriptor();
    writeUnallocatedSpaceDescriptor();
    writeTerminationDescriptor();

    memset(m_buffer, 0, sizeof(m_buffer));
    const int64_t fullFileSize = eofPos + static_cast<int64_t>(1024 * 512) + ALLOC_BLOCK_SIZE;
    while (m_file.size() < fullFileSize - SECTOR_SIZE) m_file.write(m_buffer, SECTOR_SIZE);
    writeAnchorVolumeDescriptor(m_partitionEndAddress + ALLOC_BLOCK_SIZE / SECTOR_SIZE);
}

int32_t IsoWriter::absoluteSectorNum() const
{
    return static_cast<int32_t>(m_file.pos() / SECTOR_SIZE - m_tagLocationBaseAddr);
}

void IsoWriter::sectorSeek(const Partition partition, const int pos) const
{
    const int64_t offset = (partition == Partition::MetadataPartition) ? m_curMetadataPos : m_partitionStartAddress;
    m_file.seek((offset + pos) * SECTOR_SIZE, File::SeekMethod::smBegin);
}

void IsoWriter::writeEntity(const FileEntryInfo *dir)
{
    dir->serialize();
    for (const auto &i : dir->m_files) writeEntity(i);
    for (const auto &i : dir->m_subDirs) writeEntity(i);
}

int IsoWriter::allocateEntity(FileEntryInfo *entity, int sectorNum)
{
    if ((entity->m_fileType == FileTypes::File || entity->m_fileType == FileTypes::RealtimeFile) && entity->m_objectId)
        m_totalFiles++;
    else if (entity->m_fileType == FileTypes::Directory)
        m_totalDirectories++;

    sectorNum += entity->allocateEntity(sectorNum);
    if (entity->m_objectId)
        m_mappingEntries[entity->m_objectId] = MappingEntry(entity->m_parent->m_sectorNum, entity->m_sectorNum);

    for (const auto &i : entity->m_files) sectorNum = allocateEntity(i, sectorNum);
    for (const auto &i : entity->m_subDirs) sectorNum = allocateEntity(i, sectorNum);
    return sectorNum;
}

void IsoWriter::writeAllocationExtentDescriptor(const ExtentList *extents, const size_t start, const size_t indexEnd)
{
    const auto buff32 = reinterpret_cast<uint32_t *>(m_buffer);
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DescriptorTag::AllocationExtent, absoluteSectorNum());

    uint8_t *curPos = m_buffer + 24;
    for (size_t i = start; i < indexEnd; ++i)
    {
        writeLongAD(curPos, static_cast<uint32_t>(extents->at(i).size), extents->at(i).lbnPos, 0, 0);
        curPos += 16;
    }
    if (indexEnd < extents->size())
    {
        buff32[212 / 4] += 0x10;
        writeLongAD(curPos, SECTOR_SIZE + NEXT_EXTENT, absoluteSectorNum() + 1, 1, 0);
        curPos += 16;
    }
    buff32[5] = static_cast<uint32_t>(curPos - m_buffer - 24);  // length
    calcDescriptorCRC(m_buffer, static_cast<uint16_t>(curPos - m_buffer));
    m_file.write(m_buffer, SECTOR_SIZE);
}

int IsoWriter::writeExtendedFileEntryDescriptor(const bool namedStream, const uint8_t objectId,
                                                const FileTypes fileType, const uint64_t len, const uint32_t pos,
                                                const uint16_t linkCount, const ExtentList *extents)
{
    int sectorsWrited = 0;

    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DescriptorTag::ExtendedFileEntry, absoluteSectorNum());

    const auto buff32 = reinterpret_cast<uint32_t *>(m_buffer);
    const auto buff16 = reinterpret_cast<uint16_t *>(m_buffer);
    const auto buff64 = reinterpret_cast<uint64_t *>(m_buffer);

    writeIcbTag(namedStream, m_buffer + 16, fileType);

    buff32[36 / 4] = UINT_MAX;  // uid
    buff32[40 / 4] = UINT_MAX;  // guid
    // zero Permissions, File Link Count, Record Format, Record Display Attributes
    if (fileType >= FileTypes::Metadata)
    {
        buff32[44 / 4] = 0x000000;  // Permissions
        assert(linkCount == 0);
        buff16[48 / 2] = linkCount;  // File Link Count
    }
    else if (fileType == FileTypes::File || fileType == FileTypes::RealtimeFile)
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

    buff32[72 / 4] =
        static_cast<uint32_t>(roundUp64(len, SECTOR_SIZE) / SECTOR_SIZE);  // blocks recorded (matched to bytes)
    buff32[76 / 4] = 0x00;                                                 // high part of blocks recorded

    writeTimestamp(m_buffer + 80, m_currentTime);   // access datetime
    writeTimestamp(m_buffer + 92, m_currentTime);   // modification datetime
    writeTimestamp(m_buffer + 104, m_currentTime);  // creation datetime
    writeTimestamp(m_buffer + 116, m_currentTime);  // attributes datetime
    buff32[128 / 4] = 0x01;                         // Checkpoint
    // skip Extended Attribute ICB
    // skip Stream Directory ICB

    strcpy(reinterpret_cast<char *>(m_buffer) + 169, m_impId.c_str());  // Implementation Identifier
    m_buffer[200] = objectId;                                           // Unique ID

    // skip Length of Extended Attributes
    if (fileType != FileTypes::File && fileType != FileTypes::RealtimeFile)
    {
        // metadata object (metadata file, directory e.t.c). Using short extent
        buff32[212 / 4] = 0x08;                        // Length of Allocation Descriptors
        buff32[216 / 4] = static_cast<uint32_t>(len);  // Allocation descriptors, data len in bytes
        buff32[220 / 4] = pos;  // Allocation descriptors, start logical block number inside volume
        calcDescriptorCRC(m_buffer, 224);
        m_file.write(m_buffer, SECTOR_SIZE);
        sectorsWrited++;
    }
    else if (extents == nullptr)
    {
        // file object. using long AD
        buff32[212 / 4] = 0x10;  // long AD size
        writeLongAD(m_buffer + 216, static_cast<uint32_t>(len), pos, 0, 0);
        calcDescriptorCRC(m_buffer, 232);
        m_file.write(m_buffer, SECTOR_SIZE);
        sectorsWrited++;
    }
    else
    {
        size_t indexEnd = FFMIN(MAX_EXTENTS_IN_EXTFILE, extents->size());
        if (extents->size() - indexEnd == 1)
            indexEnd++;  // continue record may be replaced by payload data
        buff32[212 / 4] = static_cast<uint32_t>(0x10 * indexEnd);
        uint8_t *curPos = m_buffer + 216;
        for (size_t i = 0; i < indexEnd; ++i)
        {
            writeLongAD(curPos, static_cast<uint32_t>(extents->at(i).size), extents->at(i).lbnPos, 0, 0);
            curPos += 16;
        }
        if (indexEnd < extents->size())
        {
            buff32[212 / 4] += 0x10;
            writeLongAD(curPos, SECTOR_SIZE + NEXT_EXTENT, absoluteSectorNum() + 1, 1, 0);  // continue expected
            curPos += 16;
        }
        calcDescriptorCRC(m_buffer, static_cast<uint16_t>(curPos - m_buffer));
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

void IsoWriter::writeIcbTag(const bool namedStream, uint8_t *buffer, FileTypes fileType)
{
    const auto buff32 = reinterpret_cast<uint32_t *>(buffer);
    const auto buff16 = reinterpret_cast<uint16_t *>(buffer);

    // icb tag
    buff32[0] = 0;  // Prior Recorded Number of Direct Entries
    buff16[2] = 4;  // Strategy Type
    buff16[3] = 0;  // Strategy parameters
    buff16[4] = 1;  // Maximum Number of Entries
    // skip reserved byte
    buffer[11] = static_cast<uint8_t>(fileType);  // metadata file type
    // skip 6 byte zero Parent ICB Location
    if (fileType == FileTypes::File || fileType == FileTypes::RealtimeFile)
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
    writer.writeDescriptorTag(DescriptorTag::FileSet, absoluteSectorNum());

    writer.writeTimestamp(m_currentTime);  // Recording Date and Time
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
    writer.writeLongAD(0x0800, 2, 1, 0);                        // Root Directory ICB
    writer.writeUDFString("*OSTA UDF Compliant", 32);           // Domain Identifier
    m_buffer[442] = 0x03;                                       // Domain Flags: Hard and Soft Write-Protect
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
    writer.writeDescriptorTag(DescriptorTag::PrimVol, absoluteSectorNum());

    writer.writeLE32(0x00);  // Volume Descriptor Sequence Number
    writer.writeLE32(0x00);  // Primary Volume Descriptor Number
    writer.writeDString(m_volumeLabel, 32);

    writer.writeLE16(0x01);  // Volume Sequence Number
    writer.writeLE16(0x01);  // Maximum Volume Sequence Number
    writer.writeLE16(0x02);  // Interchange Level
    writer.writeLE16(0x02);  // Maximum Interchange Level
    writer.writeLE32(0x01);  // Character Set List
    writer.writeLE32(0x01);  // Maximum Character Set List

    std::string volId = strToUpperCase(int32uToHex(m_volumeId));
    volId = strPadLeft(volId, 8, '0');
    const std::string volumeSetIdentifier = volId + std::string("        ") + m_volumeLabel;
    writer.writeDString(volumeSetIdentifier.c_str(), 128);

    // Descriptor Character Set
    m_buffer[200] = 0x00;  // CS0 coded character set
    strcpy(reinterpret_cast<char *>(m_buffer) + 201, "OSTA Compressed Unicode");

    // Explanatory Character Set
    m_buffer[264] = 0x00;  // CS0 coded character set
    strcpy(reinterpret_cast<char *>(m_buffer) + 265, "OSTA Compressed Unicode");

    // skip Volume Abstract
    // skip Volume Copyright Notice

    // Application Identifier
    m_buffer[344] = 0x00;  // CS0 coded character set
    strcpy(reinterpret_cast<char *>(m_buffer) + 345, m_appId.c_str());

    writeTimestamp(m_buffer + 376, m_currentTime);  // timestamp

    // Implementation Identifier
    m_buffer[388] = 0x00;  // CS0 coded character set
    strcpy(reinterpret_cast<char *>(m_buffer) + 389, m_impId.c_str());

    strcpy(reinterpret_cast<char *>(m_buffer) + 420, m_appId.c_str());

    // skip Predecessor Volume Descriptor Sequence Location (BP 484) = 0L
    m_buffer[488] = 1;  // Flags (BP 488)

    // 490..511 Reserved

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeImpUseDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DescriptorTag::ImplUseVol, absoluteSectorNum());

    const auto buff32 = reinterpret_cast<uint32_t *>(m_buffer);
    buff32[4] = 0x01;  // Descriptor Sequence Number

    const std::string impId = std::string("*UDF LV Info");
    m_buffer[20] = 0x00;
    writeUDFString(m_buffer + 20, impId.c_str(), 32);

    // Explanatory Character Set
    m_buffer[52] = 0x00;  // CS0 coded character set
    strcpy(reinterpret_cast<char *>(m_buffer) + 53, "OSTA Compressed Unicode");

    // logical volume identifier
    writeDString(m_buffer + 116, m_volumeLabel.c_str(), 128);

    strcpy(reinterpret_cast<char *>(m_buffer) + 0x161, m_impId.c_str());  // ImplementationID
    strcpy(reinterpret_cast<char *>(m_buffer) + 0x180, m_appId.c_str());  // ImplementationUse

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writePartitionDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DescriptorTag::Partition, absoluteSectorNum());

    const auto buff32 = reinterpret_cast<uint32_t *>(m_buffer);
    const auto buff16 = reinterpret_cast<uint16_t *>(m_buffer);
    buff32[4] = 0x02;                                           // Descriptor Sequence Number
    buff16[10] = 0x01;                                          // partition flags
    buff16[11] = 0x00;                                          // Partition Number
    strcpy(reinterpret_cast<char *>(m_buffer) + 25, "+NSR03");  // Partition Contents
    // skip Partition Header Descriptor (all zero)
    buff32[184 / 4] = 0x01;                                             // Access Type
    buff32[188 / 4] = m_partitionStartAddress;                          // Partition Starting Location (576K address )
    buff32[192 / 4] = m_partitionEndAddress - m_partitionStartAddress;  // Partition Length field

    strcpy(reinterpret_cast<char *>(m_buffer) + 0xc5, m_impId.c_str());  // ImplementationID
    strcpy(reinterpret_cast<char *>(m_buffer) + 0xe4, m_appId.c_str());  // ImplementationUse

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeLogicalVolumeDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DescriptorTag::LogicalVol, absoluteSectorNum());
    const auto buff32 = reinterpret_cast<uint32_t *>(m_buffer);
    const auto buff16 = reinterpret_cast<uint16_t *>(m_buffer);

    buff32[4] = 0x03;                                                            // Volume Descriptor Sequence Number
    strcpy(reinterpret_cast<char *>(m_buffer) + 21, "OSTA Compressed Unicode");  // Descriptor Character Set
    // Logical Volume Identifier
    writeDString(m_buffer + 84, m_volumeLabel.c_str(), 128);
    buff32[212 / 4] = SECTOR_SIZE;                              // Logical Block Size
    writeUDFString(m_buffer + 216, "*OSTA UDF Compliant", 32);  // Domain Identifier

    // Domain Flags #03: Hard and Soft Write-Protect
    m_buffer[242] = 0x03;

    // Logical Volume Contents Use
    m_buffer[249] = 0x10;
    m_buffer[256] = 0x01;

    m_buffer[264] = 0x46;  // partition Map Table Length ( in bytes)
    m_buffer[268] = 0x02;  // Number of Partition Maps.

    // Implementation Identifier
    strcpy(reinterpret_cast<char *>(m_buffer) + 273, m_impId.c_str());
    strcpy(reinterpret_cast<char *>(m_buffer) + 304, m_appId.c_str());

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
    const uint32_t mirrorHeaderLocation = (m_metadataMirrorLBN - 1 - m_partitionStartAddress);
    buff16[490 / 2] = static_cast<uint16_t>(mirrorHeaderLocation % 65536);
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
    writeDescriptorTag(m_buffer, DescriptorTag::UnallocSpace, absoluteSectorNum());

    const auto buff32 = reinterpret_cast<uint32_t *>(m_buffer);

    buff32[4] = 0x04;  // sequence number

    calcDescriptorCRC(m_buffer, 24);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeTerminationDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DescriptorTag::Terminating, absoluteSectorNum());

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeLogicalVolumeIntegrityDescriptor()
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DescriptorTag::LogicalVolIntegrity, absoluteSectorNum());

    const auto buff32 = reinterpret_cast<uint32_t *>(m_buffer);
    const auto buff16 = reinterpret_cast<uint16_t *>(m_buffer);

    writeTimestamp(m_buffer + 16, m_currentTime);
    buff32[7] = 0x01;              // Integrity Type
    buff32[8] = buff32[9] = 0x00;  // Next Integrity Extent
    // Logical Volume Contents Use (32 bytes)
    buff32[10] = m_objectUniqId;
    buff32[11] = 0;  // uniq ID hi

    buff32[72 / 4] = 0x02;                                             // Number of Partitions
    buff32[76 / 4] = static_cast<uint32_t>(46 + m_appId.size());       // Length of Implementation Use
    buff32[80 / 4] = buff32[84 / 4] = 0;                               // Free Space Table
    buff32[88 / 4] = m_partitionEndAddress - m_partitionStartAddress;  // main partition size
    buff32[92 / 4] = m_metadataFileLen / SECTOR_SIZE;

    strcpy(reinterpret_cast<char *>(m_buffer) + 97, m_impId.c_str());

    buff32[0x80 / 4] = m_totalFiles;
    buff32[0x84 / 4] = m_totalDirectories;
    buff16[0x88 / 2] = 0x0250;  // Minimum UDF Read Revision
    buff16[0x8a / 2] = 0x0250;  // Minimum UDF Write Revision
    buff16[0x8c / 2] = 0x0250;  // Maximum UDF Write Revision

    strcpy(reinterpret_cast<char *>(m_buffer) + 142, m_appId.c_str());

    calcDescriptorCRC(m_buffer, static_cast<uint16_t>(142 + m_appId.size()));
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeAnchorVolumeDescriptor(const uint32_t endPartitionAddr)
{
    memset(m_buffer, 0, sizeof(m_buffer));
    writeDescriptorTag(m_buffer, DescriptorTag::AnchorVolPtr, absoluteSectorNum());

    const auto buff32 = reinterpret_cast<uint32_t *>(m_buffer);
    buff32[4] = 0x8000;  // point of the main volume descriptor
    buff32[5] = 0x20;
    buff32[6] = 0x8000;
    buff32[7] = endPartitionAddr;  // set size to allocated sectors amount

    calcDescriptorCRC(m_buffer, 512);
    m_file.write(m_buffer, SECTOR_SIZE);
}

void IsoWriter::writeSector(const uint8_t *sectorData) { m_file.write(sectorData, SECTOR_SIZE); }

int IsoWriter::writeRawData(const uint8_t *data, const int size) { return m_file.write(data, size); }

void IsoWriter::checkLayerBreakPoint(const int maxExtentSize)
{
    const int lbn = absoluteSectorNum();
    if (lbn < m_layerBreakPoint && lbn + maxExtentSize > m_layerBreakPoint)
    {
        const int rest = m_layerBreakPoint - lbn;
        const int size = rest * SECTOR_SIZE;
        const auto tmpBuffer = new uint8_t[size];
        memset(tmpBuffer, 0, size);
        m_file.write(tmpBuffer, size);
        delete[] tmpBuffer;
        m_lastWritedObjectID = -1;
    }
}

void IsoWriter::setLayerBreakPoint(const int lbn) { m_layerBreakPoint = lbn; }

IsoHeaderData IsoHeaderData::normal()
{
    return IsoHeaderData{"*tsMuxeR " TSMUXER_VERSION, std::string("*tsMuxeR ") + int32uToHex(random32()), time(nullptr),
                         random32()};
}

IsoHeaderData IsoHeaderData::reproducible() { return IsoHeaderData{"*tsMuxeR", "*tsMuxeR", 1, 1593630000}; }
