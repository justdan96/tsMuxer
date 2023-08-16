#ifndef BLURAY_HELPER_H_
#define BLURAY_HELPER_H_

#include <string>
#include <vector>

#include "fs/file.h"
#include "vod_common.h"

class IsoWriter;
struct FileEntryInfo;
class TSMuxer;
class AbstractOutputStream;
class MuxerManager;

class BlurayHelper final : public FileFactory
{
   public:
    BlurayHelper();
    ~BlurayHelper() override;

    bool open(const std::string& dst, DiskType dt, int64_t diskSize = 0, int extraISOBlocks = 0,
              bool useReproducibleIsoHeader = false);
    void createBluRayDirs() const;
    bool writeBluRayFiles(const MuxerManager& muxer, bool usedBlankPL, int mplsNum, int blankNum,
                                        bool stereoMode) const;
    bool createCLPIFile(TSMuxer* muxer, int clpiNum, bool doLog) const;
    bool createMPLSFile(TSMuxer* mainMuxer, TSMuxer* subMuxer, int autoChapterLen,
                        const std::vector<double>& customChapters, DiskType dt, int mplsOffset,
                        bool isMvcBaseViewR) const;

    [[nodiscard]] std::string m2tsFileName(int num) const;
    [[nodiscard]] std::string ssifFileName(int num) const;

    [[nodiscard]] IsoWriter* isoWriter() const;

    void close();
    // file factory interface

    AbstractOutputStream* createFile() override;
    [[nodiscard]] bool isVirtualFS() const override;
    void setVolumeLabel(const std::string& label) const;

   private:
    std::string m_dstPath;
    DiskType m_dt;
    IsoWriter* m_isoWriter;
};

#endif  // _BLURAY_HELPER_H_
