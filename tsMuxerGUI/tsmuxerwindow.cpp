#include "tsmuxerwindow.h"

#include <QColorDialog>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QDropEvent>
#include <QFileDialog>
#include <QFontDialog>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QSound>
#include <QTemporaryFile>
#include <QTime>

#include "checkboxedheaderview.h"
#include "codecinfo.h"
#include "lang_codes.h"
#include "muxForm.h"
#include "ui_tsmuxerwindow.h"

namespace
{
QString fileDialogFilter()
{
    return TsMuxerWindow::tr(
        "All supported media files (*.aac *.mpv *.mpa *.avc *.mvc *.264 *.h264 *.ac3 *.dts *.dtshd *.ts *.m2ts *.mts *.ssif *.mpg *.mpeg *.vob *.evo *.mkv *.mka *.mks *.mp4 *.m4a *.m4v *.mov *.sup *.wav *.w64 *.pcm *.m1v *.m2v *.vc1 *.hevc *.hvc *.265 *.h265 *.mpls *.mpl *.srt);;\
AC3/E-AC3 (*.ac3 *.ddp);;\
AAC (advanced audio coding) (*.aac);;\
AVC/MVC/H.264 elementary stream (*.avc *.mvc *.264 *.h264);;\
HEVC (High Efficiency Video Codec) (*.hevc *.hvc *.265 *.h265);;\
Digital Theater System (*.dts);;\
DTS-HD Master Audio (*.dtshd);;\
Mpeg video elementary stream (*.mpv *.m1v *.m2v);;\
Mpeg audio elementary stream (*.mpa);;\
Transport Stream (*.ts);;\
BDAV Transport Stream (*.m2ts *.mts *.ssif);;\
Program Stream (*.mpg *.mpeg *.vob *.evo);;\
Matroska audio/video files (*.mkv *.mka *.mks);;\
MP4 audio/video files (*.mp4 *.m4a *.m4v);;\
Quick time audio/video files (*.mov);;\
Blu-ray play list (*.mpls *.mpl);;\
Blu-ray PGS subtitles (*.sup);;\
Text subtitles (*.srt);;\
WAVE - Uncompressed PCM audio (*.wav *.w64);;\
RAW LPCM Stream (*.pcm);;\
All files (*.*)");
}

QString TI_DEFAULT_TAB_NAME() { return TsMuxerWindow::tr("General track options"); }

QString TI_DEMUX_TAB_NAME() { return TsMuxerWindow::tr("Demux options"); }

QString TS_SAVE_DIALOG_FILTER() { return TsMuxerWindow::tr("Transport stream (*.ts);;all files (*.*)"); }

QString M2TS_SAVE_DIALOG_FILTER() { return TsMuxerWindow::tr("BDAV Transport Stream (*.m2ts);;all files (*.*)"); }

QString ISO_SAVE_DIALOG_FILTER() { return TsMuxerWindow::tr("Disk image (*.iso);;all files (*.*)"); }

QSettings *settings = nullptr;

enum FileCustomData
{
    MplsItemRole = Qt::UserRole,
    FileNameRole,
    ChaptersRole,
    FileDurationRole
};

static const QString FILE_JOIN_PREFIX(" ++ ");

bool doubleCompare(double a, double b) { return qAbs(a - b) < 1e-6; }

QString closeDirPath(const QString &src)
{
    if (src.isEmpty())
        return src;
    if (src[src.length() - 1] == '/' || src[src.length() - 1] == '\\')
        return src;
    return src + QDir::separator();
}

QString unquoteStr(QString val)
{
    val = val.trimmed();
    if (val.isEmpty())
        return val;
    if (val.at(0) == '\"')
    {
        if (val.right(1) == "\"")
            return val.mid(1, val.length() - 2);
        else
            return val.mid(1, val.length() - 1);
    }
    else
    {
        if (val.right(1) == "\"")
            return val.mid(0, val.length() - 1);
        else
            return val;
    }
}

bool isVideoCodec(const QString &displayName)
{
    return displayName == "H.264" || displayName == "MVC" || displayName == "VC-1" || displayName == "MPEG-2" ||
           displayName == "HEVC";
}

QString floatToTime(double time, char msSeparator = '.')
{
    int iTime = (int)time;
    int hour = iTime / 3600;
    iTime -= hour * 3600;
    int min = iTime / 60;
    iTime -= min * 60;
    int sec = iTime;
    int msec = (int)((time - (int)time) * 1000.0);
    QString str;
    str += QString::number(hour).rightJustified(2, '0');
    str += ':';
    str += QString::number(min).rightJustified(2, '0');
    str += ':';
    str += QString::number(sec).rightJustified(2, '0');
    str += msSeparator;
    str += QString::number(msec).rightJustified(3, '0');
    return str;
}

QTime qTimeFromFloat(double secondsF)
{
    int seconds = (int)secondsF;
    int ms = (secondsF - seconds) * 1000.0;
    int hours = seconds / 3600;
    seconds -= hours * 3600;
    int minute = seconds / 60;
    seconds -= minute * 60;
    return QTime(hours, minute, seconds, ms);
}

int qTimeToMsec(const QTime &time)
{
    return (time.hour() * 3600 + time.minute() * 60 + time.second()) * 1000 + time.msec();
}

double timeToFloat(const QString &chapterStr)
{
    if (chapterStr.size() == 0)
        return 0;
    QStringList timeParts = chapterStr.split(':');
    double sec = 0;
    if (timeParts.size() > 0)
        sec = timeParts[timeParts.size() - 1].toDouble();
    int min = 0;
    if (timeParts.size() > 1)
        min = timeParts[timeParts.size() - 2].toInt();
    int hour = 0;
    if (timeParts.size() > 2)
        hour = timeParts[timeParts.size() - 3].toInt();
    return hour * 3600 + min * 60 + sec;
}

QString changeFileExt(const QString &value, const QString &newExt)
{
    QFileInfo fi(unquoteStr(value));
    if (fi.suffix().length() > 0 || (!value.isEmpty() && value.right(1) == "."))
        return unquoteStr(value.left(value.length() - fi.suffix().length()) + newExt);
    else
        return unquoteStr(value) + "." + newExt;
}

QString fpsTextToFpsStr(const QString &fpsText)
{
    int p = fpsText.indexOf('/');
    if (p >= 0)
    {
        auto left = fpsText.mid(0, p).toFloat();
        auto right = fpsText.mid(p + 1).toFloat();
        return QString::number(left / right, 'f', 3);
    }
    else
        return fpsText;
}

float extractFloatFromDescr(const QString &str, const QString &pattern)
{
    try
    {
        int p = 0;
        if (!pattern.isEmpty())
            p = str.indexOf(pattern);
        if (p >= 0)
        {
            p += pattern.length();
            int p2 = p;
            while (p2 < str.length() &&
                   ((str.at(p2) >= '0' && str.at(p2) <= '9') || str.at(p2) == '.' || str.at(p2) == '-'))
                p2++;
            return str.mid(p, p2 - p).toFloat();
        }
    }
    catch (...)
    {
        return 0;
    }
    return 0;
}

QString quoteStr(const QString &val)
{
    QString rez;
    if (val.isEmpty())
        return "";
    if (val.at(0) != '\"')
        rez = '\"' + val;
    else
        rez = val;
    if (val.at(val.length() - 1) != '\"')
        rez += '\"';
    return rez;
}

QString myUnquoteStr(const QString &val) { return unquoteStr(val); }

QString getComboBoxTrackText(int idx, const QtvCodecInfo &codecInfo)
{
    auto text = QString("[%1] %2").arg(idx + 1).arg(codecInfo.displayName);
    if (!codecInfo.lang.isEmpty())
    {
        text.append(", lang : ");
        text.append(codecInfo.lang);
    }
    text.append(", ");
    text.append(codecInfo.descr);
    return text;
}

void initLanguageComboBox(QComboBox *comboBox)
{
    comboBox->addItem("English", "en");  // 0th index is also used as default if the language isn't set in the settings.
    comboBox->addItem(QString::fromUtf8("Русский"), "ru");
    comboBox->addItem(QString::fromUtf8("Français"), "fr");
    comboBox->addItem(QString::fromUtf8("简体中文"), "zh");
    comboBox->setCurrentIndex(-1);  // makes sure currentIndexChanged() is emitted when reading settings.
}

}  // namespace

// ----------------------- TsMuxerWindow -------------------------------------

QString TsMuxerWindow::getOutputDir() const
{
    QString result = ui->radioButtonOutoutInInput->isChecked() ? lastSourceDir : lastOutputDir;
    if (!result.isEmpty())
        result = QDir::toNativeSeparators(closeDirPath(result));
    return result;
}

QString TsMuxerWindow::getDefaultOutputFileName() const
{
    QString prefix = getOutputDir();

    if (ui->radioButtonTS->isChecked())
        return prefix + QString("default.ts");
    else if (ui->radioButtonM2TS->isChecked())
        return prefix + QString("default.m2ts");
    else if (ui->radioButtonBluRayISO->isChecked())
        return prefix + QString("default.iso");
    else
        return prefix;
}

