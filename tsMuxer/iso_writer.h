#ifndef ISO_WRITER_H_
#define ISO_WRITER_H_

#include <fs/file.h>

#include <map>
#include <string>

static constexpr int SECTOR_SIZE = 2048;
static constexpr int ALLOC_BLOCK_SIZE = 1024 * 64;
static constexpr int METADATA_START_ADDR = 1024 * 640 / SECTOR_SIZE;
static constexpr int MAX_EXTENT_SIZE = 0x40000000;
static constexpr uint32_t NEXT_EXTENT = 0xc0000000;

static constexpr int64_t META_BLOCK_PER_DATA = 16 * 1000000000ll;

// it can be allocated inside single sector of a extended file
static constexpr int MAX_EXTENTS_IN_EXTFILE = (SECTOR_SIZE - 216 - 32) / 16;
static constexpr int MAX_EXTENTS_IN_EXTCONT = (SECTOR_SIZE - 24 - 32) / 16;

static constexpr int MAIN_INTERLEAVE_BLOCKSIZE = 6144 * 3168;
static constexpr int SUB_INTERLEAVE_BLOCKSIZE = 6144 * 1312;

static constexpr int MAX_MAIN_MUXER_RATE = 48000000;
static constexpr int MAX_4K_MUXER_RATE = 109000000;
static constexpr int MAX_SUBMUXER_RATE = 35000000;

enum class DescriptorTag
{
    SpoaringTable = 0,  // UDF
    PrimVol = 1,
    AnchorVolPtr = 2,
    VolPtr = 3,
    ImplUseVol = 4,
    Partition = 5,
    LogicalVol = 6,
    UnallocSpace = 7,
    Terminating = 8,
    LogicalVolIntegrity = 9,

    FileSet = 256,
    FileId = 257,
    AllocationExtent = 258,
    Indirect = 259,
    Terminal = 260,
    File = 261,
    ExtendedAttrHeader = 262,
    UnallocatedSpace = 263,
    SpaceBitmap = 264,
    PartitionIntegrity = 265,
    ExtendedFileEntry = 266
};

enum class FileTypes
{
    Unspecified = 0,
    UnallocatedSpace = 1,
    PartitionIntegrityEntry = 2,
    IndirectEntry = 3,
    Directory = 4,
    File = 5,

    SystemStreamDirectory = 13,

    RealtimeFile = 249,
    Metadata = 250,
    MetadataMirror = 251,
    MetadataBitmap = 252,
};

class ByteFileWriter
{
   public:
    ByteFileWriter();
    void setBuffer(uint8_t* buffer, int len);
    void writeLE8(uint8_t value);
    void writeLE16(uint16_t value);
    void writeLE32(uint32_t value);

    void writeDescriptorTag(DescriptorTag tag, uint32_t tagLocation);
    void closeDescriptorTag(int dataSize = -1) const;

    void writeIcbTag(uint8_t fileType);
    void writeLongAD(uint32_t lenBytes, uint32_t pos, uint16_t partition, uint32_t id);
    void writeDString(const char* value, int64_t len = -1);
    void writeDString(const std::string& value, int64_t len = -1);
    void writeCharSpecString(const char* value, int len);
    void writeUDFString(const char* value, int len);
    void skipBytes(int value);
    void doPadding(int padSize);
    void writeTimestamp(time_t time);

    int64_t size() const;

   private:
    uint8_t* m_buffer;
    uint8_t* m_bufferEnd;
    uint8_t* m_curPos;
    uint8_t* m_tagPos;
};

struct FileEntryInfo;
class IsoWriter;
class ISOFile;

struct Extent
{
    Extent() : lbnPos(0), size(0) {}
    Extent(int lbn, int64_t _size) : lbnPos(lbn), size(_size) {}
    int lbnPos;  // absolute logic block number
    int64_t size;
};
typedef std::vector<Extent> ExtentList;

struct MappingEntry
{
    MappingEntry() : parentLBN(0), LBN(0) {}
    MappingEntry(const int _parentLBN, const int _lbn) : parentLBN(_parentLBN), LBN(_lbn) {}

    int parentLBN;
    int LBN;
};

