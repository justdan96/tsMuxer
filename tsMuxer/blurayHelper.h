#ifndef _BLURAY_HELPER_H__
#define _BLURAY_HELPER_H__

#include <string>
#include <vector>

#include "fs/file.h"
#include "vod_common.h"

class IsoWriter;
struct FileEntryInfo;
class TSMuxer;
class AbstractOutputStream;
class MuxerManager;

class BlurayHelper : public FileFactory
{
   public:
    BlurayHelper();
    ~BlurayHelper() override;

    bool open(const std::string& dst, DiskType dt, int64_t diskSize = 0, int extraISOBlocks = 0,
              bool useReproducibleIsoHeader = false);
    bool createBluRayDirs();
    bool writeBluRayFiles(const MuxerManager& muxer, bool usedBlankPL, int mplsNum, int blankNum, bool stereoMode);
    bool createCLPIFile(TSMuxer* muxer, int clpiNum, bool doLog);
    bool createMPLSFile(TSMuxer* mainMuxer, TSMuxer* subMuxer, int autoChapterLen, std::vector<double> customChapters,
                        DiskType dt, int mplsOffset, bool isMvcBaseViewR);

    std::string m2tsFileName(int num);
    std::string ssifFileName(int num);

    IsoWriter* isoWriter() const;

    void close();
    // file factory interface

    AbstractOutputStream* createFile() override;
    bool isVirtualFS() const override;
    void setVolumeLabel(const std::string& label);

   private:
    std::string m_dstPath;
    DiskType m_dt;
    IsoWriter* m_isoWriter;
};

#endif  // _BLURAY_HELPER_H__