TsMuxerWindow::TsMuxerWindow()
    : ui(new Ui::TsMuxerWindow),
      disableUpdatesCnt(0),
      outFileNameModified(false),
      outFileNameDisableChange(false),
      muxForm(new MuxForm(this)),
      m_updateMeta(true),
      m_3dMode(false)
{
    ui->setupUi(this);
    setUiMetaItemsData();
    qApp->installTranslator(&qtCoreTranslator);
    qApp->installTranslator(&tsMuxerTranslator);
    initLanguageComboBox(ui->languageSelectComboBox);
    setWindowTitle("tsMuxeR GUI " TSMUXER_VERSION);
    lastInputDir = QDir::homePath();
    lastOutputDir = QDir::homePath();

    void (QComboBox::*comboBoxIndexChanged)(int) = &QComboBox::currentIndexChanged;
    connect(ui->languageSelectComboBox, comboBoxIndexChanged, this, &TsMuxerWindow::onLanguageComboBoxIndexChanged);

    QString path = QFileInfo(QApplication::arguments()[0]).absolutePath();
    QString iniName = QDir::toNativeSeparators(path) + QDir::separator() + QString("tsMuxerGUI.ini");

    settings = new QSettings(QSettings::UserScope, "Network Optix", "tsMuxeR");
    readSettings();

    if (QFile::exists(iniName))
    {
        delete settings;
        settings = new QSettings(iniName, QSettings::IniFormat);
        settings->setIniCodec("UTF-8");
        if (!readSettings())
            writeSettings();  // copy current registry settings to the ini file
    }

    ui->outFileName->setText(getDefaultOutputFileName());

    m_header = new QnCheckBoxedHeaderView(this);
    ui->trackLV->setHorizontalHeader(m_header);
    ui->trackLV->horizontalHeader()->setStretchLastSection(true);
    // ui->trackLV->model()->setHeaderData(0, Qt::Vertical, "#");
    m_header->setVisible(true);
    m_header->setSectionsClickable(true);

    connect(m_header, &QnCheckBoxedHeaderView::checkStateChanged, this, &TsMuxerWindow::at_sectionCheckstateChanged);

    /////////////////////////////////////////////////////////////
    for (int i = 0; i <= 3600; i += 5 * 60) ui->memoChapters->insertPlainText(floatToTime(i, '.') + '\n');

    mSaveDialogFilter = TS_SAVE_DIALOG_FILTER();
    const static int colWidths[] = {28, 200, 62, 38, 10};
    for (unsigned i = 0u; i < sizeof(colWidths) / sizeof(int); ++i)
        ui->trackLV->horizontalHeader()->resizeSection(i, colWidths[i]);
    ui->trackLV->setWordWrap(false);

    ui->listViewFont->horizontalHeader()->resizeSection(0, 65);
    ui->listViewFont->horizontalHeader()->resizeSection(1, 185);
    for (int i = 0; i < ui->listViewFont->rowCount(); ++i)
    {
        ui->listViewFont->setRowHeight(i, 16);
        ui->listViewFont->item(i, 0)->setFlags(ui->listViewFont->item(i, 0)->flags() & (~Qt::ItemIsEditable));
        ui->listViewFont->item(i, 1)->setFlags(ui->listViewFont->item(i, 0)->flags() & (~Qt::ItemIsEditable));
    }
    void (QSpinBox::*spinBoxValueChanged)(int) = &QSpinBox::valueChanged;
    void (QDoubleSpinBox::*doubleSpinBoxValueChanged)(double) = &QDoubleSpinBox::valueChanged;
    connect(&opacityTimer, &QTimer::timeout, this, &TsMuxerWindow::onOpacityTimer);
    connect(ui->trackLV, &QTableWidget::itemSelectionChanged, this, &TsMuxerWindow::trackLVItemSelectionChanged);
    connect(ui->trackLV, &QTableWidget::itemChanged, this, &TsMuxerWindow::trackLVItemChanged);
    connect(ui->inputFilesLV, &QListWidget::currentRowChanged, this, &TsMuxerWindow::inputFilesLVChanged);
    connect(ui->addBtn, &QPushButton::clicked, this, &TsMuxerWindow::onAddBtnClick);
    connect(ui->btnAppend, &QPushButton::clicked, this, &TsMuxerWindow::onAppendButtonClick);
    connect(ui->removeFile, &QPushButton::clicked, this, &TsMuxerWindow::onRemoveBtnClick);
    connect(ui->removeTrackBtn, &QPushButton::clicked, this, &TsMuxerWindow::onRemoveTrackButtonClick);
    connect(ui->moveupBtn, &QPushButton::clicked, this, &TsMuxerWindow::onMoveUpButtonCLick);
    connect(ui->movedownBtn, &QPushButton::clicked, this, &TsMuxerWindow::onMoveDownButtonCLick);
    connect(ui->checkFPS, &QCheckBox::stateChanged, this, &TsMuxerWindow::onVideoCheckBoxChanged);
    connect(ui->checkBoxLevel, &QCheckBox::stateChanged, this, &TsMuxerWindow::onVideoCheckBoxChanged);
    connect(ui->comboBoxSEI, comboBoxIndexChanged, this, &TsMuxerWindow::onVideoCheckBoxChanged);
    connect(ui->checkBoxSecondaryVideo, &QCheckBox::stateChanged, this, &TsMuxerWindow::onVideoCheckBoxChanged);
    connect(ui->checkBoxSPS, &QCheckBox::stateChanged, this, &TsMuxerWindow::onVideoCheckBoxChanged);
    connect(ui->checkBoxRemovePulldown, &QCheckBox::stateChanged, this, &TsMuxerWindow::onPulldownCheckBoxChanged);
    connect(ui->comboBoxFPS, comboBoxIndexChanged, this, &TsMuxerWindow::onVideoComboBoxChanged);
    connect(ui->comboBoxLevel, comboBoxIndexChanged, this, &TsMuxerWindow::onVideoComboBoxChanged);
    connect(ui->comboBoxAR, comboBoxIndexChanged, this, &TsMuxerWindow::onVideoComboBoxChanged);
    connect(ui->checkBoxKeepFps, &QCheckBox::stateChanged, this, &TsMuxerWindow::onAudioSubtitlesParamsChanged);
    connect(ui->dtsDwnConvert, &QCheckBox::stateChanged, this, &TsMuxerWindow::onAudioSubtitlesParamsChanged);
    connect(ui->secondaryCheckBox, &QCheckBox::stateChanged, this, &TsMuxerWindow::onAudioSubtitlesParamsChanged);
    connect(ui->langComboBox, comboBoxIndexChanged, this, &TsMuxerWindow::onAudioSubtitlesParamsChanged);
    connect(ui->offsetsComboBox, comboBoxIndexChanged, this, &TsMuxerWindow::onAudioSubtitlesParamsChanged);
    connect(ui->comboBoxPipCorner, comboBoxIndexChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->comboBoxPipSize, comboBoxIndexChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->spinBoxPipOffsetH, spinBoxValueChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->spinBoxPipOffsetV, spinBoxValueChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->checkBoxSound, &QCheckBox::stateChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->editDelay, spinBoxValueChanged, this, &TsMuxerWindow::onEditDelayChanged);
    connect(ui->muxTimeEdit, &QTimeEdit::timeChanged, this, &TsMuxerWindow::updateMuxTime1);
    connect(ui->muxTimeClock, spinBoxValueChanged, this, &TsMuxerWindow::updateMuxTime2);
    connect(ui->fontButton, &QPushButton::clicked, this, &TsMuxerWindow::onFontBtnClicked);
    connect(ui->colorButton, &QPushButton::clicked, this, &TsMuxerWindow::onColorBtnClicked);
    connect(ui->checkBoxVBR, &QPushButton::clicked, this, &TsMuxerWindow::onGeneralCheckboxClicked);
    connect(ui->spinBoxMplsNum, spinBoxValueChanged, this, &TsMuxerWindow::onGeneralCheckboxClicked);
    connect(ui->spinBoxM2tsNum, spinBoxValueChanged, this, &TsMuxerWindow::onGeneralCheckboxClicked);
    connect(ui->checkBoxBlankPL, &QPushButton::clicked, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->checkBoxV3, &QCheckBox::stateChanged, this, &TsMuxerWindow::updateMetaLines);
    connect(ui->BlackplaylistCombo, spinBoxValueChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->checkBoxNewAudioPes, &QAbstractButton::clicked, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->checkBoxCrop, &QCheckBox::stateChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->checkBoxRVBR, &QAbstractButton::clicked, this, &TsMuxerWindow::onGeneralCheckboxClicked);
    connect(ui->checkBoxCBR, &QAbstractButton::clicked, this, &TsMuxerWindow::onGeneralCheckboxClicked);
    connect(ui->radioButtonStoreOutput, &QAbstractButton::clicked, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->radioButtonOutoutInInput, &QAbstractButton::clicked, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->editVBVLen, spinBoxValueChanged, this, &TsMuxerWindow::onGeneralSpinboxValueChanged);
    connect(ui->editMaxBitrate, doubleSpinBoxValueChanged, this, &TsMuxerWindow::onGeneralSpinboxValueChanged);
    connect(ui->editMinBitrate, doubleSpinBoxValueChanged, this, &TsMuxerWindow::onGeneralSpinboxValueChanged);
    connect(ui->editCBRBitrate, doubleSpinBoxValueChanged, this, &TsMuxerWindow::onGeneralSpinboxValueChanged);
    connect(ui->rightEyeCheckBox, &QCheckBox::stateChanged, this, &TsMuxerWindow::updateMetaLines);
    connect(ui->radioButtonAutoChapter, &QAbstractButton::clicked, this, &TsMuxerWindow::onChapterParamsChanged);
    connect(ui->radioButtonNoChapters, &QAbstractButton::clicked, this, &TsMuxerWindow::onChapterParamsChanged);
    connect(ui->radioButtonCustomChapters, &QAbstractButton::clicked, this, &TsMuxerWindow::onChapterParamsChanged);
    connect(ui->spinEditChapterLen, spinBoxValueChanged, this, &TsMuxerWindow::onChapterParamsChanged);
    connect(ui->memoChapters, &QPlainTextEdit::textChanged, this, &TsMuxerWindow::onChapterParamsChanged);
    connect(ui->noSplit, &QAbstractButton::clicked, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->splitByDuration, &QAbstractButton::clicked, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->splitBySize, &QAbstractButton::clicked, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->spinEditSplitDuration, spinBoxValueChanged, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->editSplitSize, doubleSpinBoxValueChanged, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->comboBoxMeasure, comboBoxIndexChanged, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->checkBoxCut, &QCheckBox::stateChanged, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->cutStartTimeEdit, &QTimeEdit::timeChanged, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->cutEndTimeEdit, &QTimeEdit::timeChanged, this, &TsMuxerWindow::onSplitCutParamsChanged);
    connect(ui->spinEditBorder, spinBoxValueChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->comboBoxAnimation, comboBoxIndexChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->lineSpacing, doubleSpinBoxValueChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->spinEditOffset, spinBoxValueChanged, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->radioButtonTS, &QAbstractButton::clicked, this, &TsMuxerWindow::RadioButtonMuxClick);
    connect(ui->radioButtonM2TS, &QAbstractButton::clicked, this, &TsMuxerWindow::RadioButtonMuxClick);
    connect(ui->radioButtonBluRay, &QAbstractButton::clicked, this, &TsMuxerWindow::RadioButtonMuxClick);
    connect(ui->radioButtonBluRayISO, &QAbstractButton::clicked, this, &TsMuxerWindow::RadioButtonMuxClick);
    connect(ui->radioButtonAVCHD, &QAbstractButton::clicked, this, &TsMuxerWindow::RadioButtonMuxClick);
    connect(ui->radioButtonDemux, &QAbstractButton::clicked, this, &TsMuxerWindow::RadioButtonMuxClick);
    connect(ui->outFileName, &QLineEdit::textChanged, this, &TsMuxerWindow::outFileNameChanged);
    connect(ui->DiskLabelEdit, &QLineEdit::textChanged, this, &TsMuxerWindow::onGeneralCheckboxClicked);
    connect(ui->btnBrowse, &QAbstractButton::clicked, this, &TsMuxerWindow::saveFileDialog);
    connect(ui->buttonMux, &QAbstractButton::clicked, this, &TsMuxerWindow::startMuxing);
    connect(ui->buttonSaveMeta, &QAbstractButton::clicked, this, &TsMuxerWindow::saveMetaFileBtnClick);
    connect(ui->radioButtonOutoutInInput, &QAbstractButton::clicked, this, &TsMuxerWindow::onSavedParamChanged);
    connect(ui->defaultAudioTrackComboBox, comboBoxIndexChanged, this, &TsMuxerWindow::updateMetaLines);
    connect(ui->defaultSubTrackComboBox, comboBoxIndexChanged, this, &TsMuxerWindow::updateMetaLines);
    connect(ui->defaultAudioTrackCheckBox, &QCheckBox::stateChanged, this, &TsMuxerWindow::updateMetaLines);
    connect(ui->defaultSubTrackCheckBox, &QCheckBox::stateChanged, this, &TsMuxerWindow::updateMetaLines);
    connect(ui->defaultSubTrackForcedOnlyCheckBox, &QCheckBox::stateChanged, this, &TsMuxerWindow::updateMetaLines);
    connect(ui->pipTransparencySpinBox, spinBoxValueChanged, this, &TsMuxerWindow::updateMetaLines);

    connect(&proc, &QProcess::readyReadStandardOutput, this, &TsMuxerWindow::readFromStdout);
    connect(&proc, &QProcess::readyReadStandardError, this, &TsMuxerWindow::readFromStderr);
    void (QProcess::*processFinished)(int, QProcess::ExitStatus) = &QProcess::finished;
    connect(&proc, processFinished, this, &TsMuxerWindow::onProcessFinished);
    void (QProcess::*processError)(QProcess::ProcessError) = &QProcess::error;
    connect(&proc, processError, this, &TsMuxerWindow::onProcessError);

    ui->DiskLabel->setVisible(false);
    ui->DiskLabelEdit->setVisible(false);

    ui->label_Donate->installEventFilter(this);

    ui->langComboBox->addItem("und (Undetermined)");
    ui->langComboBox->addItem("--------- common ---------");
    for (auto &&lang : shortLangList)
    {
        ui->langComboBox->addItem(QString("%1 (%2)").arg(lang.code).arg(lang.lang), QString::fromUtf8(lang.code));
    }
    ui->langComboBox->addItem("---------- all ----------");
    for (auto &&lang : fullLangList)
    {
        ui->langComboBox->addItem(QString("%1 (%2)").arg(lang.code).arg(lang.lang), QString::fromUtf8(lang.code));
    }
    trackLVItemSelectionChanged();

    ui->trackSplitter->setStretchFactor(0, 10);
    ui->trackSplitter->setStretchFactor(1, 100);

    writeSettings();
}

TsMuxerWindow::~TsMuxerWindow()
{
    disableUpdatesCnt = 0;
    writeSettings();
    delete settings;
}

void TsMuxerWindow::onTsMuxerCodecInfoReceived()
{
    m_updateMeta = false;
    codecList.clear();
    int p;
    QtvCodecInfo *codecInfo = 0;
    int lastTrackID = 0;
    QString tmpStr;
    bool firstMark = true;
    codecList.clear();
    mplsFileList.clear();
    chapters.clear();
    fileDuration = 0;
    for (int i = 0; i < procStdOutput.size(); ++i)
    {
        p = procStdOutput[i].indexOf("Track ID:    ");
        if (p >= 0)
        {
            lastTrackID = procStdOutput[i].mid(QString("Track ID:    ").length()).toInt();
            codecList << QtvCodecInfo();
            codecInfo = &(codecList.back());
            codecInfo->trackID = lastTrackID;
        }

        p = procStdOutput[i].indexOf("Stream type: ");
        if (p >= 0)
        {
            if (lastTrackID == 0)
            {
                codecList << QtvCodecInfo();
                codecInfo = &(codecList.back());
            }
            codecInfo->descr = "Can't detect codec";
            codecInfo->displayName = procStdOutput[i].mid(QString("Stream type: ").length());
            if (codecInfo->displayName != "H.264" && codecInfo->displayName != "MVC")
            {
                codecInfo->addSEIMethod = 0;
                codecInfo->addSPS = false;
            }
            if (codecInfo->displayName == "HEVC" && !ui->checkBoxV3->isChecked())
                ui->checkBoxV3->setChecked(true);
            lastTrackID = 0;
        }
        p = procStdOutput[i].indexOf("Stream ID:   ");
        if (p >= 0)
            codecInfo->programName = procStdOutput[i].mid(QString("Stream ID:   ").length());
        p = procStdOutput[i].indexOf("Stream info: ");
        if (p >= 0)
            codecInfo->descr = procStdOutput[i].mid(QString("Stream info: ").length());
        p = procStdOutput[i].indexOf("Stream lang: ");
        if (p >= 0)
            codecInfo->lang = procStdOutput[i].mid(QString("Stream lang: ").length());

        p = procStdOutput[i].indexOf("Stream delay: ");
        if (p >= 0)
        {
            tmpStr = procStdOutput[i].mid(QString("Stream delay: ").length());
            codecInfo->delay = tmpStr.toInt();
        }

        p = procStdOutput[i].indexOf("subTrack: ");
        if (p >= 0)
        {
            tmpStr = procStdOutput[i].mid(QString("subTrack: ").length());
            codecInfo->subTrack = tmpStr.toInt();
        }

        p = procStdOutput[i].indexOf("Secondary: 1");
        if (p == 0)
        {
            codecInfo->isSecondary = true;
        }
        p = procStdOutput[i].indexOf("Unselected: 1");
        if (p == 0)
        {
            codecInfo->enabledByDefault = false;
        }

        if (procStdOutput[i].startsWith("Error: "))
        {
            tmpStr = procStdOutput[i].mid(QString("Error: ").length());
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Not supported"));
            msgBox.setText(tmpStr);
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();
        }
        else if (procStdOutput[i].startsWith("File #"))
        {
            tmpStr = procStdOutput[i].mid(17);  // todo: check here
            mplsFileList << MPLSFileInfo(tmpStr, 0.0);
        }
        else if (procStdOutput[i].startsWith("Duration:"))
        {
            tmpStr = procStdOutput[i].mid(10);  // todo: check here
            if (!mplsFileList.empty())
            {
                mplsFileList.last().duration = timeToFloat(tmpStr);
            }
            else
            {
                fileDuration = timeToFloat(tmpStr);
            }
        }
        else if (procStdOutput[i].startsWith("Base view: "))
        {
            tmpStr = procStdOutput[i].mid(11);  // todo: check here
            ui->rightEyeCheckBox->setChecked(tmpStr.trimmed() == "right-eye");
        }
        else if (procStdOutput[i].startsWith("start-time: "))
        {
            tmpStr = procStdOutput[i].mid(12);
            if (tmpStr.contains(':'))
            {
                double secondsF = timeToFloat(tmpStr);
                ui->muxTimeEdit->setTime(qTimeFromFloat(secondsF));
            }
            else
            {
                ui->muxTimeClock->setValue(tmpStr.toInt());
            }
        }

        p = procStdOutput[i].indexOf("Marks: ");
        if (p == 0)
        {
            if (firstMark)
            {
                firstMark = false;
                ui->radioButtonCustomChapters->setChecked(true);
            }
            QStringList stringList = procStdOutput[i].mid(7).split(' ');
            for (int k = 0; k < stringList.size(); ++k)
                if (!stringList[k].isEmpty())
                    chapters << timeToFloat(stringList[k]);
        }
    }
    if (fileDuration == 0 && !mplsFileList.isEmpty())
    {
        foreach (const MPLSFileInfo &mplsFile, mplsFileList)
            fileDuration += mplsFile.duration;
    }

    m_updateMeta = true;
    if (codecList.isEmpty())
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Unsupported format"));
        msgBox.setText(tr("Can't detect stream type. File name: \"%1\"").arg(newFileName));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return;
    }
    updateMetaLines();
    emit codecListReady();
}

