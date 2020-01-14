#ifndef CODECINFO_H
#define CODECINFO_H

#include <QList>
#include <QString>

struct QtvCodecInfo
{
    QtvCodecInfo() = default;
    QtvCodecInfo(const QtvCodecInfo&) = default;

    int trackID = 0;
    int width = 0;
    int height = 0;
    QString displayName;
    QString programName;
    QString descr;
    QString lang;
    int delay = 0;
    int subTrack = 0;
    bool dtsDownconvert = false;
    bool isSecondary = false;
    int offsetId = -1;
    int maxPgOffsets = 0;
    QList<QString> fileList;
    bool checkFPS = false;
    bool checkLevel = false;
    int addSEIMethod = 1;
    bool addSPS = true;
    int delPulldown = -1;
    QString fpsText;
    QString fpsTextOrig;
    QString levelText;
    // for append
    int nested = 0;
    QtvCodecInfo* parent = nullptr;
    QtvCodecInfo* child = nullptr;
    bool bindFps = true;
    QStringList mplsFiles;
    QString arText;
    bool enabledByDefault = true;
};

#endif
