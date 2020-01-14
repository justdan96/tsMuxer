#ifndef __ISO_WRITER_H__
#define __ISO_WRITER_H__

#include <fs/file.h>

#include <map>
#include <string>

static const uint32_t SECTOR_SIZE = 2048;
static const uint32_t ALLOC_BLOCK_SIZE = 1024 * 64;
static const uint32_t METADATA_START_ADDR = 1024 * 640 / SECTOR_SIZE;
static const uint32_t MAX_EXTENT_SIZE = 0x40000000;
static const uint32_t NEXT_EXTENT = 0xc0000000;

static const int64_t META_BLOCK_PER_DATA = 16 * 1000000000ll;

// it can be allocated inside single sector of a extended file
static const int MAX_EXTENTS_IN_EXTFILE = (SECTOR_SIZE - 216 - 32) / 16;
static const int MAX_EXTENTS_IN_EXTCONT = (SECTOR_SIZE - 24 - 32) / 16;

static const int MAIN_INTERLEAVE_BLOCKSIZE = 6144 * 3168;
static const int SUB_INTERLEAVE_BLOCKSIZE = 6144 * 1312;

static const int MAX_MAIN_MUXER_RATE = 48000000;
static const int MAX_SUBMUXER_RATE = 35000000;

enum DescriptorTag
{
    DESC_TYPE_SpoaringTable = 0,  // UDF
    DESC_TYPE_PrimVol = 1,
    DESC_TYPE_AnchorVolPtr = 2,
    DESC_TYPE_VolPtr = 3,
    DESC_TYPE_ImplUseVol = 4,
    DESC_TYPE_Partition = 5,
    DESC_TYPE_LogicalVol = 6,
    DESC_TYPE_UnallocSpace = 7,
    DESC_TYPE_Terminating = 8,
    DESC_TYPE_LogicalVolIntegrity = 9,

    DESC_TYPE_FileSet = 256,
    DESC_TYPE_FileId = 257,
    DESC_TYPE_AllocationExtent = 258,
    DESC_TYPE_Indirect = 259,
    DESC_TYPE_Terminal = 260,
    DESC_TYPE_File = 261,
    DESC_TYPE_ExtendedAttrHeader = 262,
    DESC_TYPE_UnallocatedSpace = 263,
    DESC_TYPE_SpaceBitmap = 264,
    DESC_TYPE_PartitionIntegrity = 265,
    DESC_TYPE_ExtendedFile = 266
};

enum FileTypes
{
    FileType_Unspecified = 0,
    FileType_UnallocatedSpace = 1,
    FileType_PartitionIntegrityEntry = 2,
    FileType_IndirectEntry = 3,
    FileType_Directory = 4,
    FileType_File = 5,

    FileType_SystemStreamDirectory = 13,

    FileType_RealtimeFile = 249,
    FileType_Metadata = 250,
    FileType_MetadataMirror = 251,
    FileType_MetadataBitmap = 252,
};

class ByteFileWriter
{
   public:
    ByteFileWriter();
    void setBuffer(uint8_t* buffer, int len);
    void writeLE8(uint8_t value);
    void writeLE16(uint16_t value);
    void writeLE32(uint16_t value);

    void writeDescriptorTag(DescriptorTag tag, uint32_t tagLocation);
    void closeDescriptorTag(int dataSize = -1);

    void writeIcbTag(uint8_t fileType);
    void writeLongAD(uint32_t lenBytes, uint32_t pos, uint16_t partition, uint32_t id);
    void writeDString(const char* value, int len = -1);
    void writeDString(const std::string& value, int len = -1);
    void writeCharSpecString(const char* value, int len);
    void writeUDFString(const char* value, int len);
    void skipBytes(int value);
    void doPadding(int padSize);
    void writeTimestamp(time_t t);

    int size() const;

   private:
    uint8_t* m_buffer;
    uint8_t* m_bufferEnd;
    uint8_t* m_curPos;
    uint8_t* m_tagPos;
};

class FileEntryInfo;
class IsoWriter;
class ISOFile;

struct Extent
{
    Extent() : lbnPos(0), size(0) {}
    Extent(int lbn, int64_t _size) : lbnPos(lbn), size(_size) {}
    int lbnPos;  // absolute logic block number
    int size;
};
typedef std::vector<Extent> ExtentList;

struct MappingEntry
{
    MappingEntry() : parentLBN(0), LBN(0) {}
    MappingEntry(int _parentLBN, int _LBN) : parentLBN(_parentLBN), LBN(_LBN) {}

    int parentLBN;
    int LBN;
};

struct FileEntryInfo
{
   public:
    FileEntryInfo(IsoWriter* owner, FileEntryInfo* parent, uint32_t objectId, FileTypes fileType);
    ~FileEntryInfo();

    int write(const uint8_t* data, uint32_t len);
    bool setName(const std::string& name);
    void close();
    void setSubMode(bool value);
    void addExtent(const Extent& extent);