int TsMuxerWindow::findLangByCode(const QString &code)
{
    QString addr;
    for (int i = 0; i < ui->langComboBox->count(); ++i)
    {
        addr = ui->langComboBox->itemData(i).toString();
        if (!addr.isEmpty() && code == addr)
        {
            return i;
        }
    }
    return 0;
}

QtvCodecInfo *TsMuxerWindow::getCodecInfo(int idx)
{
    auto iCodec = ui->trackLV->item(idx, 0)->data(Qt::UserRole).toLongLong();
    return reinterpret_cast<QtvCodecInfo *>(iCodec);
}

QtvCodecInfo *TsMuxerWindow::getCurrentCodec()
{
    auto row = ui->trackLV->currentRow();
    if (row == -1)
        return nullptr;
    return getCodecInfo(row);
}

void TsMuxerWindow::onVideoComboBoxChanged(int)
{
    if (disableUpdatesCnt)
        return;
    QtvCodecInfo *codecInfo = getCurrentCodec();
    if (!codecInfo)
        return;
    codecInfo->fpsText = ui->comboBoxFPS->itemText(ui->comboBoxFPS->currentIndex());
    codecInfo->levelText = ui->comboBoxLevel->itemText(ui->comboBoxLevel->currentIndex());
    codecInfo->arText = ui->comboBoxAR->currentData().toString();
    updateMetaLines();
}

void TsMuxerWindow::onVideoCheckBoxChanged(int state)
{
    Q_UNUSED(state);
    if (disableUpdatesCnt)
        return;
    QtvCodecInfo *codecInfo = getCurrentCodec();
    if (codecInfo == nullptr)
        return;
    ui->comboBoxFPS->setEnabled(ui->checkFPS->isChecked());
    codecInfo->checkFPS = ui->checkFPS->isChecked();
    ui->comboBoxLevel->setEnabled(ui->checkBoxLevel->isChecked());
    codecInfo->checkLevel = ui->checkBoxLevel->isChecked();
    codecInfo->addSEIMethod = ui->comboBoxSEI->currentIndex();
    codecInfo->addSPS = ui->checkBoxSPS->isChecked();
    codecInfo->isSecondary = ui->checkBoxSecondaryVideo->isChecked();
    colorizeCurrentRow(codecInfo);
    updateMetaLines();
}

void TsMuxerWindow::updateCurrentColor(int dr, int dg, int db, int row)
{
    if (row == -1)
        row = ui->trackLV->currentRow();
    if (row == -1)
        return;
    QColor color = ui->trackLV->palette().color(QPalette::Base);
    color.setRed(qBound(0, color.red() + dr, 255));
    color.setGreen(qBound(0, color.green() + dg, 255));
    color.setBlue(qBound(0, color.blue() + db, 255));
    for (int i = 0; i < 5; ++i)
    {
        QModelIndex index = ui->trackLV->model()->index(row, i);
        ui->trackLV->model()->setData(index, QBrush(color), Qt::BackgroundColorRole);
    }
}

void TsMuxerWindow::colorizeCurrentRow(const QtvCodecInfo *codecInfo, int rowIndex)
{
    if (codecInfo->isSecondary)
        updateCurrentColor(-40, 0, 0, rowIndex);
    else
        updateCurrentColor(0, 0, 0, rowIndex);
}

void TsMuxerWindow::addTrackToDefaultComboBox(int trackRowIdx)
{
    auto codecInfo = getCodecInfo(trackRowIdx);
    auto text = getComboBoxTrackText(trackRowIdx, *codecInfo);
    if (codecInfo->programName.startsWith('A'))
    {
        ui->defaultAudioTrackComboBox->addItem(text, trackRowIdx);
    }
    else if (codecInfo->programName.startsWith('S'))
    {
        ui->defaultSubTrackComboBox->addItem(text, trackRowIdx);
    }
}

void TsMuxerWindow::removeTrackFromDefaultComboBox(QComboBox *targetComboBox, QCheckBox *targetCheckBox,
                                                   int comboBoxIdx, int trackRowIdx)
{
    if (targetComboBox->currentData().toInt() == trackRowIdx)
    {
        targetCheckBox->setChecked(false);
    }
    targetComboBox->removeItem(comboBoxIdx);
}

static void fixupIndices(QComboBox *comboBox, int removedTrackIdx)
{
    for (int i = 0; i < comboBox->count(); ++i)
    {
        auto trackIdx = comboBox->itemData(i).toInt();
        if (trackIdx > removedTrackIdx)
        {
            comboBox->setItemData(i, trackIdx - 1);
        }
    }
}

void TsMuxerWindow::removeTrackFromDefaultComboBox(int trackRowIdx)
{
    auto comboBoxIdx = ui->defaultAudioTrackComboBox->findData(trackRowIdx);
    if (comboBoxIdx != -1)
    {
        removeTrackFromDefaultComboBox(ui->defaultAudioTrackComboBox, ui->defaultAudioTrackCheckBox, comboBoxIdx,
                                       trackRowIdx);
    }
    comboBoxIdx = ui->defaultSubTrackComboBox->findData(trackRowIdx);
    if (comboBoxIdx != -1)
    {
        removeTrackFromDefaultComboBox(ui->defaultSubTrackComboBox, ui->defaultSubTrackCheckBox, comboBoxIdx,
                                       trackRowIdx);
    }
    fixupIndices(ui->defaultAudioTrackComboBox, trackRowIdx);
    fixupIndices(ui->defaultSubTrackComboBox, trackRowIdx);
}

void TsMuxerWindow::updateTracksComboBox(QComboBox *comboBox)
{
    for (int i = 0; i < comboBox->count(); ++i)
    {
        auto trackRowIdx = comboBox->itemData(i).toInt();
        auto codecInfo = getCodecInfo(trackRowIdx);
        comboBox->setItemText(i, getComboBoxTrackText(trackRowIdx, *codecInfo));
    }
}

void TsMuxerWindow::moveTrackInDefaultComboBox(int oldTrackRowIdx, int newTrackRowIdx)
{
    auto currentSubTrack = ui->defaultSubTrackComboBox->currentData();
    auto currentAudioTrack = ui->defaultAudioTrackComboBox->currentData();
    ui->defaultSubTrackComboBox->clear();
    ui->defaultAudioTrackComboBox->clear();
    for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    {
        addTrackToDefaultComboBox(i);
    }
    postMoveComboBoxUpdate(ui->defaultAudioTrackComboBox, currentAudioTrack, oldTrackRowIdx, newTrackRowIdx);
    postMoveComboBoxUpdate(ui->defaultSubTrackComboBox, currentSubTrack, oldTrackRowIdx, newTrackRowIdx);
}

void TsMuxerWindow::postMoveComboBoxUpdate(QComboBox *comboBox, const QVariant &preMoveIndex, int oldIndex,
                                           int newIndex)
{
    if (!preMoveIndex.isValid())
    {
        return;
    }
    auto curTrackIdx = preMoveIndex.toInt();
    if (curTrackIdx == oldIndex)
    {
        curTrackIdx = newIndex;
    }
    else if (curTrackIdx == newIndex)
    {
        curTrackIdx = oldIndex;
    }
    auto idx = comboBox->findData(curTrackIdx);
    Q_ASSERT(idx != -1);
    comboBox->setCurrentIndex(idx);
}

void TsMuxerWindow::setUiMetaItemsData()
{
    // unfortunately, the .ui files don't allow the user to specify "item data" for combo boxes, which is the most
    // convenient way to associate some extra data that's not displayed in the UI in a combo box item without having
    // to resort to some kind of external containers which need to be kept synchronised.
    // as some of the combo boxes are taken as input for the meta file, it would end up having translated strings in it
    // if a non-English translation is active, and thus being invalid. item data for these UI items should always
    // contain the valid metafile tokens, as they are the actual things incorporated into the metafile content.
    ui->comboBoxAR->setItemData(0, QString());
    ui->comboBoxAR->setItemData(1, "1:1");
    ui->comboBoxAR->setItemData(2, "4:3");
    ui->comboBoxAR->setItemData(3, "16:9");
    ui->comboBoxAR->setItemData(4, "2.21:1");
    ui->comboBoxMeasure->setItemData(0, "KB");
    ui->comboBoxMeasure->setItemData(1, "KiB");
    ui->comboBoxMeasure->setItemData(2, "MB");
    ui->comboBoxMeasure->setItemData(3, "MiB");
    ui->comboBoxMeasure->setItemData(4, "GB");
    ui->comboBoxMeasure->setItemData(5, "GiB");
}

void TsMuxerWindow::onAudioSubtitlesParamsChanged()
{
    if (disableUpdatesCnt)
        return;
    QtvCodecInfo *codecInfo = getCurrentCodec();
    if (!codecInfo)
        return;
    codecInfo->bindFps = ui->checkBoxKeepFps->isChecked();
    codecInfo->dtsDownconvert = ui->dtsDwnConvert->isChecked();
    codecInfo->isSecondary = ui->secondaryCheckBox->isChecked();
    codecInfo->offsetId = ui->offsetsComboBox->currentIndex() - 1;
    QString addr = ui->langComboBox->itemData(ui->langComboBox->currentIndex()).toString();
    if (!addr.isEmpty())
    {
        codecInfo->lang = addr;
    }
    else
    {
        codecInfo->lang.clear();
    }
    ui->trackLV->item(ui->trackLV->currentRow(), 3)->setText(codecInfo->lang);
    colorizeCurrentRow(codecInfo);

    updateMetaLines();
}

void TsMuxerWindow::onEditDelayChanged(int)
{
    if (disableUpdatesCnt)
        return;
    QtvCodecInfo *codecInfo = getCurrentCodec();
    if (!codecInfo)
        return;
    codecInfo->delay = ui->editDelay->value();
    updateMetaLines();
}

void TsMuxerWindow::onPulldownCheckBoxChanged(int state)
{
    Q_UNUSED(state);
    if (disableUpdatesCnt)
        return;
    QtvCodecInfo *codecInfo = getCurrentCodec();
    if (!codecInfo)
        return;
    if (ui->checkBoxRemovePulldown->isEnabled())
    {
        if (ui->checkBoxRemovePulldown->isChecked())
        {
            codecInfo->delPulldown = 1;
            if (codecInfo->fpsTextOrig == "29.97")
            {
                ui->checkFPS->setChecked(true);
                ui->checkFPS->setEnabled(true);
                codecInfo->checkFPS = true;
                ui->comboBoxFPS->setEnabled(true);
                ui->comboBoxFPS->setCurrentIndex(3);
                codecInfo->fpsText = "24000/1001";
                setComboBoxText(ui->comboBoxFPS, "24000/1001");
            }
        }
        else
            codecInfo->delPulldown = 0;
    }
    else
        codecInfo->delPulldown = -1;
    updateMetaLines();
}

void TsMuxerWindow::addFiles(const QList<QUrl> &files)
{
    addFileList.clear();
    addFileList = files;
    addFile();
}

void TsMuxerWindow::onAddBtnClick()
{
    showAddFilesDialog(tr("Add media files"), [this]() { addFile(); });
}

void TsMuxerWindow::addFile()
{
    processAddFileList(&TsMuxerWindow::continueAddFile, &TsMuxerWindow::fileAdded, &TsMuxerWindow::addFile);
}

bool TsMuxerWindow::checkFileDuplicate(const QString &fileName)
{
    QString t = myUnquoteStr(fileName).trimmed();
    for (int i = 0; i < ui->inputFilesLV->count(); ++i)
        if (myUnquoteStr(ui->inputFilesLV->item(i)->data(FileNameRole).toString()) == t)
        {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("File already exists"));
            msgBox.setText(tr("File \"%1\" already exists").arg(fileName));
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();
            return false;
        }
    return true;
}

void TsMuxerWindow::setComboBoxText(QComboBox *comboBox, const QString &text)
{
    for (int k = 0; k < comboBox->count(); ++k)
    {
        if (comboBox->itemText(k) == text)
        {
            comboBox->setCurrentIndex(k);
            return;
        }
    }
    comboBox->addItem(text);
    comboBox->setCurrentIndex(comboBox->count() - 1);
}

