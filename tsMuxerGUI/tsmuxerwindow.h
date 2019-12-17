#ifndef TSMUXER_H_
#define TSMUXER_H_

#include <QtGui>
#include <QFileDialog>
#include <QProcess>
#include <QSound>

#include "ui_tsmuxerwindow.h"
#include "muxForm.h"

typedef QList<double> ChapterList;

enum MplsType {
    MPLS_NONE,
    MPLS_PRIMARY,
    MPLS_M2TS,
};


class QnCheckBoxedHeaderView: public QHeaderView {
    Q_OBJECT
        typedef QHeaderView base_type;
public:
    explicit QnCheckBoxedHeaderView(QWidget *parent = nullptr);

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);
signals:
    void checkStateChanged(Qt::CheckState state);
protected:
    void paintEvent(QPaintEvent *e) override;
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;
    private slots:
        void at_sectionClicked(int logicalIndex);
private:
    Qt::CheckState m_checkState;
    int m_checkColumnIndex;
};

struct QtvCodecInfo
{
  QtvCodecInfo():
    trackID(0),
    width(0),
    height(0),
    displayName(),
    programName(),
    descr(),
    lang(),
    delay(0),
    subTrack(0),
    dtsDownconvert(false),
    isSecondary(false),
    offsetId(-1),
    maxPgOffsets(0),
    fileList(),
    checkFPS(false),
    checkLevel(false),
    addSEIMethod(1),
    addSPS(true),
    delPulldown(-1),
    fpsText(),
    fpsTextOrig(),
    levelText(),
    nested(0),
    parent(nullptr),
    child(nullptr),
    bindFps(true),
    mplsFiles(),
    arText(),
    enabledByDefault(true) {}

  int trackID;
  int width;
  int height;
  QString displayName;
  QString programName;
  QString descr;
  QString lang;
  int delay;
  int subTrack;
  bool dtsDownconvert;
  bool isSecondary;
  int offsetId;
  int maxPgOffsets;
  QList<QString> fileList;
  bool checkFPS;
  bool checkLevel;
  int addSEIMethod;
  bool addSPS;
  int delPulldown;
  QString fpsText;
  QString fpsTextOrig;
  QString levelText;
  // for append
  int nested;
  QtvCodecInfo* parent;
  QtvCodecInfo* child;
  bool bindFps;
  QStringList mplsFiles;
  QString arText;
  bool enabledByDefault;
};

class TsMuxerWindow: public QWidget
{
    Q_OBJECT
public:
  TsMuxerWindow();
  ~TsMuxerWindow() override;

    void addFiles(const QList<QUrl>& files);
signals:
  void tsMuxerSuccessFinished();
  void codecListReady();
  void fileAdded();
  void fileAppended();
private slots:
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
  void getCodecInfo();
  void addFile();
  void appendFile();
  void onOpacityTimer();
    void updateMetaLines();
    void at_sectionCheckstateChanged(Qt::CheckState state);
    void updateMuxTime1();
    void updateMuxTime2();
protected:
    void closeEvent(QCloseEvent * event ) override;
    bool eventFilter(QObject *obj, QEvent* event) override;
    void updateMaxOffsets();
    void updateCustomChapters();

    void writeSettings();
    bool readSettings();
    QString getOutputDir() const;
private:
    struct MPLSFileInfo
    {
        MPLSFileInfo(const QString& _name, double _duration): name(_name), duration(_duration) {}
        MPLSFileInfo(): duration(0) {}
        QString name;
        double duration;
    };

    bool checkFileDuplicate(const QString& fileName);
    void tsMuxerExecute(const QStringList& args);
    void addLines(const QByteArray& arr, QList<QString>& outList, bool isError);
    void doAppendInt(const QString& fileName, const QString& parentFileName, double duration, bool doublePrefix, MplsType mplsRole);
    bool isDiskOutput() const;
    QString getMuxOpts();
    QString getFileList(QtvCodecInfo* codecInfo);
    QString getAudioMetaInfo(QtvCodecInfo* codecInfo);
    QString getVideoMetaInfo(QtvCodecInfo* codecInfo);
    QString getFileList();
    QString getSrtParams();
    int findLangByCode(const QString& code);
    void setComboBoxText(QComboBox* comboBox, const QString& text);
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

    //QTemporaryFile* tempFile;
    //QString tempFileName;
    QString metaName;
    Ui::TsMuxerWindow ui;
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
    QString saveDialogFilter;
    MuxForm muxForm;
    QString newFileName;
    QList<QtvCodecInfo> codecList;
    ChapterList chapters;
    double fileDuration;
    bool runInMuxMode;
    QString lastInputDir;
    QString lastOutputDir;
    QList <QUrl> addFileList;
    QTimer opacityTimer;
    bool m_updateMeta;
    bool m_3dMode;
    QnCheckBoxedHeaderView* m_header;
    QString lastSourceDir;

  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent* event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dragLeaveEvent ( QDragLeaveEvent * event) override;

  QTemporaryFile* tempSoundFile;
  QSound* sound;
  void myPlaySound(const QString& fileName);
  bool isVideoCropped();
};

Q_DECLARE_METATYPE(ChapterList);

#endif