    FileEntryInfo* subDirByName(const std::string& name) const;
    FileEntryInfo* fileByName(const std::string& name) const;

   private:
    void addSubDir(FileEntryInfo* dir);
    void addFile(FileEntryInfo* file);
    void serialize();  // flush directory tree to a disk
    int allocateEntity(int sectorNum);
    void writeEntity(ByteFileWriter& writer, FileEntryInfo* subDir);
    void serializeDir();
    void serializeFile();
    bool isFile() const;

   private:
    friend class IsoWriter;
    friend class ISOFile;

    std::vector<FileEntryInfo*> m_subDirs;
    std::vector<FileEntryInfo*> m_files;
    IsoWriter* m_owner;
    FileEntryInfo* m_parent;
    int m_sectorNum;
    int m_sectorsUsed;
    uint32_t m_objectId;
    std::string m_name;
    FileTypes m_fileType;

    ExtentList m_extents;
    int64_t m_fileSize;
    uint8_t* m_sectorBuffer;
    int m_sectorBufferSize;
    bool m_subMode;  // sub file in stereo interleaved mode
};

class IsoWriter
{
   public:
    IsoWriter();
    ~IsoWriter();

    void setVolumeLabel(const std::string& value);
    bool open(const std::string& fileName, int64_t diskSize, int extraISOBlocks);

    bool createDir(const std::string& fileName);
    ISOFile* createFile();

    bool createInterleavedFile(const std::string& inFile1, const std::string& inFile2, const std::string& outFile);

    void close();

    void setLayerBreakPoint(int lbn);

   private:
    enum Partition
    {
        MainPartition,
        MetadataPartition
    };

    void setMetaPartitionSize(int size);
    int writeRawData(const uint8_t* data, int size);
    void checkLayerBreakPoint(int maxExtentSize);
    void writePrimaryVolumeDescriptor();
    void writeAnchorVolumeDescriptor(uint32_t endPartitionAddr);
    void writeImpUseDescriptor();
    void writePartitionDescriptor();
    void writeLogicalVolumeDescriptor();
    void writeUnallocatedSpaceDescriptor();
    void writeTerminationDescriptor();
    void writeLogicalVolumeIntegrityDescriptor();
    int writeExtentFileDescriptor(uint8_t fileType, uint64_t len, uint32_t pos, int linkCount, ExtentList* extents = 0);
    void writeFileSetDescriptor();
    void writeAllocationExtentDescriptor(ExtentList* extents, int start, int indexEnd);
    // void writeFileIdentifierDescriptor();

    void writeEntity(FileEntryInfo* dir);
    int allocateEntity(FileEntryInfo* dir, int sectorNum);

    void sectorSeek(Partition partition, int pos);
    void writeSector(uint8_t* sectorData);
    uint32_t absoluteSectorNum();

    void writeIcbTag(uint8_t* buffer, uint8_t fileType);
    void writeDescriptors();
    void allocateMetadata();
    void writeMetadata(int lbn);

    FileEntryInfo* mkdir(const char* name, FileEntryInfo* parent = 0);
    FileEntryInfo* getEntryByName(const std::string& name, FileTypes fileType);
    FileEntryInfo* createFileEntry(FileEntryInfo* parent, FileTypes fileType);

   private:
    friend class ByteFileWriter;
    friend class FileEntryInfo;
    friend class ISOFile;

    std::string m_volumeLabel;
    std::string m_impId;
    std::string m_appId;
    uint32_t m_volumeId;
    File m_file;
    uint8_t m_buffer[SECTOR_SIZE];
    time_t m_currentTime;

    uint32_t m_objectUniqId;
    uint32_t m_totalFiles;
    uint32_t m_totalDirectories;
    uint32_t m_volumeSize;

    // int m_sectorNum; // where to allocate new data
    FileEntryInfo* m_rootDirInfo;
    FileEntryInfo* m_systemStreamDir;
    FileEntryInfo* m_metadataMappingFile;

    int m_partitionStartAddress;
    int m_partitionEndAddress;
    int m_metadataFileLen;
    int m_systemStreamLBN;

    int m_metadataLBN;  // where to allocate data
    int m_metadataMirrorLBN;
    int m_curMetadataPos;
    int m_tagLocationBaseAddr;
    int m_lastWritedObjectID;

    std::map<int, MappingEntry> m_mappingEntries;
    bool m_opened;
    int m_layerBreakPoint;  // in sectors
};

class ISOFile : public AbstractOutputStream
{
   public:
    ISOFile(IsoWriter* owner) : AbstractOutputStream(), m_owner(owner), m_entry(0) {}
    ~ISOFile() override { close(); }

    int write(const void* data, uint32_t len) override;
    bool open(const char* name, unsigned int oflag, unsigned int systemDependentFlags = 0) override;
    void sync() override;
    virtual bool close() override;
    virtual int64_t size() const override;
    void setSubMode(bool value);

   private:
    IsoWriter* m_owner;
    FileEntryInfo* m_entry;
};

#endif  // __ISO_WRITER_H__