void TsMuxerWindow::trackLVItemSelectionChanged()
{
    if (disableUpdatesCnt)
        return;
    while (ui->tabWidgetTracks->count()) ui->tabWidgetTracks->removeTab(0);
    if (ui->trackLV->currentRow() == -1)
    {
        ui->tabWidgetTracks->addTab(ui->tabSheetFake, TI_DEFAULT_TAB_NAME());
        return;
    }
    QtvCodecInfo *codecInfo = getCurrentCodec();
    if (!codecInfo)
        return;
    disableUpdatesCnt++;
    if (ui->trackLV->rowCount() >= 1)
    {
        if (isVideoCodec(codecInfo->displayName))
        {
            ui->tabWidgetTracks->addTab(ui->tabSheetVideo, TI_DEFAULT_TAB_NAME());

            ui->checkFPS->setChecked(codecInfo->checkFPS);
            ui->checkBoxLevel->setChecked(codecInfo->checkLevel);
            ui->comboBoxFPS->setEnabled(ui->checkFPS->isChecked());
            ui->comboBoxLevel->setEnabled(ui->checkBoxLevel->isChecked());
            ui->comboBoxSEI->setCurrentIndex(codecInfo->addSEIMethod);
            ui->checkBoxSecondaryVideo->setChecked(codecInfo->isSecondary);
            ui->checkBoxSPS->setChecked(codecInfo->addSPS);
            ui->checkBoxRemovePulldown->setChecked(codecInfo->delPulldown == 1);
            ui->checkBoxRemovePulldown->setEnabled(codecInfo->delPulldown >= 0);

            setComboBoxText(ui->comboBoxFPS, codecInfo->fpsText);
            if (!codecInfo->arText.isEmpty())
                setComboBoxText(ui->comboBoxAR, codecInfo->arText);
            else
                ui->comboBoxAR->setCurrentIndex(0);
            ui->checkBoxLevel->setEnabled(codecInfo->displayName == "H.264" || codecInfo->displayName == "MVC");
            if (ui->checkBoxLevel->isEnabled())
                setComboBoxText(ui->comboBoxLevel, codecInfo->levelText);
            else
                ui->comboBoxLevel->setCurrentIndex(0);
            ui->comboBoxSEI->setEnabled(ui->checkBoxLevel->isEnabled());
            ui->checkBoxSPS->setEnabled(ui->checkBoxLevel->isEnabled());
            ui->comboBoxAR->setEnabled(codecInfo->displayName == "MPEG-2");
            ui->comboBoxAR->setEnabled(true);
            ui->labelAR->setEnabled(ui->comboBoxAR->isEnabled());
            ui->checkBoxSecondaryVideo->setEnabled(codecInfo->displayName != "MVC");
        }
        else
        {
            ui->tabWidgetTracks->addTab(ui->tabSheetAudio, TI_DEFAULT_TAB_NAME());
            if (codecInfo->displayName == "LPCM")
                ui->tabWidgetTracks->addTab(ui->demuxLpcmOptions, TI_DEMUX_TAB_NAME());

            if (codecInfo->displayName == "DTS-HD")
                ui->dtsDwnConvert->setText(tr("Downconvert DTS-HD to DTS"));
            else if (codecInfo->displayName == "TRUE-HD")
                ui->dtsDwnConvert->setText(tr("Downconvert TRUE-HD to AC3"));
            else if (codecInfo->displayName == "E-AC3 (DD+)")
                ui->dtsDwnConvert->setText(tr("Downconvert E-AC3 to AC3"));
            else
                ui->dtsDwnConvert->setText(tr("Downconvert HD audio"));
            ui->dtsDwnConvert->setEnabled(codecInfo->displayName == "DTS-HD" || codecInfo->displayName == "TRUE-HD" ||
                                          codecInfo->displayName == "E-AC3 (DD+)");
            ui->secondaryCheckBox->setEnabled(codecInfo->descr.contains("(DTS Express)") ||
                                              codecInfo->descr.contains("(DTS Express 24bit)") ||
                                              codecInfo->displayName == "E-AC3 (DD+)");

            if (!ui->secondaryCheckBox->isEnabled())
                ui->secondaryCheckBox->setChecked(false);

            ui->langComboBox->setCurrentIndex(findLangByCode(codecInfo->lang));
            ui->offsetsComboBox->setCurrentIndex(codecInfo->offsetId + 1);
            ui->dtsDwnConvert->setVisible(codecInfo->displayName != "PGS" && codecInfo->displayName != "SRT");
            ui->secondaryCheckBox->setVisible(ui->dtsDwnConvert->isVisible());

            bool isPGS = codecInfo->displayName == "PGS";
            ui->checkBoxKeepFps->setVisible(isPGS);
            ui->offsetsLabel->setVisible(isPGS);
            ui->offsetsComboBox->setVisible(isPGS);

            ui->imageSubtitles->setVisible(!ui->dtsDwnConvert->isVisible());
            ui->imageAudio->setVisible(ui->dtsDwnConvert->isVisible());

            ui->editDelay->setValue(codecInfo->delay);
            ui->dtsDwnConvert->setChecked(codecInfo->dtsDownconvert);
            ui->secondaryCheckBox->setChecked(codecInfo->isSecondary);
            ui->checkBoxKeepFps->setChecked(codecInfo->bindFps);
            ui->editDelay->setEnabled(!ui->radioButtonDemux->isChecked());
        }
    }

    disableUpdatesCnt--;
    trackLVItemChanged(0);
}

void TsMuxerWindow::trackLVItemChanged(QTableWidgetItem *item)
{
    if (disableUpdatesCnt > 0)
        return;

    Q_UNUSED(item);
    updateMetaLines();
    ui->moveupBtn->setEnabled(ui->trackLV->currentItem() != 0);
    ui->movedownBtn->setEnabled(ui->trackLV->currentItem() != 0);
    ui->removeTrackBtn->setEnabled(ui->trackLV->currentItem() != 0);
    if (ui->trackLV->rowCount() == 0)
        oldFileName.clear();

    disableUpdatesCnt++;
    bool checkedExist = false;
    bool uncheckedExist = false;
    for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    {
        if (ui->trackLV->item(i, 0)->checkState() == Qt::Checked)
            checkedExist = true;
        else
            uncheckedExist = true;
    }
    if (checkedExist && uncheckedExist)
        m_header->setCheckState(Qt::PartiallyChecked);
    else if (checkedExist)
        m_header->setCheckState(Qt::Checked);
    else
        m_header->setCheckState(Qt::Unchecked);
    update();
    disableUpdatesCnt--;
}

void TsMuxerWindow::inputFilesLVChanged()
{
    QListWidgetItem *itm = ui->inputFilesLV->currentItem();
    if (!itm)
    {
        ui->btnAppend->setEnabled(false);
        ui->removeFile->setEnabled(false);
        return;
    }
    ui->btnAppend->setEnabled(itm->data(MplsItemRole).toInt() != MPLS_M2TS && ui->buttonMux->isEnabled());
    ui->removeFile->setEnabled(itm->data(MplsItemRole).toInt() != MPLS_M2TS);
}

void TsMuxerWindow::modifyOutFileName(const QString fileName)
{
    QFileInfo fi(unquoteStr(fileName));
    QString name = fi.completeBaseName();

    QString existingName = QDir::toNativeSeparators(ui->outFileName->text());
    QFileInfo fiDst(existingName);
    if (fiDst.completeBaseName() == "default")
    {
        QString dstPath;
        if (existingName.contains(QDir::separator()))
            dstPath = QDir::toNativeSeparators(fiDst.absolutePath());
        else
            dstPath = getOutputDir();

        if (ui->radioButtonTS->isChecked())
            ui->outFileName->setText(dstPath + QDir::separator() + name + ".ts");
        else if (ui->radioButtonM2TS->isChecked())
            ui->outFileName->setText(dstPath + QDir::separator() + name + ".m2ts");
        else if (ui->radioButtonBluRayISO->isChecked())
            ui->outFileName->setText(dstPath + QDir::separator() + name + ".iso");
        else
            ui->outFileName->setText(dstPath);
        if (ui->outFileName->text() == fileName)
            ui->outFileName->setText(dstPath + QDir::separator() + name + "_new." + fi.suffix());
    }
}

void TsMuxerWindow::continueAddFile()
{
    double fps;
    double level;
    disableUpdatesCnt++;
    bool firstWarn = true;
    int firstAddedIndex = -1;
    for (int i = 0; i < codecList.size(); ++i)
    {
        if (codecList[i].displayName == "PGS (depended view)")
            continue;

        QtvCodecInfo info = codecList[i];
        if (info.displayName.isEmpty())
        {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Unsupported format"));
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setStandardButtons(QMessageBox::Ok);
            if (codecList.size() == 0)
            {
                msgBox.setText(
                    tr("Unsupported format or all tracks are not recognized. File name: \"%1\"").arg(newFileName));
                msgBox.exec();
                disableUpdatesCnt--;
                return;
            }
            else
            {
                if (firstWarn)
                {
                    msgBox.setText(
                        tr("Track %1 was not recognized and ignored. File name: \"%2\"").arg(i).arg(newFileName));
                    msgBox.exec();
                    firstWarn = false;
                }
                continue;
            }
        }
        if (mplsFileList.isEmpty())
            info.fileList << newFileName;
        else
        {
            info.fileList << mplsFileList[0].name;
            QFileInfo fileInfo(unquoteStr(newFileName));
            // info.mplsFile = fileInfo.baseName();
            info.mplsFiles << unquoteStr(newFileName);
        }
        if (info.descr.indexOf("not found") >= 0)
        {
            fps = 23.976;
            info.checkFPS = true;
        }
        else
            fps = extractFloatFromDescr(info.descr, "Frame rate: ");
        info.width = extractFloatFromDescr(info.descr, "Resolution: ") + 0.5;
        info.height = extractFloatFromDescr(info.descr, QString::number(info.width) + ":") + 0.5;
        if (doubleCompare(fps, 23.976))
            info.fpsText = "24000/1001";
        else if (doubleCompare(fps, 29.97))
            info.fpsText = "30000/1001";
        else if (doubleCompare(fps, 59.94))
            info.fpsText = "60000/1001";
        else
            info.fpsText = QString::number(fps);
        info.fpsTextOrig = QString::number(fps);
        level = extractFloatFromDescr(info.descr, "@");
        info.levelText = QString::number(level);
        if (info.descr.indexOf("pulldown") >= 0)
            info.delPulldown = 0;

        info.maxPgOffsets = extractFloatFromDescr(info.descr, "3d-pg-planes: ");
        if (info.maxPgOffsets > 0)
            m_3dMode = true;

        if (info.descr.contains("3d-plane: zero"))
            info.offsetId = -1;
        else if (info.descr.contains("3d-plane: "))
            info.offsetId = extractFloatFromDescr(info.descr, "3d-plane: ");
        else if (m_3dMode)
            info.offsetId = 0;

        auto newTrackRowIdx = ui->trackLV->rowCount();
        ui->trackLV->setRowCount(newTrackRowIdx + 1);
        ui->trackLV->setRowHeight(newTrackRowIdx, 18);
        QTableWidgetItem *item = new QTableWidgetItem("");
        item->setCheckState(info.enabledByDefault ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, reinterpret_cast<qlonglong>(new QtvCodecInfo(info)));
        ui->trackLV->setCurrentItem(item);

        ui->trackLV->setItem(newTrackRowIdx, 0, item);
        item = new QTableWidgetItem(newFileName);
        item->setFlags(item->flags() & (~Qt::ItemIsEditable));
        ui->trackLV->setItem(newTrackRowIdx, 1, item);
        item = new QTableWidgetItem(info.displayName);
        item->setFlags(item->flags() & (~Qt::ItemIsEditable));
        ui->trackLV->setItem(newTrackRowIdx, 2, item);
        item = new QTableWidgetItem(info.lang);
        item->setFlags(item->flags() & (~Qt::ItemIsEditable));
        ui->trackLV->setItem(newTrackRowIdx, 3, item);
        item = new QTableWidgetItem(info.descr);
        item->setFlags(item->flags() & (~Qt::ItemIsEditable));
        ui->trackLV->setItem(newTrackRowIdx, 4, item);
        if (firstAddedIndex == -1)
            firstAddedIndex = newTrackRowIdx;
        colorizeCurrentRow(&info, newTrackRowIdx);
        addTrackToDefaultComboBox(newTrackRowIdx);
    }
    if (firstAddedIndex >= 0)
    {
        ui->trackLV->setRangeSelected(QTableWidgetSelectionRange(firstAddedIndex, 0, ui->trackLV->rowCount() - 1, 4),
                                      true);
        ui->trackLV->setCurrentCell(firstAddedIndex, 0);
    }

    QString displayName = newFileName;
    if (fileDuration > 0)
        displayName += QString(" (%1)").arg(floatToTime(fileDuration));
    ui->inputFilesLV->addItem(displayName);

    int index = ui->inputFilesLV->count() - 1;
    QListWidgetItem *fileItem = ui->inputFilesLV->item(index);
    if (!mplsFileList.empty())
        fileItem->setData(MplsItemRole, MPLS_PRIMARY);
    fileItem->setData(FileNameRole, newFileName);
    QVariant v;
    v.setValue<ChapterList>(chapters);
    fileItem->setData(ChaptersRole, v);
    fileItem->setData(FileDurationRole, fileDuration);
    chapters.clear();
    fileDuration = 0.0;

    ui->inputFilesLV->setCurrentItem(fileItem);
    if (mplsFileList.size() > 0)
        doAppendInt(mplsFileList[0].name, newFileName, mplsFileList[0].duration, false, MPLS_M2TS);
    for (int mplsCnt = 1; mplsCnt < mplsFileList.size(); ++mplsCnt)
        doAppendInt(mplsFileList[mplsCnt].name, mplsFileList[mplsCnt - 1].name, mplsFileList[mplsCnt].duration, false,
                    MPLS_M2TS);
    ui->inputFilesLV->setCurrentItem(ui->inputFilesLV->item(index));
    updateMetaLines();
    ui->moveupBtn->setEnabled(ui->trackLV->currentRow() >= 0);
    ui->movedownBtn->setEnabled(ui->trackLV->currentRow() >= 0);
    ui->removeTrackBtn->setEnabled(ui->trackLV->currentRow() >= 0);
    if (!outFileNameModified)
    {
        modifyOutFileName(newFileName);
        outFileNameModified = true;
    }
    updateMaxOffsets();
    updateCustomChapters();
    disableUpdatesCnt--;
    trackLVItemSelectionChanged();
    emit fileAdded();
}