struct FileEntryInfo
{
    FileEntryInfo(IsoWriter* owner, FileEntryInfo* parent, uint8_t objectId, FileTypes fileType);
    ~FileEntryInfo();

    int32_t write(const uint8_t* data, int32_t len);
    bool setName(const std::string& name);
    void close();
    void setSubMode(bool value);
    void addExtent(const Extent& extent);

    FileEntryInfo* subDirByName(const std::string& name) const;
    FileEntryInfo* fileByName(const std::string& name) const;

   private:
    void addSubDir(FileEntryInfo* dir);
    void addFile(FileEntryInfo* file);
    void serialize() const;  // flush directory tree to a disk
    int allocateEntity(int sectorNum);
    void writeEntity(ByteFileWriter& writer, const FileEntryInfo* subDir) const;
    void serializeDir() const;
    void serializeFile() const;
    bool isFile() const;

    friend class IsoWriter;
    friend class ISOFile;

    std::vector<FileEntryInfo*> m_subDirs;
    std::vector<FileEntryInfo*> m_files;
    IsoWriter* m_owner;
    FileEntryInfo* m_parent;
    int m_sectorNum;
    int m_sectorsUsed;
    uint8_t m_objectId;
    std::string m_name;
    FileTypes m_fileType;

    ExtentList m_extents;
    int64_t m_fileSize;
    uint8_t* m_sectorBuffer;
    int m_sectorBufferSize;
    bool m_subMode;  // sub file in stereo interleaved mode
};

struct IsoHeaderData
{
    std::string appId;
    std::string impId;
    time_t fileTime;
    uint32_t volumeId;

    static IsoHeaderData normal();
    static IsoHeaderData reproducible();
};

class IsoWriter
{
   public:
    IsoWriter(const IsoHeaderData&);
    ~IsoWriter();

    void setVolumeLabel(const std::string& value);
    bool open(const std::string& fileName, int64_t diskSize, int extraISOBlocks);

    bool createDir(const std::string& dir);
    ISOFile* createFile();

    bool createInterleavedFile(const std::string& inFile1, const std::string& inFile2, const std::string& outFile);

    void close();

    void setLayerBreakPoint(int lbn);

   private:
    enum class Partition
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
    int writeExtendedFileEntryDescriptor(bool namedStream, uint8_t objectId, FileTypes fileType, uint64_t len,
                                         uint32_t pos, uint16_t linkCount, const ExtentList* extents = nullptr);
    void writeFileSetDescriptor();
    void writeAllocationExtentDescriptor(const ExtentList* extents, size_t start, size_t indexEnd);

    static void writeEntity(const FileEntryInfo* dir);
    int allocateEntity(FileEntryInfo* entity, int sectorNum);

    void sectorSeek(Partition partition, int pos) const;
    void writeSector(const uint8_t* sectorData);
    int32_t absoluteSectorNum() const;

    static void writeIcbTag(bool namedStream, uint8_t* buffer, FileTypes fileType);
    void writeDescriptors();
    void allocateMetadata();
    void writeMetadata(int lbn);

    FileEntryInfo* mkdir(const char* name, FileEntryInfo* parent = nullptr);
    FileEntryInfo* getEntryByName(const std::string& name, FileTypes fileType);
    FileEntryInfo* createFileEntry(FileEntryInfo* parent, FileTypes fileType);

    friend class ByteFileWriter;
    friend struct FileEntryInfo;
    friend class ISOFile;

    std::string m_volumeLabel;
    std::string m_impId;
    std::string m_appId;
    uint32_t m_volumeId;
    File m_file;
    uint8_t m_buffer[SECTOR_SIZE];
    time_t m_currentTime;

    uint8_t m_objectUniqId;
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

class ISOFile final : public AbstractOutputStream
{
   public:
    ISOFile(IsoWriter* owner) : AbstractOutputStream(), m_owner(owner), m_entry(nullptr) {}
    ~ISOFile() override { close(); }

    int write(const void* data, uint32_t len) override;
    bool open(const char* name, unsigned int oflag, unsigned int systemDependentFlags = 0) override;
    void sync() override;
    bool close() override;
    int64_t size() const override;
    void setSubMode(bool value) const;

   private:
    IsoWriter* m_owner;
    FileEntryInfo* m_entry;
};

#endif  // ISO_WRITER_H_
