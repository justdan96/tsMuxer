#ifndef _BLURAY_HELPER_H__
#define _BLURAY_HELPER_H__

#include <string>
#include <vector>
#include "vod_common.h"
#include "fs/file.h"

class IsoWriter;
struct FileEntryInfo;
class TSMuxer;
class AbstractOutputStream;

class BlurayHelper: public FileFactory
{
public:
    BlurayHelper();
    ~BlurayHelper() override;

    bool open(const std::string& dst, DiskType dt, int64_t diskSize = 0, int extraISOBlocks = 0);
    bool createBluRayDirs();
    bool writeBluRayFiles(bool usedBlackPL, int mplsNum, int blankNum, bool stereoMode);
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

#endif // _BLURAY_HELPER_H__