void TsMuxerWindow::updateCustomChapters()
{
    QSet<qint64> chaptersSet;
    double prevDuration = 0.0;
    double offset = 0.0;
    for (int i = 0; i < ui->inputFilesLV->count(); ++i)
    {
        QListWidgetItem *item = ui->inputFilesLV->item(i);
        if (item->data(MplsItemRole).toInt() == MPLS_M2TS)
            continue;
        if (item->text().left(4) == FILE_JOIN_PREFIX)
            offset += prevDuration;
        else
            offset = 0;

        ChapterList chapters = item->data(ChaptersRole).value<ChapterList>();
        foreach (double chapter, chapters)
            chaptersSet << qint64((chapter + offset) * 1000000);
        prevDuration = item->data(FileDurationRole).toDouble();
    }

    ui->memoChapters->clear();
    QList<qint64> mergedChapterList = chaptersSet.toList();
    std::sort(std::begin(mergedChapterList), std::end(mergedChapterList));
    for (auto chapter : mergedChapterList)
        ui->memoChapters->insertPlainText(floatToTime(chapter / 1000000.0) + QString('\n'));
}

void splitLines(const QString &str, QList<QString> &strList)
{
    strList = str.split('\n');
    for (int i = 0; i < strList.size(); ++i)
    {
        if (!strList[i].isEmpty() && strList[i].at(0) == '\r')
            strList[i] = strList[i].mid(1);
        else if (!strList[i].isEmpty() && strList[i].at(strList[i].length() - 1) == '\r')
            strList[i] = strList[i].mid(0, strList[i].length() - 1);
    }
}

void TsMuxerWindow::addLines(const QByteArray &arr, QList<QString> &outList, bool isError)
{
    QString str = QString::fromUtf8(arr);
    QList<QString> strList;
    splitLines(str, strList);
    QString text;
    for (int i = 0; i < strList.size(); ++i)
    {
        if (strList[i].trimmed().isEmpty())
            continue;
        int p = strList[i].indexOf("% complete");
        if (p >= 0)
        {
            int numStartPos = 0;
            for (int j = 0; j < strList[i].length(); ++j)
            {
                if (strList[i].at(j) >= '0' && strList[i].at(j) <= '9')
                {
                    numStartPos = j;
                    break;
                }
            }
            QString progress = strList[i].mid(numStartPos, p - numStartPos);
            float tmpVal = progress.toFloat();
            if (qAbs(tmpVal) > 0 && runInMuxMode)
                muxForm->setProgress(int(double(tmpVal * 10) + 0.5));  // todo: uncomment here
        }
        else
        {
            if (runInMuxMode)
            {
                if (!text.isEmpty())
                    text += '\n';
                text += strList[i];
            }
            else
                outList << strList[i];
        }
    }
    if (runInMuxMode && !text.isEmpty())
    {
        if (isError)
            muxForm->addStdErrLine(text);
        else
            muxForm->addStdOutLine(text);
    }
}

void TsMuxerWindow::readFromStdout() { addLines(proc.readAllStandardOutput(), procStdOutput, false); }

void TsMuxerWindow::readFromStderr()
{
    QByteArray data(proc.readAllStandardError());
    addLines(data, procErrOutput, true);
}

void TsMuxerWindow::myPlaySound(const QString &fileName)
{
    sound.setSource(QUrl(QString("qrc%1").arg(fileName)));
    sound.play();
}

void TsMuxerWindow::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    processFinished = true;
    if (!metaName.isEmpty())
    {
        QFile::remove(metaName);
        metaName.clear();
        if (ui->checkBoxSound->isChecked())
        {
            if (exitCode == 0)
                myPlaySound(":/sounds/success.wav");
            else
                myPlaySound(":/sounds/fail.wav");
        }
    }
    processExitCode = exitCode;
    processExitStatus = exitStatus;
    muxForm->muxFinished(processExitCode, ui->radioButtonDemux->isChecked() ? tr("Demux") : tr("Mux"));
    ui->buttonMux->setEnabled(true);
    ui->addBtn->setEnabled(true);
    inputFilesLVChanged();
    if (processExitCode == 0)
        emit tsMuxerSuccessFinished();
}

void TsMuxerWindow::onProcessError(QProcess::ProcessError error)
{
    processExitCode = -1;
    if (!metaName.isEmpty())
    {
        QFile::remove(metaName);
        metaName.clear();
    }
    processFinished = true;
    QMessageBox msgBox(this);
    QString text;
    msgBox.setWindowTitle(tr("tsMuxeR error"));
    switch (error)
    {
    case QProcess::FailedToStart:
        msgBox.setText(tr("tsMuxeR not found!"));
        ui->buttonMux->setEnabled(true);
        ui->addBtn->setEnabled(true);
        inputFilesLVChanged();
        break;
    case QProcess::Crashed:
        // process killed
        if (runInMuxMode)
            return;
        for (int i = 0; i < procErrOutput.size(); ++i)
        {
            if (i > 0)
                text += '\n';
            text += procErrOutput[i];
        }
        msgBox.setText(text);
        break;
    default:
        msgBox.setText(tr("Can't execute tsMuxeR!"));
        break;
    }
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

static QString getTsMuxerBinaryPath()
{
    const auto applicationDirPath =
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) + QDir::separator();
    for (auto binaryName : {"tsmuxer", "tsMuxeR"})
    {
        auto binaryPath = QStandardPaths::findExecutable(binaryName, {applicationDirPath});
        if (!binaryPath.isEmpty())
        {
            return binaryPath;
        }
    }
    return QString();
}

void TsMuxerWindow::tsMuxerExecute(const QStringList &args)
{
    const auto exePath = getTsMuxerBinaryPath();
    ui->buttonMux->setEnabled(false);
    procStdOutput.clear();
    procErrOutput.clear();
    processFinished = false;
    processExitCode = -1;
    proc.start(exePath, args);
    if (muxForm->isVisible())
        muxForm->setProcess(&proc);
}

void TsMuxerWindow::doAppendInt(const QString &fileName, const QString &parentFileName, double duration,
                                bool doublePrefix, MplsType mplsRole)
{
    QString itemName = FILE_JOIN_PREFIX;
    if (doublePrefix)
        itemName += FILE_JOIN_PREFIX;
    itemName += fileName;
    if (duration > 0)
        itemName += QString(" ( %1)").arg(floatToTime(duration));
    QListWidgetItem *item = new QListWidgetItem(itemName);
    ui->inputFilesLV->insertItem(ui->inputFilesLV->currentRow() + 1, item);
    item->setData(MplsItemRole, mplsRole);
    item->setData(FileNameRole, fileName);
    if (duration > 0)
        item->setData(FileDurationRole, duration);
    QVariant v;
    v.setValue<ChapterList>(chapters);
    item->setData(ChaptersRole, v);

    ui->inputFilesLV->setCurrentItem(item);

    for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    {
        auto info = getCodecInfo(i);
        if (!info)
            continue;
        if (mplsRole == MPLS_PRIMARY)
        {
            for (int j = 0; j < info->mplsFiles.size(); ++j)
            {
                if (info->mplsFiles[j] == parentFileName)
                {
                    info->mplsFiles.insert(j + 1, fileName);
                    break;
                }
            }
        }
        else
        {
            for (int j = 0; j < info->fileList.size(); ++j)
            {
                if (info->fileList[j] == parentFileName)
                {
                    info->fileList.insert(j + 1, fileName);
                    break;
                }
            }
        }
    }
}

bool TsMuxerWindow::isVideoCropped()
{
    for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    {
        auto codecInfo = getCodecInfo(i);
        if (!codecInfo)
            continue;

        if (isVideoCodec(codecInfo->displayName))
        {
            if (codecInfo->height < 1080 && codecInfo->height != 720 && codecInfo->height != 576 &&
                codecInfo->height != 480)
                return true;
        }
    }
    return false;
}

bool TsMuxerWindow::isDiskOutput() const
{
    return ui->radioButtonAVCHD->isChecked() || ui->radioButtonBluRay->isChecked() ||
           ui->radioButtonBluRayISO->isChecked();
}

QString TsMuxerWindow::getMuxOpts()
{
    QString rez = "MUXOPT --no-pcr-on-video-pid ";
    if (ui->checkBoxNewAudioPes->isChecked())
        rez += "--new-audio-pes --hdmv-descriptors ";
    if (ui->radioButtonBluRay->isChecked())
        rez += (ui->checkBoxV3->isChecked() ? "--blu-ray-v3 " : "--blu-ray ");
    else if (ui->radioButtonBluRayISO->isChecked())
    {
        rez += (ui->checkBoxV3->isChecked() ? "--blu-ray-v3 " : "--blu-ray ");
        if (!ui->DiskLabelEdit->text().isEmpty())
            rez += QString("--label=\"%1\" ").arg(ui->DiskLabelEdit->text());
    }
    else if (ui->radioButtonAVCHD->isChecked())
        rez += "--avchd ";
    else if (ui->radioButtonDemux->isChecked())
        rez += "--demux ";
    if (ui->checkBoxCBR->isChecked())
        rez += "--cbr --bitrate=" + QString::number(ui->editCBRBitrate->value(), 'f', 3);
    else
    {
        rez += "--vbr ";
        if (ui->checkBoxRVBR->isChecked())
        {
            rez += QString("--minbitrate=") + QString::number(ui->editMinBitrate->value(), 'f', 3);
            rez += QString(" --maxbitrate=") + QString::number(ui->editMaxBitrate->value(), 'f', 3);
        }
    }
    // if (!ui->checkBoxuseAsynIO->isChecked())
    //	rez += " --no-asyncio";
    if (isDiskOutput())
    {
        if (ui->checkBoxBlankPL->isChecked() && isVideoCropped())
        {
            rez += " --insertBlankPL";
            if (ui->BlackplaylistCombo->value() != 1900)
                rez += QString(" --blankOffset=") + QString::number(ui->BlackplaylistCombo->value());
        }
        if (ui->spinBoxMplsNum->value() > 0)
            rez += " --mplsOffset=" + QString::number(ui->spinBoxMplsNum->value());
        if (ui->spinBoxM2tsNum->value() > 0)
            rez += " --m2tsOffset=" + QString::number(ui->spinBoxM2tsNum->value());
    }
    if (isDiskOutput())
    {
        if (ui->radioButtonAutoChapter->isChecked())
            rez += " --auto-chapters=" + QString::number(ui->spinEditChapterLen->value());
        if (ui->radioButtonCustomChapters->isChecked())
        {
            QString custChapStr;
            QList<QString> lines;
            splitLines(ui->memoChapters->toPlainText(), lines);
            for (int i = 0; i < lines.size(); ++i)
            {
                QString tmpStr = lines[i].trimmed();
                if (!tmpStr.isEmpty())
                {
                    if (!custChapStr.isEmpty())
                        custChapStr += ';';
                    custChapStr += tmpStr;
                }
            }
            rez += QString(" --custom-chapters=") + custChapStr;
        }
    }
    if (ui->splitByDuration->isChecked())
        rez += QString(" --split-duration=") + ui->spinEditSplitDuration->text();
    if (ui->splitBySize->isChecked())
        rez += QString(" --split-size=") + QString::number(ui->editSplitSize->value(), 'f', 3) +
               ui->comboBoxMeasure->currentData().toString();

    int startCut = qTimeToMsec(ui->cutStartTimeEdit->time());
    int endCut = qTimeToMsec(ui->cutEndTimeEdit->time());
    if (startCut > 0 && ui->checkBoxCut->isChecked())
        rez += QString(" --cut-start=%1ms").arg(startCut);
    if (endCut > 0 && ui->checkBoxCut->isChecked())
        rez += QString(" --cut-end=%1ms").arg(endCut);

    int vbvLen = ui->editVBVLen->value();
    if (vbvLen > 0)
        rez += " --vbv-len=" + QString::number(vbvLen);

    if (isDiskOutput() && ui->rightEyeCheckBox->isChecked())
        rez += " --right-eye";
    /*
    QString muxTimeStr = ui->muxTimeEdit->time().toString("hh:mm:ss.zzz");
    if (muxTimeStr != "00:00:00.000")
        rez += " --start-time=" + muxTimeStr;
    */
    if (ui->muxTimeClock->value())
        rez += QString(" --start-time=%1").arg(ui->muxTimeClock->value());
    return rez;
}

int getCharsetCode(const QString &name)
{
    Q_UNUSED(name);
    return 0;  // todo: refactor this function
}

double TsMuxerWindow::getRendererAnimationTime() const
{
    switch (ui->comboBoxAnimation->currentIndex())
    {
    case 1:
        return 0.1;  // fast
        break;
    case 2:
        return 0.25;  // medium
        break;
    case 3:
        return 0.5;  // slow
        break;
    case 4:
        return 1.0;  // very slow
    }
    return 0.0;
}

void TsMuxerWindow::setRendererAnimationTime(double value)
{
    int index = 0;
    if (doubleCompare(value, 0.1))
        index = 1;
    else if (doubleCompare(value, 0.25))
        index = 2;
    else if (doubleCompare(value, 0.5))
        index = 3;
    else if (doubleCompare(value, 1.0))
        index = 4;

    ui->comboBoxAnimation->setCurrentIndex(index);
}

QString TsMuxerWindow::getSrtParams()
{
    QString rez;
    if (ui->listViewFont->rowCount() < 5)
        return rez;
    rez = QString(",font-name=\"") + ui->listViewFont->item(0, 1)->text();
    rez += QString("\",font-size=") + ui->listViewFont->item(1, 1)->text();
    rez += QString(",font-color=") + ui->listViewFont->item(2, 1)->text();
    int charsetCode = getCharsetCode(ui->listViewFont->item(3, 1)->text());
    if (charsetCode)
        rez += QString(",font-charset=") + QString::number(charsetCode);
    if (ui->lineSpacing->value() != 1.0)
        rez += ",line-spacing=" + QString::number(ui->lineSpacing->value());

    if (ui->listViewFont->item(4, 1)->text().indexOf("Italic") >= 0)
        rez += ",font-italic";
    if (ui->listViewFont->item(4, 1)->text().indexOf("Bold") >= 0)
        rez += ",font-bold";
    if (ui->listViewFont->item(4, 1)->text().indexOf("Underline") >= 0)
        rez += ",font-underline";
    if (ui->listViewFont->item(4, 1)->text().indexOf("Strikeout") >= 0)
        rez += ",font-strikeout";
    rez += QString(",bottom-offset=") + QString::number(ui->spinEditOffset->value()) +
           ",font-border=" + QString::number(ui->spinEditBorder->value());
    if (ui->rbhLeft->isChecked())
        rez += ",text-align=left";
    else if (ui->rbhRight->isChecked())
        rez += ",text-align=right";
    else
        rez += ",text-align=center";

    double animationTime = getRendererAnimationTime();
    if (animationTime > 0.0)
        rez += QString(",fadein-time=%1,fadeout-time=%1").arg(animationTime);

    for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    {
        if (ui->trackLV->item(i, 0)->checkState() == Qt::Checked)
        {
            auto codecInfo = getCodecInfo(i);
            if (!codecInfo)
                continue;

            if (isVideoCodec(codecInfo->displayName))
            {
                rez += QString(",video-width=") + QString::number(codecInfo->width);
                rez += QString(",video-height=") + QString::number(codecInfo->height);
                rez += QString(",fps=") + fpsTextToFpsStr(codecInfo->fpsText);
                return rez;
            }
        }
    }
    rez += ",video-width=1920,video-height=1080,fps=23.976";
    return rez;
}

