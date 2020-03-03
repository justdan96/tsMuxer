#ifndef TSMUXER_H_
#define TSMUXER_H_

#include <QHeaderView>
#include <QProcess>
#include <QSoundEffect>
#include <QTimer>
#include <QTranslator>
#include <QWidget>

#include "codecinfo.h"

class QFileDialog;
class QTableWidgetItem;
class QComboBox;
class QCheckBox;

class MuxForm;
namespace Ui
{
class TsMuxerWindow;
}

class QnCheckBoxedHeaderView;

typedef QList<double> ChapterList;

enum MplsType
{
    MPLS_NONE,
    MPLS_PRIMARY,
    MPLS_M2TS,
};

class TsMuxerWindow : public QWidget
{
    Q_OBJECT
   public:
    TsMuxerWindow();

    void addFiles(const QList<QUrl>& files);
   signals:
    void tsMuxerSuccessFinished();
    void codecListReady();
    void fileAdded();
    void fileAppended();

   private:
    void onAddBtnClick();
    void readFromStdout();
    void readFromStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void trackLVItemSelectionChanged();
    void inputFilesLVChanged();
    void trackLVItemChanged(QTableWidgetItem* item);
    void onVideoCheckBoxChanged(int state);
    void onPulldownCheckBoxChanged(int state);
    void onVideoComboBoxChanged(int index);
    void onAudioSubtitlesParamsChanged();
    void onEditDelayChanged(int i);
    void onFontBtnClicked();
    void onColorBtnClicked();
    void onGeneralCheckboxClicked();
    void onGeneralSpinboxValueChanged();
    void onChapterParamsChanged();
    void onSplitCutParamsChanged();
    void onSavedParamChanged();
    void onFontParamsChanged();
    void onRemoveBtnClick();
    void onAppendButtonClick();
    void onRemoveTrackButtonClick();
    void onMoveUpButtonCLick();
    void onMoveDownButtonCLick();
    void RadioButtonMuxClick();
    void outFileNameChanged();
    void saveFileDialog();
    void startMuxing();
    void saveMetaFileBtnClick();
    void continueAppendFile();
    void continueAddFile();
    void onTsMuxerCodecInfoReceived();
    void addFile();
    void appendFile();
    void onOpacityTimer();
    void updateMetaLines();
    void at_sectionCheckstateChanged(Qt::CheckState state);
    void updateMuxTime1();
    void updateMuxTime2();
    void onLanguageComboBoxIndexChanged(int);

   protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void updateMaxOffsets();
    void updateCustomChapters();

    void writeSettings();
    bool readSettings();
    bool readGeneralSettings(const QString& prefix);
    QString getOutputDir() const;

   private:
    struct MPLSFileInfo
    {
        MPLSFileInfo(const QString& _name, double _duration) : name(_name), duration(_duration) {}
        MPLSFileInfo() : duration(0) {}
        QString name;
        double duration;
    };

    bool checkFileDuplicate(const QString& fileName);
    void tsMuxerExecute(const QStringList& args);
    void addLines(const QByteArray& arr, QList<QString>& outList, bool isError);
    void doAppendInt(const QString& fileName, const QString& parentFileName, double duration, bool doublePrefix,
                     MplsType mplsRole);
    bool isDiskOutput() const;
    QString getMuxOpts();
    QString getFileList(QtvCodecInfo* codecInfo);
    QString getAudioMetaInfo(QtvCodecInfo* codecInfo);
    QString getVideoMetaInfo(QtvCodecInfo* codecInfo);
    QString getFileList();
    QString getSrtParams();
    int findLangByCode(const QString& code);
    void setComboBoxText(QComboBox* comboBox, const QString& text);
    QtvCodecInfo* getCodecInfo(int idx);
    QtvCodecInfo* getCurrentCodec();
    void delTracksByFileName(const QString& fileName);
    void deleteTrack(int idx);
    void updateNum();
    void modifyOutFileName(const QString fileName);
    void moveRow(int index, int index2);
    bool saveMetaFile(const QString& metaName);
    void updateBtns(QWidget* w);
    double getRendererAnimationTime() const;
    void setRendererAnimationTime(double value);
    void setTextItemColor(QString str);
    QString getDefaultOutputFileName() const;
    void updateCurrentColor(int dr, int dg, int db, int rowIndex);
    void colorizeCurrentRow(const QtvCodecInfo* codecInfo, int rowIndex = -1);

    void addTrackToDefaultComboBox(int trackRowIdx);
    void removeTrackFromDefaultComboBox(int trackRowIdx);
    void removeTrackFromDefaultComboBox(QComboBox*, QCheckBox*, int comboBoxIdx, int trackRowIdx);
    void updateTracksComboBox(QComboBox*);
    void moveTrackInDefaultComboBox(int oldIndex, int newIndex);
    void postMoveComboBoxUpdate(QComboBox*, const QVariant& preMoveIndex, int oldIndex, int newIndex);

    // QTemporaryFile* tempFile;
    // QString tempFileName;
    QString metaName;
    Ui::TsMuxerWindow* ui;
    QFileDialog* openFileDialog;
    int disableUpdatesCnt;
    bool processFinished;
    int processExitCode;
    QProcess::ExitStatus processExitStatus;
    QProcess::ProcessError processError;
    QList<MPLSFileInfo> mplsFileList;
    QProcess proc;
    QList<QString> procStdOutput;
    QList<QString> procErrOutput;
    bool outFileNameModified;
    QString oldFileName;
    bool outFileNameDisableChange;
    QString mSaveDialogFilter;
    MuxForm* muxForm;
    QString newFileName;
    QList<QtvCodecInfo> codecList;
    ChapterList chapters;
    double fileDuration;
    bool runInMuxMode;
    QString lastInputDir;
    QString lastOutputDir;
    QList<QUrl> addFileList;
    QTimer opacityTimer;
    bool m_updateMeta;
    bool m_3dMode;
    QnCheckBoxedHeaderView* m_header;
    QString lastSourceDir;

    QTranslator qtCoreTranslator;
    QTranslator tsMuxerTranslator;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;

    QSoundEffect sound;
    void myPlaySound(const QString& fileName);
    bool isVideoCropped();
};

Q_DECLARE_METATYPE(ChapterList);

#endif