QString TsMuxerWindow::getFileList(QtvCodecInfo *codecInfo)
{
    QString rezStr;

    if (codecInfo->mplsFiles.isEmpty())
    {
        for (int i = 0; i < codecInfo->fileList.size(); ++i)
        {
            if (i > 0)
                rezStr += '+';
            rezStr += QString("\"") + codecInfo->fileList[i] + "\"";
        }
    }
    else
    {
        for (int i = 0; i < codecInfo->mplsFiles.size(); ++i)
        {
            if (i > 0)
                rezStr += '+';
            rezStr += QString("\"") + codecInfo->mplsFiles[i] + "\"";
        }
    }

    return rezStr;
}

QString cornerToStr(int corner)
{
    if (corner == 0)
        return "topLeft";
    else if (corner == 1)
        return "topRight";
    else if (corner == 2)
        return "bottomRight";
    else
        return "bottomLeft";
}

QString toPipScaleStr(int scaleIdx)
{
    if (scaleIdx == 0)
        return "1";
    if (scaleIdx == 1)
        return "1/2";
    else if (scaleIdx == 2)
        return "1/4";
    else if (scaleIdx == 3)
        return "1.5";
    else
        return "fullScreen";
}

QString TsMuxerWindow::getVideoMetaInfo(QtvCodecInfo *codecInfo)
{
    QString fpsStr;
    QString levelStr;
    QString rezStr = codecInfo->programName + ", ";

    rezStr += getFileList(codecInfo);

    if (codecInfo->checkFPS)
        fpsStr = fpsTextToFpsStr(codecInfo->fpsText);
    if (codecInfo->checkLevel)
        levelStr = QString::number(codecInfo->levelText.toDouble(), 'f', 1);

    if (ui->checkBoxCrop->isChecked() && ui->checkBoxCrop->isEnabled())
        rezStr += QString(", ") + "restoreCrop";
    if (!fpsStr.isEmpty())
        rezStr += QString(", ") + "fps=" + fpsStr;
    if (!levelStr.isEmpty())
        rezStr += QString(", ") + "level=" + levelStr;
    if (codecInfo->addSEIMethod == 1)
        rezStr += QString(", ") + "insertSEI";
    else if (codecInfo->addSEIMethod == 2)
        rezStr += QString(", ") + "forceSEI";
    if (codecInfo->addSPS)
        rezStr += QString(", ") + "contSPS";
    if (codecInfo->delPulldown == 1)
        rezStr += QString(", ") + "delPulldown";
    if (!codecInfo->arText.isEmpty())
        rezStr += QString(", ") + "ar=" + codecInfo->arText;

    if (codecInfo->isSecondary)
    {
        rezStr += QString(", secondary");
        rezStr += QString(", pipCorner=%1").arg(cornerToStr(ui->comboBoxPipCorner->currentIndex()));
        rezStr += QString(" ,pipHOffset=%1").arg(ui->spinBoxPipOffsetH->value());
        rezStr += QString(" ,pipVOffset=%1").arg(ui->spinBoxPipOffsetV->value());
        rezStr += QString(", pipScale=%1").arg(toPipScaleStr(ui->comboBoxPipSize->currentIndex()));
        rezStr += QString(", pipLumma=%1").arg(ui->pipTransparencySpinBox->value());
    }

    return rezStr;
}

QString TsMuxerWindow::getAudioMetaInfo(QtvCodecInfo *codecInfo)
{
    QString rezStr = codecInfo->programName + ", ";
    rezStr += getFileList(codecInfo);
    if (codecInfo->delay != 0)
        rezStr += QString(", timeshift=") + QString::number(codecInfo->delay) + "ms";
    if (codecInfo->dtsDownconvert && codecInfo->programName == "A_DTS")
        rezStr += ", down-to-dts";
    else if (codecInfo->dtsDownconvert && codecInfo->programName == "A_AC3")
        rezStr += ", down-to-ac3";
    if (codecInfo->isSecondary)
        rezStr += ", secondary";
    return rezStr;
}

void TsMuxerWindow::updateMuxTime1()
{
    QTime t = ui->muxTimeEdit->time();
    int clock = (t.hour() * 3600 + t.minute() * 60 + t.second()) * 45000ll + t.msec() * 45ll;
    ui->muxTimeClock->blockSignals(true);
    ui->muxTimeClock->setValue(clock);
    ui->muxTimeClock->blockSignals(false);
    updateMetaLines();
}

void TsMuxerWindow::updateMuxTime2()
{
    double timeF = ui->muxTimeClock->value() / 45000.0;
    ui->muxTimeEdit->blockSignals(true);
    ui->muxTimeEdit->setTime(qTimeFromFloat(timeF));
    ui->muxTimeEdit->blockSignals(false);
    updateMetaLines();
}

void TsMuxerWindow::onLanguageComboBoxIndexChanged(int idx)
{
    auto lang = ui->languageSelectComboBox->itemData(idx).toString();
    qtCoreTranslator.load(QString("qtbase_%1").arg(lang), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    tsMuxerTranslator.load(QString("tsmuxergui_%1").arg(lang), ":/i18n");
    QFile aboutContent(QString(":/about_%1.html").arg(lang));
    if (aboutContent.open(QIODevice::ReadOnly))
    {
        ui->textEdit->setHtml(QString::fromUtf8(aboutContent.readAll()));
    }
    else
    {
        qWarning() << "Failed to open about.html for language" << lang << aboutContent.errorString();
        ui->textEdit->clear();
    }
    writeSettings();
}

void TsMuxerWindow::updateMetaLines()
{
    if (!m_updateMeta || disableUpdatesCnt > 0)
        return;

    ui->memoMeta->clear();
    QString metaContent;
    metaContent.append(getMuxOpts() + '\n');
    QString tmpFps;
    for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    {
        auto codecInfo = getCodecInfo(i);
        if (!codecInfo)
            continue;

        if (isVideoCodec(codecInfo->displayName))
        {
            if (codecInfo->checkFPS)
                tmpFps = fpsTextToFpsStr(codecInfo->fpsText);
            else
                tmpFps = codecInfo->fpsTextOrig;
            break;
        }
    }

    QString prefix;
    QString postfix;

    bool bluray3D = isDiskOutput() && m_3dMode;

    for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    {
        if (ui->trackLV->item(i, 0)->checkState() == Qt::Checked)
            prefix = "";
        else
            prefix = "#";
        auto codecInfo = getCodecInfo(i);
        if (!codecInfo)
            continue;

        postfix.clear();
        if (codecInfo->programName.startsWith('S'))
        {
            if (isDiskOutput() && ui->defaultSubTrackCheckBox->isChecked() &&
                ui->defaultSubTrackComboBox->currentData().toInt() == i)
            {
                postfix +=
                    QString(", default=") + (ui->defaultSubTrackForcedOnlyCheckBox->isChecked() ? "forced" : "all");
            }
        }
        if (codecInfo->displayName == "PGS")
        {
            if (codecInfo->bindFps && !tmpFps.isEmpty())
                postfix += QString(", fps=") + tmpFps;
            if (bluray3D && codecInfo->offsetId >= 0)
                postfix += QString(", 3d-plane=%1").arg(codecInfo->offsetId);
        }
        if (codecInfo->displayName == "SRT")
        {
            postfix += getSrtParams();
            if (bluray3D && codecInfo->offsetId >= 0)
                postfix += QString(", offset=%1").arg(codecInfo->offsetId);
        }
        if (codecInfo->trackID != 0)
            postfix += QString(", track=") + QString::number(codecInfo->trackID);
        if (!codecInfo->lang.isEmpty())
            postfix += QString(", lang=") + codecInfo->lang;
        // if (!codecInfo->mplsFile.isEmpty())
        //	postfix += QString(", mplsFile=") + codecInfo->mplsFile;
        if (codecInfo->subTrack != 0)
            postfix += QString(", subTrack=") + QString::number(codecInfo->subTrack);
        if (isVideoCodec(codecInfo->displayName))
            metaContent.append(prefix + getVideoMetaInfo(codecInfo) + postfix + '\n');
        else
        {
            if (isDiskOutput() && ui->defaultAudioTrackCheckBox->isChecked() &&
                ui->defaultAudioTrackComboBox->currentData().toInt() == i && codecInfo->programName.startsWith('A'))
            {
                postfix += QString(", default");
            }
            metaContent.append(prefix + getAudioMetaInfo(codecInfo) + postfix + '\n');
        }
    }
    ui->memoMeta->setPlainText(metaContent);
}

void TsMuxerWindow::onFontBtnClicked()
{
    bool ok;
    QFont font;
    font.setFamily(ui->listViewFont->item(0, 1)->text());
    font.setPointSize((ui->listViewFont->item(1, 1)->text()).toInt());
    font.setItalic(ui->listViewFont->item(4, 1)->text().indexOf("Italic") >= 0);
    font.setBold(ui->listViewFont->item(4, 1)->text().indexOf("Bold") >= 0);
    font.setUnderline(ui->listViewFont->item(4, 1)->text().indexOf("Underline") >= 0);
    font.setStrikeOut(ui->listViewFont->item(4, 1)->text().indexOf("Strikeout") >= 0);
    font = QFontDialog::getFont(&ok, font, this);
    if (ok)
    {
        // FT_Face fontFace = fontfreetypeFace();
        ui->listViewFont->item(0, 1)->setText(font.family());
        ui->listViewFont->item(1, 1)->setText(QString::number(font.pointSize()));
        QString optStr;
        if (font.italic())
            optStr += "Italic";
        if (font.bold())
        {
            if (!optStr.isEmpty())
                optStr += ',';
            optStr += "Bold";
        }
        if (font.underline())
        {
            if (!optStr.isEmpty())
                optStr += ',';
            optStr += "Underline";
        }
        if (font.strikeOut())
        {
            if (!optStr.isEmpty())
                optStr += ',';
            optStr += "Strikeout";
        }
        ui->listViewFont->item(4, 1)->setText(optStr);
        writeSettings();
        updateMetaLines();
    }
}

quint32 bswap(quint32 val)
{
    return val;  // ntohl(val);
}

int colorLight(QColor color) { return (0.257 * color.red()) + (0.504 * color.green()) + (0.098 * color.blue()) + 16; }

void TsMuxerWindow::setTextItemColor(QString str)
{
    while (str.length() < 8) str = '0' + str;
    QColor color = bswap(str.toLongLong(0, 16));
    QTableWidgetItem *item = ui->listViewFont->item(2, 1);
    item->setBackground(QBrush(color));
    if (colorLight(color) < 128)
        item->setForeground(QBrush(QColor(255, 255, 255, 255)));
    else
        item->setForeground(QBrush(QColor(0, 0, 0, 255)));
    item->setText(QString("0x") + str);
}

void TsMuxerWindow::onColorBtnClicked()
{
    QColor color = bswap(ui->listViewFont->item(2, 1)->text().toLongLong(0, 16));
    color = QColorDialog::getColor(color, this);
    QString str = QString::number(bswap(color.rgba()), 16);
    setTextItemColor(str);

    writeSettings();
    updateMetaLines();
}

void TsMuxerWindow::onGeneralCheckboxClicked()
{
    ui->editMaxBitrate->setEnabled(ui->checkBoxRVBR->isChecked());
    ui->editMinBitrate->setEnabled(ui->checkBoxRVBR->isChecked());
    ui->editCBRBitrate->setEnabled(ui->checkBoxCBR->isChecked());
    ui->BlackplaylistCombo->setEnabled(ui->checkBoxBlankPL->isChecked());
    ui->BlackplaylistLabel->setEnabled(ui->checkBoxBlankPL->isChecked());
    updateMetaLines();
}

void TsMuxerWindow::onGeneralSpinboxValueChanged() { updateMetaLines(); }

void TsMuxerWindow::onChapterParamsChanged()
{
    ui->memoChapters->setEnabled(ui->radioButtonCustomChapters->isChecked());
    ui->spinEditChapterLen->setEnabled(ui->radioButtonAutoChapter->isChecked());
    updateMetaLines();
}

void TsMuxerWindow::onSplitCutParamsChanged()
{
    ui->spinEditSplitDuration->setEnabled(ui->splitByDuration->isChecked());
    ui->labelSplitByDur->setEnabled(ui->splitByDuration->isChecked());
    ui->editSplitSize->setEnabled(ui->splitBySize->isChecked());
    ui->comboBoxMeasure->setEnabled(ui->splitBySize->isChecked());

    ui->cutStartTimeEdit->setEnabled(ui->checkBoxCut->isChecked());
    ui->cutEndTimeEdit->setEnabled(ui->checkBoxCut->isChecked());
    updateMetaLines();
}

void TsMuxerWindow::onSavedParamChanged()
{
    writeSettings();
    updateMetaLines();
}

void TsMuxerWindow::onFontParamsChanged() { updateMetaLines(); }

void TsMuxerWindow::onRemoveBtnClick()
{
    if (!ui->inputFilesLV->currentItem())
        return;
    int idx = ui->inputFilesLV->currentRow();
    bool delMplsM2ts = false;

    if (idx < ui->inputFilesLV->count() - 1)
    {
        if (ui->inputFilesLV->currentItem()->data(MplsItemRole).toInt() == MPLS_PRIMARY)
            delMplsM2ts = true;
        else if (ui->inputFilesLV->currentItem()->text().left(4) != FILE_JOIN_PREFIX)
        {
            QString text = ui->inputFilesLV->item(idx + 1)->text();
            if (text.length() >= 4 && text.left(4) == FILE_JOIN_PREFIX)
                ui->inputFilesLV->item(idx + 1)->setText(text.mid(4));
        }
    }

    delTracksByFileName(myUnquoteStr(ui->inputFilesLV->currentItem()->data(FileNameRole).toString()));
    ui->inputFilesLV->takeItem(idx);
    if (idx >= ui->inputFilesLV->count())
        idx--;
    if (delMplsM2ts)
    {
        while (idx < ui->inputFilesLV->count())
        {
            QString text = ui->inputFilesLV->item(idx)->text();
            if (text.length() >= 4 && text.left(4) == FILE_JOIN_PREFIX)
            {
                delTracksByFileName(myUnquoteStr(ui->inputFilesLV->item(idx)->data(FileNameRole).toString()));
                ui->inputFilesLV->takeItem(idx);
            }
            else
                break;
        }
    }
    if (ui->inputFilesLV->count() > 0)
        ui->inputFilesLV->setCurrentRow(idx);
    updateCustomChapters();
}

void TsMuxerWindow::delTracksByFileName(const QString &fileName)
{
    for (int i = ui->trackLV->rowCount() - 1; i >= 0; --i)
    {
        if (auto info = getCodecInfo(i))
        {
            for (int j = info->fileList.count() - 1; j >= 0; --j)
            {
                if (info->fileList[j] == fileName)
                {
                    info->fileList.removeAt(j);
                    break;
                }
            }
            for (int j = info->mplsFiles.count() - 1; j >= 0; --j)
            {
                if (info->mplsFiles[j] == fileName)
                {
                    info->mplsFiles.removeAt(j);
                    break;
                }
            }
            if (info->fileList.count() == 0)
                deleteTrack(i);
        }
    }
    updateMaxOffsets();
    updateMetaLines();
}

void TsMuxerWindow::deleteTrack(int idx)
{
    disableUpdatesCnt++;
    removeTrackFromDefaultComboBox(idx);
    delete getCodecInfo(idx);
    int lastItemIndex = idx;  // trackLV.items[idx].index;
    ui->trackLV->removeRow(idx);
    if (ui->trackLV->rowCount() == 0)
    {
        lastSourceDir.clear();
        while (ui->tabWidgetTracks->count()) ui->tabWidgetTracks->removeTab(0);
        ui->tabWidgetTracks->addTab(ui->tabSheetFake, TI_DEFAULT_TAB_NAME());
        ui->outFileName->setText(getDefaultOutputFileName());
        outFileNameModified = false;
    }
    else
    {
        if (lastItemIndex > ui->trackLV->rowCount() - 1)
            --lastItemIndex;
        if (lastItemIndex >= 0)
        {
            ui->trackLV->setCurrentCell(lastItemIndex, 0);
            ui->trackLV->setRangeSelected(QTableWidgetSelectionRange(lastItemIndex, 0, lastItemIndex, 4), true);
        }
        updateNum();
    }
    updateMaxOffsets();
    updateMetaLines();
    ui->moveupBtn->setEnabled(ui->trackLV->currentItem() != 0);
    ui->movedownBtn->setEnabled(ui->trackLV->currentItem() != 0);
    ui->removeTrackBtn->setEnabled(ui->trackLV->currentItem() != 0);
    disableUpdatesCnt--;
    trackLVItemSelectionChanged();
    updateTracksComboBox(ui->defaultAudioTrackComboBox);
    updateTracksComboBox(ui->defaultSubTrackComboBox);
}

void TsMuxerWindow::updateNum()
{
    // for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    //	ui->trackLV->item(i,0)->setText(QString::number(i+1));
}

void TsMuxerWindow::onAppendButtonClick()
{
    QList<QtvCodecInfo> codecList;
    if (ui->inputFilesLV->currentItem() == 0)
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("No track selected"));
        msgBox.setText(tr("No track selected"));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return;
    }
    showAddFilesDialog(tr("Append media files"), [this]() { appendFile(); });
}

void TsMuxerWindow::appendFile()
{
    processAddFileList(&TsMuxerWindow::continueAppendFile, &TsMuxerWindow::fileAppended, &TsMuxerWindow::appendFile);
}

void TsMuxerWindow::continueAppendFile()
{
    QString parentFileName = myUnquoteStr((ui->inputFilesLV->currentItem()->data(FileNameRole).toString()));
    QFileInfo newFi(unquoteStr(newFileName));
    QFileInfo oldFi(unquoteStr(parentFileName));
    if (newFi.suffix().toUpper() != oldFi.suffix().toUpper())
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Invalid file extension"));
        msgBox.setText(tr("Appended file must have same file extension."));
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return;
    }

    disableUpdatesCnt++;

    int idx = ui->inputFilesLV->currentRow();
    bool firstStep = true;
    bool doublePrefix = false;
    for (; idx < ui->inputFilesLV->count(); ++idx)
    {
        int mplsRole = ui->inputFilesLV->item(idx)->data(MplsItemRole).toInt();
        if ((mplsRole == MPLS_PRIMARY && firstStep) || mplsRole == MPLS_M2TS)
        {
            ui->inputFilesLV->setCurrentRow(idx);
            doublePrefix = true;
        }
        else
            break;
        firstStep = false;
    }

    doAppendInt(newFileName, parentFileName, fileDuration, false, doublePrefix ? MPLS_PRIMARY : MPLS_NONE);

    if (mplsFileList.size() > 0)
        doAppendInt(mplsFileList[0].name, newFileName, mplsFileList[0].duration, doublePrefix, MPLS_M2TS);
    for (int mplsCnt = 1; mplsCnt < mplsFileList.size(); ++mplsCnt)
        doAppendInt(mplsFileList[mplsCnt].name, mplsFileList[mplsCnt - 1].name, mplsFileList[mplsCnt].duration,
                    doublePrefix, MPLS_M2TS);

    updateMaxOffsets();
    if (!outFileNameModified)
    {
        modifyOutFileName(newFileName);
        outFileNameModified = true;
    }
    disableUpdatesCnt--;
    updateMetaLines();
    updateCustomChapters();
    emit fileAppended();
}

void TsMuxerWindow::onRemoveTrackButtonClick()
{
    if (ui->trackLV->currentItem())
        deleteTrack(ui->trackLV->currentRow());
}

void TsMuxerWindow::onMoveUpButtonCLick()
{
    if (ui->trackLV->currentItem() == 0 || ui->trackLV->currentRow() < 1)
        return;
    disableUpdatesCnt++;
    auto preMoveRow = ui->trackLV->currentRow();
    moveRow(preMoveRow, preMoveRow - 1);
    moveTrackInDefaultComboBox(preMoveRow, preMoveRow - 1);
    disableUpdatesCnt--;
    updateMetaLines();
    updateNum();
}

void TsMuxerWindow::onMoveDownButtonCLick()
{
    if (ui->trackLV->currentItem() == 0 || ui->trackLV->rowCount() == 0 ||
        ui->trackLV->currentRow() == ui->trackLV->rowCount() - 1)
        return;
    disableUpdatesCnt++;
    auto preMoveRow = ui->trackLV->currentRow();
    moveRow(preMoveRow, preMoveRow + 2);
    moveTrackInDefaultComboBox(preMoveRow, preMoveRow + 1);
    disableUpdatesCnt--;
    updateMetaLines();
    updateNum();
}

void TsMuxerWindow::moveRow(int index, int index2)
{
    ui->trackLV->insertRow(index2);
    ui->trackLV->setRowHeight(index2, 18);
    if (index2 < index)
        index++;
    for (int i = 0; i < ui->trackLV->columnCount(); ++i)
        ui->trackLV->setItem(index2, i, ui->trackLV->item(index, i)->clone());
    ui->trackLV->removeRow(index);
    if (index2 > index)
        index2--;
    ui->trackLV->setRangeSelected(QTableWidgetSelectionRange(index2, 0, index2, 4), true);
    ui->trackLV->setCurrentCell(index2, 0);
}

void TsMuxerWindow::RadioButtonMuxClick()
{
    if (outFileNameDisableChange)
        return;
    if (ui->radioButtonDemux->isChecked())
        ui->buttonMux->setText(tr("Sta&rt demuxing"));
    else
        ui->buttonMux->setText(tr("Sta&rt muxing"));
    ui->checkBoxNewAudioPes->setChecked(!ui->radioButtonTS->isChecked());
    ui->checkBoxNewAudioPes->setEnabled(ui->radioButtonTS->isChecked() || ui->radioButtonM2TS->isChecked());
    outFileNameDisableChange = true;
    if (ui->radioButtonBluRay->isChecked() || ui->radioButtonDemux->isChecked() || ui->radioButtonAVCHD->isChecked())
    {
        QFileInfo fi(unquoteStr(ui->outFileName->text()));
        if (!fi.suffix().isEmpty())
        {
            oldFileName = fi.fileName();
            ui->outFileName->setText(QDir::toNativeSeparators(fi.absolutePath()) + QDir::separator());
        }
        ui->FilenameLabel->setText(tr("Folder"));
    }
    else
    {
        ui->FilenameLabel->setText(tr("File name"));
        if (!oldFileName.isEmpty())
        {
            ui->outFileName->setText(QDir::toNativeSeparators(ui->outFileName->text()));
            if (!ui->outFileName->text().isEmpty() && ui->outFileName->text().right(1) != QDir::separator())
                ui->outFileName->setText(ui->outFileName->text() + QDir::separator());
            ui->outFileName->setText(ui->outFileName->text() + oldFileName);
            oldFileName.clear();
        }
        if (ui->radioButtonTS->isChecked())
        {
            ui->outFileName->setText(changeFileExt(ui->outFileName->text(), "ts"));
            mSaveDialogFilter = TS_SAVE_DIALOG_FILTER();
        }
        else if (ui->radioButtonBluRayISO->isChecked())
        {
            ui->outFileName->setText(changeFileExt(ui->outFileName->text(), "iso"));
            mSaveDialogFilter = ISO_SAVE_DIALOG_FILTER();
        }
        else
        {
            ui->outFileName->setText(changeFileExt(ui->outFileName->text(), "m2ts"));
            mSaveDialogFilter = M2TS_SAVE_DIALOG_FILTER();
        }
    }
    ui->DiskLabel->setVisible(ui->radioButtonBluRayISO->isChecked());
    ui->DiskLabelEdit->setVisible(ui->radioButtonBluRayISO->isChecked());
    ui->editDelay->setEnabled(!ui->radioButtonDemux->isChecked());
    updateMetaLines();
    outFileNameDisableChange = false;
}

void TsMuxerWindow::outFileNameChanged()
{
    outFileNameModified = true;
    if (outFileNameDisableChange)
        return;
    if (ui->radioButtonDemux->isChecked() || ui->radioButtonBluRay->isChecked() || ui->radioButtonAVCHD->isChecked())
        return;

    outFileNameDisableChange = true;
    QFileInfo fi(unquoteStr(ui->outFileName->text().trimmed()));
    QString ext = fi.suffix().toUpper();

    bool isISOMode = ui->radioButtonBluRayISO->isChecked();

    if (ext == "M2TS" || ext == "M2TS\"")
        ui->radioButtonM2TS->setChecked(true);
    else if (ext == "ISO" || ext == "ISO\"")
    {
        ui->radioButtonBluRayISO->setChecked(true);
    }
    else
        ui->radioButtonTS->setChecked(true);

    bool isISOModeNew = ui->radioButtonBluRayISO->isChecked();

    ui->DiskLabel->setVisible(ui->radioButtonBluRayISO->isChecked());
    ui->DiskLabelEdit->setVisible(ui->radioButtonBluRayISO->isChecked());
    if (isISOMode != isISOModeNew)
        updateMetaLines();
    outFileNameDisableChange = false;
}

void TsMuxerWindow::saveFileDialog()
{
    if (ui->radioButtonDemux->isChecked() || ui->radioButtonBluRay->isChecked() || ui->radioButtonAVCHD->isChecked())
    {
        QString folder = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, getOutputDir()));
        if (!folder.isEmpty())
        {
            ui->outFileName->setText(folder + QDir::separator());
            outFileNameModified = true;
            lastOutputDir = folder;
            writeSettings();
        }
    }
    else
    {
        auto fileName = unquoteStr(ui->outFileName->text());
        auto path = fileName.isEmpty() ? getOutputDir() : QFileInfo(fileName).absoluteFilePath();
        fileName = QDir::toNativeSeparators(
            QFileDialog::getSaveFileName(this, tr("Select file for muxing"), path, mSaveDialogFilter));
        if (!fileName.isEmpty())
        {
            ui->outFileName->setText(fileName);
            lastOutputDir = QFileInfo(fileName).absolutePath();
            writeSettings();
        }
    }
}

void TsMuxerWindow::startMuxing()
{
    QString outputName = unquoteStr(ui->outFileName->text().trimmed());
    ui->outFileName->setText(outputName);
    lastOutputDir = QFileInfo(outputName).absolutePath();
    writeSettings();
    if (ui->radioButtonM2TS->isChecked())
    {
        QFileInfo fi(ui->outFileName->text());
        if (fi.suffix().toUpper() != "M2TS")
        {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Invalid file name"));
            msgBox.setText(tr("The output file \"%1\" has invalid extension. Please, change file extension "
                              "to \".m2ts\"")
                               .arg(ui->outFileName->text()));
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();
            return;
        }
    }
    else if (ui->radioButtonBluRayISO->isChecked())
    {
        QFileInfo fi(ui->outFileName->text());
        if (fi.suffix().toUpper() != "ISO")
        {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Invalid file name"));
            msgBox.setText(tr("The output file \"%1\" has invalid extension. Please, change file extension to \".iso\"")
                               .arg(ui->outFileName->text()));
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();
            return;
        }
    }

    bool isFile =
        ui->radioButtonM2TS->isChecked() || ui->radioButtonTS->isChecked() || ui->radioButtonBluRayISO->isChecked();
    if (isFile && QFile::exists(ui->outFileName->text()))
    {
        //: Used in expressions "Overwrite existing %1" and "The output %1 already exists".
        auto fileOrDir = isFile ? tr("file") : tr("directory");
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Overwrite existing %1?").arg(fileOrDir));
        msgBox.setText(tr("The output %1 \"%2\" already exists. Do you want to overwrite it?")
                           .arg(fileOrDir)
                           .arg(ui->outFileName->text()));
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if (msgBox.exec() != QMessageBox::Yes)
            return;
    }

    QFileInfo fi(ui->outFileName->text());
    metaName =
        QDir::toNativeSeparators(QDir::tempPath()) + QDir::separator() + "tsMuxeR_" + fi.completeBaseName() + ".meta";
    if (!saveMetaFile(metaName))
    {
        metaName.clear();
        return;
    }
    muxForm->prepare(!ui->radioButtonDemux->isChecked() ? tr("Muxing in progress") : tr("Demuxing in progress"));
    ui->buttonMux->setEnabled(false);
    ui->addBtn->setEnabled(false);
    ui->btnAppend->setEnabled(false);
    muxForm->show();
    disconnect();
    // QCoreApplication::dir
    runInMuxMode = true;
    tsMuxerExecute(QStringList() << metaName << quoteStr(ui->outFileName->text()));
}

void TsMuxerWindow::saveMetaFileBtnClick()
{
    QString metaName = QFileDialog::getSaveFileName(this, "", changeFileExt(ui->outFileName->text(), "meta"),
                                                    tr("tsMuxeR project file (*.meta);;All files (*.*)"));
    if (metaName.isEmpty())
        return;
    QFileInfo fi(metaName);
    QDir dir;
    dir.mkpath(fi.absolutePath());
    saveMetaFile(metaName);
}

bool TsMuxerWindow::saveMetaFile(const QString &metaName)
{
    QFile file(metaName);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Can't create temporary meta file"));
        msgBox.setText(tr("Can't create temporary meta file \"%1\"").arg(metaName));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return false;
    }
    QByteArray metaText = ui->memoMeta->toPlainText().toUtf8();
    file.write(metaText);
    file.close();
    return true;
}

void TsMuxerWindow::closeEvent(QCloseEvent *event)
{
    if (!metaName.isEmpty())
    {
        QFile::remove(metaName);
        metaName.clear();
    }
    muxForm->close();
    event->accept();
}

void TsMuxerWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
    QWidget::changeEvent(event);
}

void TsMuxerWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/plain") || event->mimeData()->hasFormat("text/uri-list"))
    {
        if (ui->addBtn->isEnabled())
        {
            opacityTimer.stop();
            setWindowOpacity(0.9);
            event->acceptProposedAction();
            QWidget *w = childAt(event->pos());
            updateBtns(w);
        }
    }
}

void TsMuxerWindow::dropEvent(QDropEvent *event)
{
    setWindowOpacity(1.0);
    updateBtns(0);
    if (event->mimeData()->hasFormat("text/uri-list"))
    {
        addFileList = event->mimeData()->urls();
        event->acceptProposedAction();
    }
    else if (event->mimeData()->hasFormat("text/plain"))
    {
        QList<QString> strList;
        addFileList.clear();
        splitLines(event->mimeData()->text(), strList);
        QList<QUrl> urls = event->mimeData()->urls();
        for (int i = 0; i < strList.size(); ++i) addFileList << QUrl::fromLocalFile(strList[i]);
        event->acceptProposedAction();
    }
    if (addFileList.isEmpty())
        return;
    auto w = childAt(event->pos());
    if (w && w == ui->btnAppend && w->isEnabled())
        appendFile();
    else if (ui->addBtn->isEnabled())
        addFile();
}

void TsMuxerWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
    QWidget *w = childAt(event->pos());
    updateBtns(w);
}

void TsMuxerWindow::updateBtns(QWidget *w)
{
    if (w)
    {
        ui->btnAppend->setDefault(w == ui->btnAppend && w->isEnabled());
        ui->addBtn->setDefault(w == ui->addBtn && w->isEnabled());
    }
    else
    {
        ui->btnAppend->setDefault(false);
        ui->addBtn->setDefault(false);
    }
    QFont font = ui->removeFile->font();
    QFont bFont(font);
    bFont.setBold(true);
    if (ui->btnAppend->isDefault())
        ui->btnAppend->setFont(bFont);
    else
        ui->btnAppend->setFont(font);
    if (ui->addBtn->isDefault())
        ui->addBtn->setFont(bFont);
    else
        ui->addBtn->setFont(font);
}

void TsMuxerWindow::dragLeaveEvent(QDragLeaveEvent *) { opacityTimer.start(100); }

void TsMuxerWindow::onOpacityTimer()
{
    opacityTimer.stop();
    setWindowOpacity(1.0);
    updateBtns(0);
}

void TsMuxerWindow::updateMaxOffsets()
{
    int maxPGOffsets = 0;
    m_3dMode = false;

    for (int i = 0; i < ui->trackLV->rowCount(); ++i)
    {
        auto codecInfo = getCodecInfo(i);
        if (!codecInfo)
            continue;

        if (codecInfo->displayName == "MVC")
        {
            m_3dMode = true;
            maxPGOffsets = qMax(maxPGOffsets, codecInfo->maxPgOffsets);
        }
    }

    disableUpdatesCnt++;

    int oldIndex = ui->offsetsComboBox->currentIndex();
    ui->offsetsComboBox->clear();
    ui->offsetsComboBox->addItem(QString("zero"));
    for (int i = 0; i < maxPGOffsets; ++i) ui->offsetsComboBox->addItem(QString("plane #%1").arg(i));
    if (oldIndex >= 0 && oldIndex < ui->offsetsComboBox->count())
        ui->offsetsComboBox->setCurrentIndex(oldIndex);

    disableUpdatesCnt--;
}

bool TsMuxerWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->label_Donate && event->type() == QEvent::MouseButtonPress)
    {
        QDesktopServices::openUrl(QUrl("https://github.com/justdan96/tsMuxer"));
        return true;
    }
    else
    {
        return QWidget::eventFilter(obj, event);
    }
}

void TsMuxerWindow::at_sectionCheckstateChanged(Qt::CheckState state)
{
    if (disableUpdatesCnt > 0)
        return;

    disableUpdatesCnt++;
    for (int i = 0; i < ui->trackLV->rowCount(); ++i) ui->trackLV->item(i, 0)->setCheckState(state);
    disableUpdatesCnt--;
    trackLVItemSelectionChanged();
}

void TsMuxerWindow::writeSettings()
{
    if (disableUpdatesCnt > 0)
        return;

    disableUpdatesCnt++;

    settings->beginGroup("main");
    // settings->setValue("asyncIO", ui->checkBoxuseAsynIO->isChecked());
    settings->setValue("soundEnabled", ui->checkBoxSound->isChecked());
    settings->setValue("hdmvPES", ui->checkBoxNewAudioPes->isChecked());
    if (ui->checkBoxCrop->isEnabled())
        settings->setValue("restoreCropEnabled", ui->checkBoxCrop->isChecked());
    settings->setValue("inputDir", lastInputDir);
    settings->setValue("outputDir", lastOutputDir);
    settings->setValue("useBlankPL", ui->checkBoxBlankPL->isChecked());
    settings->setValue("blankPLNum", ui->BlackplaylistCombo->value());

    settings->setValue("outputToInputFolder", ui->radioButtonOutoutInInput->isChecked());
    settings->setValue("language", ui->languageSelectComboBox->currentText());
    settings->setValue("windowSize", size());

    settings->endGroup();

    settings->beginGroup("subtitles");
    settings->setValue("fontBorder", ui->spinEditBorder->value());
    settings->setValue("fontLineSpacing", ui->lineSpacing->value());
    settings->setValue("offset", ui->spinEditOffset->value());
    settings->setValue("fadeTime", getRendererAnimationTime());
    settings->setValue("family", ui->listViewFont->item(0, 1)->text());
    settings->setValue("size", ui->listViewFont->item(1, 1)->text().toUInt());
    settings->setValue("color", ui->listViewFont->item(2, 1)->text().mid(2).toUInt(0, 16));
    settings->setValue("options", ui->listViewFont->item(4, 1)->text());
    settings->endGroup();

    settings->beginGroup("pip");
    settings->setValue("corner", ui->comboBoxPipCorner->currentIndex());
    settings->setValue("h_offset", ui->spinBoxPipOffsetH->value());
    settings->setValue("v_offset", ui->spinBoxPipOffsetV->value());
    settings->setValue("size", ui->comboBoxPipSize->currentIndex());
    settings->endGroup();

    disableUpdatesCnt--;
}

bool TsMuxerWindow::readSettings()
{
    // due to QTBUG-28893, the settings saved under "general" are not accessible
    // when using .ini files for storage - those are used on Linux by default.
    // newer GUI versions will save the settings under the "main" group to avoid
    // that. the "general" group is still read in order to import the old settings
    // on non-Linux systems where the bug doesn't occur.
    if (!readGeneralSettings("main") && !readGeneralSettings("general"))
    {
        return false;  // no settings still written
    }
    // checkBoxVBR checkBoxRVBR    editMaxBitrate  editMinBitrate  checkBoxCBR
    // editCBRBitrate  editVBVLen

    settings->beginGroup("subtitles");
    ui->spinEditBorder->setValue(settings->value("fontBorder").toInt());
    ui->lineSpacing->setValue(settings->value("fontLineSpacing").toDouble());
    setRendererAnimationTime(settings->value("fadeTime").toDouble());
    ui->spinEditOffset->setValue(settings->value("offset").toInt());
    // keep backward compatibility with versions < 2.6.15 which contain "famaly" key
    if (settings->contains("famaly"))
    {
        settings->setValue("family", settings->value("famaly"));
        settings->remove("famaly");
    }
    QString fontName = settings->value("family").toString();
    if (!fontName.isEmpty())
        ui->listViewFont->item(0, 1)->setText(fontName);
    int fontSize = settings->value("size").toInt();
    if (fontSize > 0)
        ui->listViewFont->item(1, 1)->setText(QString::number(fontSize));
    if (!settings->value("color").isNull())
    {
        quint32 color = settings->value("color").toUInt();
        setTextItemColor(QString::number(color, 16));
    }
    ui->listViewFont->item(4, 1)->setText(settings->value("options").toString());
    settings->endGroup();

    settings->beginGroup("pip");
    ui->comboBoxPipCorner->setCurrentIndex(settings->value("corner").toInt());
    ui->spinBoxPipOffsetH->setValue(settings->value("h_offset").toInt());
    ui->spinBoxPipOffsetV->setValue(settings->value("v_offset").toInt());
    ui->comboBoxPipSize->setCurrentIndex(settings->value("size").toInt());
    settings->endGroup();

    return true;
}

bool TsMuxerWindow::readGeneralSettings(const QString &prefix)
{
    settings->beginGroup(prefix);

    auto size = settings->value("windowSize");
    if (size.isValid() && size.canConvert<QSize>())
    {
        resize(size.toSize());
    }

    auto lang = settings->value("language");
    if (lang.isValid())
    {
        ui->languageSelectComboBox->setCurrentText(lang.toString());
    }
    else
    {
        ui->languageSelectComboBox->setCurrentIndex(0);
    }

    if (!settings->contains("outputDir"))
    {
        settings->endGroup();
        return false;
    }

    lastInputDir = settings->value("inputDir").toString();
    lastOutputDir = settings->value("outputDir").toString();

    // ui->checkBoxuseAsynIO->setChecked(settings->value("asyncIO").toBool());
    ui->checkBoxSound->setChecked(settings->value("soundEnabled").toBool());
    ui->checkBoxNewAudioPes->setChecked(settings->value("hdmvPES").toBool());
    ui->checkBoxCrop->setChecked(settings->value("restoreCropEnabled").toBool());
    ui->checkBoxBlankPL->setChecked(settings->value("useBlankPL").toBool());
    int plNum = settings->value("blankPLNum").toInt();
    if (plNum)
        ui->BlackplaylistCombo->setValue(plNum);

    ui->radioButtonOutoutInInput->setChecked(settings->value("outputToInputFolder").toBool());
    ui->radioButtonStoreOutput->setChecked(!ui->radioButtonOutoutInInput->isChecked());

    settings->endGroup();
    return true;
}

template <typename OnCodecListReadyFn, typename PostActionSignal, typename PostActionFn>
void TsMuxerWindow::processAddFileList(OnCodecListReadyFn onCodecListReady, PostActionSignal postActionSignal,
                                       PostActionFn postActionFn)
{
    if (addFileList.isEmpty())
        return;
    newFileName = QDir::toNativeSeparators(addFileList[0].toLocalFile());
    if (lastSourceDir.isEmpty())
        lastSourceDir = QFileInfo(newFileName).absolutePath();
    addFileList.removeAt(0);
    if (!checkFileDuplicate(newFileName))
        return;
    // disconnect(this, SIGNAL(tsMuxerSuccessFinished()));
    // disconnect(this, SIGNAL(codecListReady()));
    disconnect();
    connect(this, &TsMuxerWindow::tsMuxerSuccessFinished, this, &TsMuxerWindow::onTsMuxerCodecInfoReceived);
    connect(this, &TsMuxerWindow::codecListReady, this, onCodecListReady);
    connect(this, postActionSignal, this, postActionFn);
    runInMuxMode = false;
    tsMuxerExecute(QStringList() << newFileName);
}

template <typename F>
void TsMuxerWindow::showAddFilesDialog(QString &&windowTitle, F &&windowOkFn)
{
    const auto files = QFileDialog::getOpenFileNames(this, windowTitle, lastInputDir, fileDialogFilter());
    if (files.isEmpty())
        return;
    lastInputDir = QDir::toNativeSeparators(files.back());
    addFileList.clear();
    for (auto f : files)
    {
        addFileList << QUrl::fromLocalFile(QDir::toNativeSeparators(f));
    }
    windowOkFn();
}
