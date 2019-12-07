#include "tsmuxerwindow.h"
#include "lang_codes.h"
#include <QApplication>
#include <QColorDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>

#ifdef WIN32
#include "windows.h"
#endif

const char fileDialogFilter[] =
    "All supported media files (*.aac *.mpv *.mpa *.avc *.mvc *.264 *.h264 *.ac3 *.dts *.ts *.m2ts *.mts *.ssif *.mpg *.mpeg *.vob *.evo *.mkv *.mka *.mks *.mp4 *.m4a *.mov *.sup *.wav *.w64 *.pcm *.m1v *.m2v *.vc1 *.hevc *.hvc *.265 *.h265 *.mpls *.mpl *.srt);;\
AC3/E-AC3 (*.ac3 *.ddp);;\
AAC (advanced audio coding) (*.aac);;\
AVC/MVC/H.264 elementary stream (*.avc *.mvc *.264 *.h264);;\
HEVC (High Efficiency Video Codec) (*.hevc *.hvc *.265 *.h265);;\
Digital Theater System (*.dts);;\
Mpeg video elementary stream (*.mpv *.m1v *.m2v);;\
Mpeg audio elementary stream (*.mpa);;\
Transport Stream (*.ts);;\
BDAV Transport Stream (*.m2ts *.mts *.ssif);;\
Program Stream (*.mpg *.mpeg *.vob *.evo);;\
Matroska audio/video files (*.mkv *.mka *.mks);;\
MP4 audio/video files (*.mp4 *.m4a);;\
Quick time audio/video files (*.mov);;\
Blu-ray play list (*.mpls *.mpl);;\
Blu-ray PGS subtitles (*.sup);;\
Text subtitles (*.srt);;\
WAVE - Uncompressed PCM audio (*.wav *.w64);;\
RAW LPCM Stream (*.pcm);;\
All files (*.*)";
const char saveMetaFilter[] = "tsMuxeR project file (*.meta);;All files (*.*)";

const double EPS = 1e-6;
const char TI_DEFAULT_TAB_NAME[] = "General track options";
const char TI_DEMUX_TAB_NAME[] = "Demux options";
const char TS_SAVE_DIALOG_FILTER[] = "Transport stream (*.ts);;all files (*.*)";
const char M2TS_SAVE_DIALOG_FILTER[] =
    "BDAV Transport Stream (*.m2ts);;all files (*.*)";
const char ISO_SAVE_DIALOG_FILTER[] = "Disk image (*.iso);;all files (*.*)";

QSettings *settingsIniFile = nullptr;
QSettings *settings = nullptr;

enum FileCustomData {
  MplsItemRole = Qt::UserRole,
  FileNameRole,
  ChaptersRole,
  FileDurationRole
};

static const QString FILE_JOIN_PREFIX(" ++ ");

bool doubleCompare(double a, double b) { return qAbs(a - b) < 1e-6; }

QLocale locale;

QString closeDirPath(const QString &src) {
  if (src.isEmpty())
    return src;
  if (src[src.length() - 1] == L'/' || src[src.length() - 1] == L'\\')
    return src;
  return src + QDir::separator();
}

QString unquoteStr(QString val) {
  val = val.trimmed();
  if (val.isEmpty())
    return val;
  if (val.at(0) == '\"') {
    if (val.right(1) == "\"")
      return val.mid(1, val.length() - 2);
    else
      return val.mid(1, val.length() - 1);
  } else {
    if (val.right(1) == "\"")
      return val.mid(0, val.length() - 1);
    else
      return val;
  }
}

bool isVideoCodec(const QString &displayName) {
  return displayName == "H.264" || displayName == "MVC" ||
         displayName == "VC-1" || displayName == "MPEG-2" ||
         displayName == "HEVC";
}

QString floatToTime(double time, char msSeparator = '.') {
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

QTime qTimeFromFloat(double secondsF) {
  int seconds = (int)secondsF;
  int ms = (secondsF - seconds) * 1000.0;
  int hours = seconds / 3600;
  seconds -= hours * 3600;
  int minute = seconds / 60;
  seconds -= minute * 60;
  return QTime(hours, minute, seconds, ms);
}

int qTimeToMsec(const QTime &time) {
  return (time.hour() * 3600 + time.minute() * 60 + time.second()) * 1000 +
         time.msec();
}

double timeToFloat(const QString &chapterStr) {
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

QString changeFileExt(const QString &value, const QString &newExt) {
  QFileInfo fi(unquoteStr(value));
  if (fi.suffix().length() > 0 || (!value.isEmpty() && value.right(1) == "."))
    return unquoteStr(value.left(value.length() - fi.suffix().length()) +
                      newExt);
  else
    return unquoteStr(value) + "." + newExt;
}

QString toFixedDecPoint(QString str) {
  for (int i = 0; i < str.length(); ++i)
    if (str[i] == ',')
      str[i] = '.';
  return str;
}

QString toNativeDecPoint(QString str) {
  for (int i = 0; i < str.length(); ++i)
    if (str[i] == ',' || str[i] == '.')
      str[i] = locale.decimalPoint();
  return str;
}

float myStrToFloat(QString str) {
  for (int i = 0; i < str.length(); ++i)
    if (str[i] == ',' || str[i] == '.')
      str[i] = locale.decimalPoint();
  return str.toFloat();
}

QString myFloatToStr(float val) {
  QString str = QString::number(double(val));
  for (int i = 0; i < str.length(); ++i)
    if (str[i] == locale.decimalPoint())
      str[i] = '.';
  return str;
}

QString fpsTextToFpsStr(const QString &fpsText) {
  int p = fpsText.indexOf('/');
  if (p >= 0) {
    QString leftStr = fpsText.mid(0, p);
    QString rightStr = fpsText.mid(p + 1);
    return toFixedDecPoint(QString::number(
        double(myStrToFloat(leftStr) / myStrToFloat(rightStr)), 'f', 3));
  } else
    return myFloatToStr(myStrToFloat(fpsText));
}

float extractFloatFromDescr(const QString &str, const QString &pattern) {
  try {
    int p = 0;
    if (!pattern.isEmpty())
      p = str.indexOf(pattern);
    if (p >= 0) {
      p += pattern.length();
      int p2 = p;
      while (p2 < str.length() && ((str.at(p2) >= '0' && str.at(p2) <= '9') ||
                                   str.at(p2) == '.' || str.at(p2) == '-'))
        p2++;
      QString tmp = str.mid(p, p2 - p);
      return myStrToFloat(tmp);
    }
  } catch (...) {
    return 0;
  }
  return 0;
}

QString quoteStr(const QString &val) {
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

// -------------------------------------------------------------------------- //
// QnCheckBoxedHeaderView
// -------------------------------------------------------------------------- //
QnCheckBoxedHeaderView::QnCheckBoxedHeaderView(QWidget *parent)
    : base_type(Qt::Horizontal, parent), m_checkState(Qt::Unchecked),
      m_checkColumnIndex(0) {
  connect(this, &QnCheckBoxedHeaderView::sectionClicked, this,
          &QnCheckBoxedHeaderView::at_sectionClicked);
}

Qt::CheckState QnCheckBoxedHeaderView::checkState() const {
  return m_checkState;
}

void QnCheckBoxedHeaderView::setCheckState(Qt::CheckState state) {
  if (state == m_checkState)
    return;
  m_checkState = state;
  emit checkStateChanged(state);

  viewport()->update();
}

void QnCheckBoxedHeaderView::paintEvent(QPaintEvent *e) {
  base_type::paintEvent(e);
}

void QnCheckBoxedHeaderView::paintSection(QPainter *painter, const QRect &rect,
                                          int logicalIndex) const {
  painter->save();
  base_type::paintSection(painter, rect, logicalIndex);
  painter->restore();

  if (logicalIndex == m_checkColumnIndex) {

    if (!rect.isValid())
      return;
    QStyleOptionButton opt;
    opt.initFrom(this);

    QStyle::State state = QStyle::State_Raised;
    if (isEnabled())
      state |= QStyle::State_Enabled;
    if (window()->isActiveWindow())
      state |= QStyle::State_Active;

    switch (m_checkState) {
    case Qt::Checked:
      state |= QStyle::State_On;
      break;
    case Qt::Unchecked:
      state |= QStyle::State_Off;
      break;
    default:
      state |= QStyle::State_NoChange;
      break;
    }

    opt.rect = rect.adjusted(4, 0, 0, 0);
    opt.state |= state;
    opt.text = QString();
    painter->save();
    style()->drawControl(QStyle::CE_CheckBox, &opt, painter, this);
    painter->restore();
    return;
  }
}

QSize QnCheckBoxedHeaderView::sectionSizeFromContents(int logicalIndex) const {
  QSize size = base_type::sectionSizeFromContents(logicalIndex);
  if (logicalIndex != m_checkColumnIndex)
    return size;
  size.setWidth(15);
  return size;
}

void QnCheckBoxedHeaderView::at_sectionClicked(int logicalIndex) {
  if (logicalIndex != m_checkColumnIndex)
    return;
  if (m_checkState != Qt::Checked)
    setCheckState(Qt::Checked);
  else
    setCheckState(Qt::Unchecked);
}

// ----------------------- TsMuxerWindow -------------------------------------

QString TsMuxerWindow::getOutputDir() const {
  QString result =
      ui.radioButtonOutoutInInput->isChecked() ? lastSourceDir : lastOutputDir;
  if (!result.isEmpty())
    result = QDir::toNativeSeparators(closeDirPath(result));
  return result;
}

QString TsMuxerWindow::getDefaultOutputFileName() const {
  QString prefix = getOutputDir();

  if (ui.radioButtonTS->isChecked())
    return prefix + QString("default.ts");
  else if (ui.radioButtonM2TS->isChecked())
    return prefix + QString("default.m2ts");
  else if (ui.radioButtonBluRayISO->isChecked())
    return prefix + QString("default.iso");
  else if (ui.radioButtonBluRayISOUHD->isChecked())
    return prefix + QString("default.iso");
  else
    return prefix;
}

TsMuxerWindow::TsMuxerWindow()
    : disableUpdatesCnt(0), outFileNameModified(false),
      outFileNameDisableChange(false), muxForm(this), tempSoundFile(0),
      sound(0), m_updateMeta(true), m_3dMode(false) {
  ui.setupUi(this);
  lastInputDir = QDir::homePath();
  lastOutputDir = QDir::homePath();

  QString path = QFileInfo(QApplication::arguments()[0]).absolutePath();
  QString iniName = QDir::toNativeSeparators(path) + QDir::separator() +
                    QString("tsMuxerGUI.ini");

  settings = new QSettings(QSettings::UserScope, "Network Optix", "tsMuxeR");
  readSettings();

  if (QFile::exists(iniName)) {
    delete settings;
    settings = new QSettings(iniName, QSettings::IniFormat);
    settings->setIniCodec("UTF-8");
    if (!readSettings())
      writeSettings(); // copy current registry settings to the ini file
  }

  ui.outFileName->setText(getDefaultOutputFileName());

  // next properties supported by Designer in version 4.5 only.
  ui.listViewFont->horizontalHeader()->setVisible(false);
  ui.listViewFont->verticalHeader()->setVisible(false);
  ui.listViewFont->horizontalHeader()->setStretchLastSection(true);

  m_header = new QnCheckBoxedHeaderView(this);
  ui.trackLV->setHorizontalHeader(m_header);
  ui.trackLV->horizontalHeader()->setStretchLastSection(true);
  // ui.trackLV->model()->setHeaderData(0, Qt::Vertical, "#");
  m_header->setVisible(true);
  m_header->setSectionsClickable(true);

  connect(m_header, &QnCheckBoxedHeaderView::checkStateChanged, this,
          &TsMuxerWindow::at_sectionCheckstateChanged);

  /////////////////////////////////////////////////////////////
  for (int i = 0; i <= 3600; i += 5 * 60)
    ui.memoChapters->insertPlainText(floatToTime(i, '.') + '\n');

  saveDialogFilter = TS_SAVE_DIALOG_FILTER;
  const static int colWidths[] = {28, 200, 62, 38, 10};
  for (int i = 0; i < sizeof(colWidths) / sizeof(int); ++i)
    ui.trackLV->horizontalHeader()->resizeSection(i, colWidths[i]);

  ui.listViewFont->horizontalHeader()->resizeSection(0, 65);
  ui.listViewFont->horizontalHeader()->resizeSection(1, 185);
  for (int i = 0; i < ui.listViewFont->rowCount(); ++i) {
    ui.listViewFont->setRowHeight(i, 16);
    ui.listViewFont->item(i, 0)->setFlags(ui.listViewFont->item(i, 0)->flags() &
                                          (~Qt::ItemIsEditable));
    ui.listViewFont->item(i, 1)->setFlags(ui.listViewFont->item(i, 0)->flags() &
                                          (~Qt::ItemIsEditable));
  }
  const auto comboBoxIndexChanged = QOverload<int>::of(&QComboBox::currentIndexChanged);
  const auto spinBoxValueChanged = QOverload<int>::of(&QSpinBox::valueChanged);
  const auto doubleSpinBoxValueChanged = QOverload<double>::of(&QDoubleSpinBox::valueChanged);
  connect(&opacityTimer, &QTimer::timeout, this, &TsMuxerWindow::onOpacityTimer);
  connect(ui.trackLV, &QTableWidget::itemSelectionChanged, this,
          &TsMuxerWindow::trackLVItemSelectionChanged);
  connect(ui.trackLV, &QTableWidget::itemChanged, this,
          &TsMuxerWindow::trackLVItemChanged);
  connect(ui.inputFilesLV, &QListWidget::currentRowChanged, this,
          &TsMuxerWindow::inputFilesLVChanged);
  connect(ui.addBtn, &QPushButton::clicked, this, &TsMuxerWindow::onAddBtnClick);
  connect(ui.btnAppend, &QPushButton::clicked, this, &TsMuxerWindow::onAppendButtonClick);
  connect(ui.removeFile, &QPushButton::clicked, this, &TsMuxerWindow::onRemoveBtnClick);
  connect(ui.removeTrackBtn, &QPushButton::clicked, this,
          &TsMuxerWindow::onRemoveTrackButtonClick);
  connect(ui.moveupBtn, &QPushButton::clicked, this, &TsMuxerWindow::onMoveUpButtonCLick);
  connect(ui.movedownBtn, &QPushButton::clicked, this,
          &TsMuxerWindow::onMoveDownButtonCLick);
  connect(ui.checkFPS, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onVideoCheckBoxChanged);
  connect(ui.checkBoxLevel, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onVideoCheckBoxChanged);
  connect(ui.comboBoxSEI, comboBoxIndexChanged, this,
          &TsMuxerWindow::onVideoCheckBoxChanged);
  connect(ui.checkBoxSecondaryVideo, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onVideoCheckBoxChanged);
  connect(ui.checkBoxSPS, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onVideoCheckBoxChanged);
  connect(ui.checkBoxRemovePulldown, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onPulldownCheckBoxChanged);
  connect(ui.comboBoxFPS, comboBoxIndexChanged, this,
          &TsMuxerWindow::onVideoComboBoxChanged);
  connect(ui.comboBoxLevel, comboBoxIndexChanged, this,
          &TsMuxerWindow::onVideoComboBoxChanged);
  connect(ui.comboBoxAR, comboBoxIndexChanged, this,
          &TsMuxerWindow::onVideoComboBoxChanged);
  connect(ui.checkBoxKeepFps, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onAudioSubtitlesParamsChanged);
  connect(ui.dtsDwnConvert, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onAudioSubtitlesParamsChanged);
  connect(ui.secondaryCheckBox, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onAudioSubtitlesParamsChanged);
  connect(ui.langComboBox, comboBoxIndexChanged, this,
          &TsMuxerWindow::onAudioSubtitlesParamsChanged);
  connect(ui.offsetsComboBox, comboBoxIndexChanged, this,
          &TsMuxerWindow::onAudioSubtitlesParamsChanged);
  connect(ui.comboBoxPipCorner, comboBoxIndexChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.comboBoxPipSize, comboBoxIndexChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.spinBoxPipOffsetH, spinBoxValueChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.spinBoxPipOffsetV, spinBoxValueChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.checkBoxSound, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.editDelay, spinBoxValueChanged, this,
          &TsMuxerWindow::onEditDelayChanged);
  connect(ui.muxTimeEdit, &QTimeEdit::timeChanged, this,
          &TsMuxerWindow::updateMuxTime1);
  connect(ui.muxTimeClock, spinBoxValueChanged, this,
          &TsMuxerWindow::updateMuxTime2);
  connect(ui.fontButton, &QPushButton::clicked, this, &TsMuxerWindow::onFontBtnClicked);
  connect(ui.colorButton, &QPushButton::clicked, this, &TsMuxerWindow::onColorBtnClicked);
  connect(ui.checkBoxVBR, &QPushButton::clicked, this,
          &TsMuxerWindow::onGeneralCheckboxClicked);
  connect(ui.spinBoxMplsNum, spinBoxValueChanged, this,
          &TsMuxerWindow::onGeneralCheckboxClicked);
  connect(ui.spinBoxM2tsNum, spinBoxValueChanged, this,
          &TsMuxerWindow::onGeneralCheckboxClicked);
  connect(ui.checkBoxBlankPL, &QPushButton::clicked, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.BlackplaylistCombo, spinBoxValueChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.checkBoxNewAudioPes, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.checkBoxCrop, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.checkBoxRVBR, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onGeneralCheckboxClicked);
  connect(ui.checkBoxCBR, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onGeneralCheckboxClicked);
  // connect(ui.checkBoxuseAsynIO,	   SIGNAL(stateChanged(int)), this,
  // SLOT(onSavedParamChanged()));
  connect(ui.radioButtonStoreOutput, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.radioButtonOutoutInInput, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.editVBVLen, spinBoxValueChanged, this,
          &TsMuxerWindow::onGeneralSpinboxValueChanged);
  connect(ui.editMaxBitrate, doubleSpinBoxValueChanged, this,
          &TsMuxerWindow::onGeneralSpinboxValueChanged);
  connect(ui.editMinBitrate, doubleSpinBoxValueChanged, this,
          &TsMuxerWindow::onGeneralSpinboxValueChanged);
  connect(ui.editCBRBitrate, doubleSpinBoxValueChanged, this,
          &TsMuxerWindow::onGeneralSpinboxValueChanged);
  connect(ui.rightEyeCheckBox, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::updateMetaLines);
  connect(ui.radioButtonAutoChapter, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onChapterParamsChanged);
  connect(ui.radioButtonNoChapters, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onChapterParamsChanged);
  connect(ui.radioButtonCustomChapters, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onChapterParamsChanged);
  connect(ui.spinEditChapterLen, spinBoxValueChanged, this,
          &TsMuxerWindow::onChapterParamsChanged);
  connect(ui.memoChapters, &QTextEdit::textChanged, this,
          &TsMuxerWindow::onChapterParamsChanged);
  connect(ui.noSplit, &QAbstractButton::clicked, this, &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.splitByDuration, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.splitBySize, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.spinEditSplitDuration, spinBoxValueChanged, this,
          &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.editSplitSize, doubleSpinBoxValueChanged, this,
          &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.comboBoxMeasure, comboBoxIndexChanged, this,
          &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.checkBoxCut, &QCheckBox::stateChanged, this,
          &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.cutStartTimeEdit, &QTimeEdit::timeChanged, this,
          &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.cutEndTimeEdit, &QTimeEdit::timeChanged, this,
          &TsMuxerWindow::onSplitCutParamsChanged);
  connect(ui.spinEditBorder, spinBoxValueChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.comboBoxAnimation, comboBoxIndexChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.lineSpacing, doubleSpinBoxValueChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.spinEditOffset, spinBoxValueChanged, this,
          &TsMuxerWindow::onSavedParamChanged);
  connect(ui.radioButtonTS, &QAbstractButton::clicked, this,
          &TsMuxerWindow::RadioButtonMuxClick);
  connect(ui.radioButtonM2TS, &QAbstractButton::clicked, this,
          &TsMuxerWindow::RadioButtonMuxClick);
  connect(ui.radioButtonBluRay, &QAbstractButton::clicked, this,
          &TsMuxerWindow::RadioButtonMuxClick);
  connect(ui.radioButtonBluRayISO, &QAbstractButton::clicked, this,
          &TsMuxerWindow::RadioButtonMuxClick);
  connect(ui.radioButtonBluRayUHD, &QAbstractButton::clicked, this,
          &TsMuxerWindow::RadioButtonMuxClick);
  connect(ui.radioButtonBluRayISOUHD, &QAbstractButton::clicked, this,
          &TsMuxerWindow::RadioButtonMuxClick);
  connect(ui.radioButtonAVCHD, &QAbstractButton::clicked, this,
          &TsMuxerWindow::RadioButtonMuxClick);
  connect(ui.radioButtonDemux, &QAbstractButton::clicked, this,
          &TsMuxerWindow::RadioButtonMuxClick);
  connect(ui.outFileName, &QLineEdit::textChanged, this,
          &TsMuxerWindow::outFileNameChanged);
  connect(ui.DiskLabelEdit, &QLineEdit::textChanged, this,
          &TsMuxerWindow::onGeneralCheckboxClicked);
  connect(ui.btnBrowse, &QAbstractButton::clicked, this, &TsMuxerWindow::saveFileDialog);
  connect(ui.buttonMux, &QAbstractButton::clicked, this, &TsMuxerWindow::startMuxing);
  connect(ui.buttonSaveMeta, &QAbstractButton::clicked, this,
          &TsMuxerWindow::saveMetaFileBtnClick);
  connect(ui.radioButtonOutoutInInput, &QAbstractButton::clicked, this,
          &TsMuxerWindow::onSavedParamChanged);

  connect(&proc, &QProcess::readyReadStandardOutput, this,
          &TsMuxerWindow::readFromStdout);
  connect(&proc, &QProcess::readyReadStandardError, this,
          &TsMuxerWindow::readFromStderr);
  connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &TsMuxerWindow::onProcessFinished);
  connect(&proc, QOverload<QProcess::ProcessError>::of(&QProcess::error), this,
          &TsMuxerWindow::onProcessError);

  ui.DiskLabel->setVisible(false);
  ui.DiskLabelEdit->setVisible(false);

  ui.label_Donate->installEventFilter(this);

  ui.langComboBox->addItem("und (Undetermined)");
  ui.langComboBox->addItem("--------- common ---------");
  for (int i = 0; i < sizeof(shortLangList) / 2 / sizeof(char *); ++i) {
    const char *addr = shortLangList[i][0];
    ui.langComboBox->addItem(QString(shortLangList[i][0]) + " (" +
                                 shortLangList[i][1] + ")",
                             (qlonglong)(void *)addr);
  }
  ui.langComboBox->addItem("---------- all ----------");
  for (int i = 0; i < sizeof(fullLangList) / 2 / sizeof(char *); ++i) {
    const char *addr = fullLangList[i][0];
    ui.langComboBox->addItem(QString(fullLangList[i][0]) + " (" +
                                 fullLangList[i][1] + ")",
                             (qlonglong)(void *)addr);
  }
  trackLVItemSelectionChanged();

  ui.trackSplitter->setStretchFactor(0, 10);
  ui.trackSplitter->setStretchFactor(1, 100);

  writeSettings();
}

TsMuxerWindow::~TsMuxerWindow() {
  delete sound;
  delete tempSoundFile;
}

void TsMuxerWindow::getCodecInfo() {
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
  for (int i = 0; i < procStdOutput.size(); ++i) {
    p = procStdOutput[i].indexOf("Track ID:    ");
    if (p >= 0) {
      lastTrackID =
          procStdOutput[i].mid(QString("Track ID:    ").length()).toInt();
      codecList << QtvCodecInfo();
      codecInfo = &(codecList.back());
      codecInfo->trackID = lastTrackID;
    }

    p = procStdOutput[i].indexOf("Stream type: ");
    if (p >= 0) {
      if (lastTrackID == 0) {
        codecList << QtvCodecInfo();
        codecInfo = &(codecList.back());
      }
      codecInfo->descr = "Can't detect codec";
      codecInfo->displayName =
          procStdOutput[i].mid(QString("Stream type: ").length());
      if (codecInfo->displayName != "H.264" &&
          codecInfo->displayName != "MVC") {
        codecInfo->addSEIMethod = 0;
        codecInfo->addSPS = false;
      }
      lastTrackID = 0;
    }
    p = procStdOutput[i].indexOf("Stream ID:   ");
    if (p >= 0)
      codecInfo->programName =
          procStdOutput[i].mid(QString("Stream ID:   ").length());
    p = procStdOutput[i].indexOf("Stream info: ");
    if (p >= 0)
      codecInfo->descr =
          procStdOutput[i].mid(QString("Stream info: ").length());
    p = procStdOutput[i].indexOf("Stream lang: ");
    if (p >= 0)
      codecInfo->lang = procStdOutput[i].mid(QString("Stream lang: ").length());

    p = procStdOutput[i].indexOf("Stream delay: ");
    if (p >= 0) {
      tmpStr = procStdOutput[i].mid(QString("Stream delay: ").length());
      codecInfo->delay = tmpStr.toInt();
    }

    p = procStdOutput[i].indexOf("subTrack: ");
    if (p >= 0) {
      tmpStr = procStdOutput[i].mid(QString("subTrack: ").length());
      codecInfo->subTrack = tmpStr.toInt();
    }

    p = procStdOutput[i].indexOf("Secondary: 1");
    if (p == 0) {
      codecInfo->isSecondary = true;
    }
    p = procStdOutput[i].indexOf("Unselected: 1");
    if (p == 0) {
      codecInfo->enabledByDefault = false;
    }

    if (procStdOutput[i].startsWith("Error: ")) {
      tmpStr = procStdOutput[i].mid(QString("Error: ").length());
      QMessageBox msgBox(this);
      msgBox.setWindowTitle("Not supported");
      msgBox.setText(tmpStr);
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.exec();
    } else if (procStdOutput[i].startsWith("File #")) {
      tmpStr = procStdOutput[i].mid(17); // todo: check here
      mplsFileList << MPLSFileInfo(tmpStr, 0.0);
    } else if (procStdOutput[i].startsWith("Duration:")) {
      tmpStr = procStdOutput[i].mid(10); // todo: check here
      if (!mplsFileList.empty()) {
        mplsFileList.last().duration = timeToFloat(tmpStr);
      } else {
        fileDuration = timeToFloat(tmpStr);
      }
    } else if (procStdOutput[i].startsWith("Base view: ")) {
      tmpStr = procStdOutput[i].mid(11); // todo: check here
      ui.rightEyeCheckBox->setChecked(tmpStr.trimmed() == "right-eye");
    } else if (procStdOutput[i].startsWith("start-time: ")) {
      tmpStr = procStdOutput[i].mid(12);
      if (tmpStr.contains(':')) {
        double secondsF = timeToFloat(tmpStr);
        ui.muxTimeEdit->setTime(qTimeFromFloat(secondsF));
      } else {
        ui.muxTimeClock->setValue(tmpStr.toInt());
      }
    }

    p = procStdOutput[i].indexOf("Marks: ");
    if (p == 0) {
      if (firstMark) {
        firstMark = false;
        ui.radioButtonCustomChapters->setChecked(true);
      }
      QStringList stringList = procStdOutput[i].mid(7).split(' ');
      for (int k = 0; k < stringList.size(); ++k)
        if (!stringList[k].isEmpty())
          chapters << timeToFloat(stringList[k]);
    }
  }
  if (fileDuration == 0 && !mplsFileList.isEmpty()) {
    foreach (const MPLSFileInfo &mplsFile, mplsFileList)
      fileDuration += mplsFile.duration;
  }

  m_updateMeta = true;
  if (codecList.isEmpty()) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Unsupported format");
    msgBox.setText(QString("Can't detect stream type. File name: \"") +
                   newFileName + "\"");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    return;
  }
  updateMetaLines();
  emit codecListReady();
}

int TsMuxerWindow::findLangByCode(const QString &code) {
  QString addr;
  for (int i = 0; i < ui.langComboBox->count(); ++i) {
    addr = ui.langComboBox->itemData(i).toString();
    if (!addr.isEmpty() && code == addr) {
      return i;
    }
  }
  return 0;
}

QtvCodecInfo *TsMuxerWindow::getCurrentCodec() {
  if (ui.trackLV->currentRow() == -1)
    return nullptr;
  long iCodec = long(ui.trackLV->item(ui.trackLV->currentRow(), 0)
                         ->data(Qt::UserRole)
                         .toLongLong());
  return iCodec ? (QtvCodecInfo *)(void *)iCodec : nullptr;
}

void TsMuxerWindow::onVideoComboBoxChanged(int index) {
  Q_UNUSED(index);
  if (disableUpdatesCnt)
    return;
  QtvCodecInfo *codecInfo = getCurrentCodec();
  if (!codecInfo)
    return;
  codecInfo->fpsText = ui.comboBoxFPS->itemText(ui.comboBoxFPS->currentIndex());
  codecInfo->levelText =
      ui.comboBoxLevel->itemText(ui.comboBoxLevel->currentIndex());
  codecInfo->arText = ui.comboBoxAR->itemText(ui.comboBoxAR->currentIndex());
  updateMetaLines();
}

void TsMuxerWindow::onVideoCheckBoxChanged(int state) {
  Q_UNUSED(state);
  if (disableUpdatesCnt)
    return;
  QtvCodecInfo *codecInfo = getCurrentCodec();
  if (codecInfo == nullptr)
    return;
  ui.comboBoxFPS->setEnabled(ui.checkFPS->isChecked());
  codecInfo->checkFPS = ui.checkFPS->isChecked();
  ui.comboBoxLevel->setEnabled(ui.checkBoxLevel->isChecked());
  codecInfo->checkLevel = ui.checkBoxLevel->isChecked();
  codecInfo->addSEIMethod = ui.comboBoxSEI->currentIndex();
  codecInfo->addSPS = ui.checkBoxSPS->isChecked();
  codecInfo->isSecondary = ui.checkBoxSecondaryVideo->isChecked();
  colorizeCurrentRow(codecInfo);
  updateMetaLines();
}

void TsMuxerWindow::updateCurrentColor(int dr, int dg, int db, int row) {
  if (row == -1)
    row = ui.trackLV->currentRow();
  if (row == -1)
    return;
  QColor color = ui.trackLV->palette().color(QPalette::Base);
  color.setRed(qBound(0, color.red() + dr, 255));
  color.setGreen(qBound(0, color.green() + dg, 255));
  color.setBlue(qBound(0, color.blue() + db, 255));
  for (int i = 0; i < 5; ++i) {
    QModelIndex index = ui.trackLV->model()->index(row, i);
    ui.trackLV->model()->setData(index, QBrush(color), Qt::BackgroundColorRole);
  }
}

void TsMuxerWindow::colorizeCurrentRow(const QtvCodecInfo *codecInfo,
                                       int rowIndex) {
  if (codecInfo->isSecondary)
    updateCurrentColor(-40, 0, 0, rowIndex);
  else
    updateCurrentColor(0, 0, 0, rowIndex);
}

void TsMuxerWindow::onAudioSubtitlesParamsChanged() {
  if (disableUpdatesCnt)
    return;
  QtvCodecInfo *codecInfo = getCurrentCodec();
  if (!codecInfo)
    return;
  codecInfo->bindFps = ui.checkBoxKeepFps->isChecked();
  codecInfo->dtsDownconvert = ui.dtsDwnConvert->isChecked();
  codecInfo->isSecondary = ui.secondaryCheckBox->isChecked();
  codecInfo->offsetId = ui.offsetsComboBox->currentIndex() - 1;
  QString addr =
      ui.langComboBox->itemData(ui.langComboBox->currentIndex()).toString();
  if (!addr.isEmpty()) {
    codecInfo->lang = addr;
  } else {
    codecInfo->lang.clear();
  }
  ui.trackLV->item(ui.trackLV->currentRow(), 3)->setText(codecInfo->lang);
  colorizeCurrentRow(codecInfo);

  updateMetaLines();
}

void TsMuxerWindow::onEditDelayChanged(int i) {
  Q_UNUSED(i);
  if (disableUpdatesCnt)
    return;
  QtvCodecInfo *codecInfo = getCurrentCodec();
  if (!codecInfo)
    return;
  codecInfo->delay = ui.editDelay->value();
  updateMetaLines();
}

void TsMuxerWindow::onPulldownCheckBoxChanged(int state) {
  Q_UNUSED(state);
  if (disableUpdatesCnt)
    return;
  QtvCodecInfo *codecInfo = getCurrentCodec();
  if (!codecInfo)
    return;
  if (ui.checkBoxRemovePulldown->isEnabled()) {
    if (ui.checkBoxRemovePulldown->isChecked()) {
      codecInfo->delPulldown = 1;
      if (codecInfo->fpsTextOrig == "29.97") {
        ui.checkFPS->setChecked(true);
        ui.checkFPS->setEnabled(true);
        codecInfo->checkFPS = true;
        ui.comboBoxFPS->setEnabled(true);
        ui.comboBoxFPS->setCurrentIndex(3);
        codecInfo->fpsText = "24000/1001";
        setComboBoxText(ui.comboBoxFPS, "24000/1001");
      }
    } else
      codecInfo->delPulldown = 0;
  } else
    codecInfo->delPulldown = -1;
  updateMetaLines();
}

void TsMuxerWindow::addFiles(const QList<QUrl> &files) {
  addFileList.clear();
  addFileList = files;
  addFile();
}

void TsMuxerWindow::onAddBtnClick() {
  QString fileName = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
      this, tr("Add media file"), lastInputDir, tr(fileDialogFilter)));
  if (fileName.isEmpty())
    return;
  lastInputDir = fileName;
  // disconnect();
  addFileList.clear();
  addFileList << QUrl::fromLocalFile(fileName);
  addFile();
}

void TsMuxerWindow::addFile() {
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
  connect(this, &TsMuxerWindow::tsMuxerSuccessFinished, this, &TsMuxerWindow::getCodecInfo);
  connect(this, &TsMuxerWindow::codecListReady, this, &TsMuxerWindow::continueAddFile);
  connect(this, &TsMuxerWindow::fileAdded, this, &TsMuxerWindow::addFile);
  runInMuxMode = false;
  shellExecute(
      QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) +
          QDir::separator() + "tsMuxeR",
      QStringList() << newFileName);
}

bool TsMuxerWindow::checkFileDuplicate(const QString &fileName) {
  QString t = myUnquoteStr(fileName).trimmed();
  for (int i = 0; i < ui.inputFilesLV->count(); ++i)
    if (myUnquoteStr(ui.inputFilesLV->item(i)->data(FileNameRole).toString()) ==
        t) {
      QMessageBox msgBox(this);
      msgBox.setWindowTitle("File already exist");
      msgBox.setText("File \"" + fileName + "\" already exist");
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.exec();
      return false;
    }
  return true;
}

void TsMuxerWindow::setComboBoxText(QComboBox *comboBox, const QString &text) {
  for (int k = 0; k < comboBox->count(); ++k) {
    if (comboBox->itemText(k) == text) {
      comboBox->setCurrentIndex(k);
      return;
    }
  }
  comboBox->addItem(text);
  comboBox->setCurrentIndex(comboBox->count() - 1);
}

void TsMuxerWindow::trackLVItemSelectionChanged() {
  if (disableUpdatesCnt)
    return;
  while (ui.tabWidgetTracks->count())
    ui.tabWidgetTracks->removeTab(0);
  if (ui.trackLV->currentRow() == -1) {
    ui.tabWidgetTracks->addTab(ui.tabSheetFake, TI_DEFAULT_TAB_NAME);
    return;
  }
  QtvCodecInfo *codecInfo = getCurrentCodec();
  if (!codecInfo)
    return;
  disableUpdatesCnt++;
  if (ui.trackLV->rowCount() >= 1) {
    if (isVideoCodec(codecInfo->displayName)) {
      ui.tabWidgetTracks->addTab(ui.tabSheetVideo, TI_DEFAULT_TAB_NAME);

      ui.checkFPS->setChecked(codecInfo->checkFPS);
      ui.checkBoxLevel->setChecked(codecInfo->checkLevel);
      ui.comboBoxFPS->setEnabled(ui.checkFPS->isChecked());
      ui.comboBoxLevel->setEnabled(ui.checkBoxLevel->isChecked());
      ui.comboBoxSEI->setCurrentIndex(codecInfo->addSEIMethod);
      ui.checkBoxSecondaryVideo->setChecked(codecInfo->isSecondary);
      ui.checkBoxSPS->setChecked(codecInfo->addSPS);
      ui.checkBoxRemovePulldown->setChecked(codecInfo->delPulldown == 1);
      ui.checkBoxRemovePulldown->setEnabled(codecInfo->delPulldown >= 0);

      setComboBoxText(ui.comboBoxFPS, codecInfo->fpsText);
      if (!codecInfo->arText.isEmpty())
        setComboBoxText(ui.comboBoxAR, codecInfo->arText);
      else
        ui.comboBoxAR->setCurrentIndex(0);
      ui.checkBoxLevel->setEnabled(codecInfo->displayName == "H.264" ||
                                   codecInfo->displayName == "MVC");
      if (ui.checkBoxLevel->isEnabled())
        setComboBoxText(ui.comboBoxLevel, codecInfo->levelText);
      else
        ui.comboBoxLevel->setCurrentIndex(0);
      ui.comboBoxSEI->setEnabled(ui.checkBoxLevel->isEnabled());
      ui.checkBoxSPS->setEnabled(ui.checkBoxLevel->isEnabled());
      ui.comboBoxAR->setEnabled(codecInfo->displayName == "MPEG-2");
      ui.comboBoxAR->setEnabled(true);
      ui.labelAR->setEnabled(ui.comboBoxAR->isEnabled());
      ui.checkBoxSecondaryVideo->setEnabled(codecInfo->displayName != "MVC");
    } else {
      ui.tabWidgetTracks->addTab(ui.tabSheetAudio, TI_DEFAULT_TAB_NAME);
      if (codecInfo->displayName == "LPCM")
        ui.tabWidgetTracks->addTab(ui.demuxLpcmOptions, TI_DEMUX_TAB_NAME);

      if (codecInfo->displayName == "DTS-HD")
        ui.dtsDwnConvert->setText("Downconvert DTS-HD to DTS");
      else if (codecInfo->displayName == "TRUE-HD")
        ui.dtsDwnConvert->setText("Downconvert TRUE-HD to AC3");
      else if (codecInfo->displayName == "E-AC3 (DD+)")
        ui.dtsDwnConvert->setText("Downconvert E-AC3 to AC3");
      else
        ui.dtsDwnConvert->setText("Downconvert HD audio");
      ui.dtsDwnConvert->setEnabled(codecInfo->displayName == "DTS-HD" ||
                                   codecInfo->displayName == "TRUE-HD" ||
                                   codecInfo->displayName == "E-AC3 (DD+)");
      ui.secondaryCheckBox->setEnabled(
          codecInfo->descr.contains("(DTS Express)") ||
          codecInfo->displayName == "E-AC3 (DD+)");

      if (!ui.secondaryCheckBox->isEnabled())
        ui.secondaryCheckBox->setChecked(false);

      ui.langComboBox->setCurrentIndex(findLangByCode(codecInfo->lang));
      ui.offsetsComboBox->setCurrentIndex(codecInfo->offsetId + 1);
      ui.dtsDwnConvert->setVisible(codecInfo->displayName != "PGS" &&
                                   codecInfo->displayName != "SRT");
      ui.secondaryCheckBox->setVisible(ui.dtsDwnConvert->isVisible());

      bool isPGS = codecInfo->displayName == "PGS";
      ui.checkBoxKeepFps->setVisible(isPGS);
      ui.offsetsLabel->setVisible(isPGS);
      ui.offsetsComboBox->setVisible(isPGS);

      ui.imageSubtitles->setVisible(!ui.dtsDwnConvert->isVisible());
      ui.imageAudio->setVisible(ui.dtsDwnConvert->isVisible());

      ui.editDelay->setValue(codecInfo->delay);
      ui.dtsDwnConvert->setChecked(codecInfo->dtsDownconvert);
      ui.secondaryCheckBox->setChecked(codecInfo->isSecondary);
      ui.checkBoxKeepFps->setChecked(codecInfo->bindFps);
      ui.editDelay->setEnabled(!ui.radioButtonDemux->isChecked());
    }
  }

  disableUpdatesCnt--;
  trackLVItemChanged(0);
}

void TsMuxerWindow::trackLVItemChanged(QTableWidgetItem *item) {
  if (disableUpdatesCnt > 0)
    return;

  Q_UNUSED(item);
  updateMetaLines();
  ui.moveupBtn->setEnabled(ui.trackLV->currentItem() != 0);
  ui.movedownBtn->setEnabled(ui.trackLV->currentItem() != 0);
  ui.removeTrackBtn->setEnabled(ui.trackLV->currentItem() != 0);
  if (ui.trackLV->rowCount() == 0)
    oldFileName.clear();

  disableUpdatesCnt++;
  bool checkedExist = false;
  bool uncheckedExist = false;
  for (int i = 0; i < ui.trackLV->rowCount(); ++i) {
    if (ui.trackLV->item(i, 0)->checkState() == Qt::Checked)
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

void TsMuxerWindow::inputFilesLVChanged() {
  QListWidgetItem *itm = ui.inputFilesLV->currentItem();
  if (!itm) {
    ui.btnAppend->setEnabled(false);
    ui.removeFile->setEnabled(false);
    return;
  }
  ui.btnAppend->setEnabled(itm->data(MplsItemRole).toInt() != MPLS_M2TS &&
                           ui.buttonMux->isEnabled());
  ui.removeFile->setEnabled(itm->data(MplsItemRole).toInt() != MPLS_M2TS);
}

void TsMuxerWindow::modifyOutFileName(const QString fileName) {
  QFileInfo fi(unquoteStr(fileName));
  QString name = fi.completeBaseName();

  QString existingName = QDir::toNativeSeparators(ui.outFileName->text());
  QFileInfo fiDst(existingName);
  if (fiDst.completeBaseName() == "default") {
    QString dstPath;
    if (existingName.contains(QDir::separator()))
      dstPath = QDir::toNativeSeparators(fiDst.absolutePath());
    else
      dstPath = getOutputDir();

    if (ui.radioButtonTS->isChecked())
      ui.outFileName->setText(dstPath + QDir::separator() + name + ".ts");
    else if (ui.radioButtonM2TS->isChecked())
      ui.outFileName->setText(dstPath + QDir::separator() + name + ".m2ts");
    else if (ui.radioButtonBluRayISO->isChecked())
      ui.outFileName->setText(dstPath + QDir::separator() + name + ".iso");
    else if (ui.radioButtonBluRayISOUHD->isChecked())
      ui.outFileName->setText(dstPath + QDir::separator() + name + ".iso");
    else
      ui.outFileName->setText(dstPath);
    if (ui.outFileName->text() == fileName)
      ui.outFileName->setText(dstPath + QDir::separator() + name + "_new." +
                              fi.suffix());
  }
}

void TsMuxerWindow::continueAddFile() {
  double fps;
  double level;
  disableUpdatesCnt++;
  bool firstWarn = true;
  int firstAddedIndex = -1;
  for (int i = 0; i < codecList.size(); ++i) {
    if (codecList[i].displayName == "PGS (depended view)")
      continue;

    QtvCodecInfo info = codecList[i];
    if (info.displayName.isEmpty()) {
      QMessageBox msgBox(this);
      msgBox.setWindowTitle("Unsupported format");
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setStandardButtons(QMessageBox::Ok);
      if (codecList.size() == 0) {
        msgBox.setText(QString("Unsupported format or all tracks are not "
                               "recognized. File name: \"") +
                       newFileName + "\"");
        msgBox.exec();
        disableUpdatesCnt--;
        return;
      } else {
        if (firstWarn) {
          msgBox.setText(QString("Some tracks not recognized. This tracks was "
                                 "ignored. File name: \"") +
                         newFileName + "\"");
          msgBox.exec();
          firstWarn = false;
        }
        continue;
      }
    }
    if (mplsFileList.isEmpty())
      info.fileList << newFileName;
    else {
      info.fileList << mplsFileList[0].name;
      QFileInfo fileInfo(unquoteStr(newFileName));
      // info.mplsFile = fileInfo.baseName();
      info.mplsFiles << unquoteStr(newFileName);
    }
    if (info.descr.indexOf("not found") >= 0) {
      fps = 23.976;
      info.checkFPS = true;
    } else
      fps = extractFloatFromDescr(info.descr, "Frame rate: ");
    info.width = extractFloatFromDescr(info.descr, "Resolution: ") + 0.5;
    info.height =
        extractFloatFromDescr(info.descr, QString::number(info.width) + ":") +
        0.5;
    if (qAbs(fps - 23.976) < EPS)
      info.fpsText = "24000/1001";
    else if (qAbs(fps - 29.97) < EPS)
      info.fpsText = "30000/1001";
    else
      info.fpsText = myFloatToStr(fps);
    info.fpsTextOrig = myFloatToStr(fps);
    level = extractFloatFromDescr(info.descr, "@");
    info.levelText = myFloatToStr(level);
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

    ui.trackLV->setRowCount(ui.trackLV->rowCount() + 1);
    ui.trackLV->setRowHeight(ui.trackLV->rowCount() - 1, 18);
    QTableWidgetItem *item = new QTableWidgetItem("");
    item->setCheckState(info.enabledByDefault ? Qt::Checked : Qt::Unchecked);
    QtvCodecInfo *__info = new QtvCodecInfo();
    *__info = info;
    item->setData(Qt::UserRole, (qlonglong)(void *)__info);
    ui.trackLV->setCurrentItem(item);

    ui.trackLV->setItem(ui.trackLV->rowCount() - 1, 0, item);
    item = new QTableWidgetItem(newFileName);
    item->setFlags(item->flags() & (~Qt::ItemIsEditable));
    ui.trackLV->setItem(ui.trackLV->rowCount() - 1, 1, item);
    item = new QTableWidgetItem(info.displayName);
    item->setFlags(item->flags() & (~Qt::ItemIsEditable));
    ui.trackLV->setItem(ui.trackLV->rowCount() - 1, 2, item);
    item = new QTableWidgetItem(info.lang);
    item->setFlags(item->flags() & (~Qt::ItemIsEditable));
    ui.trackLV->setItem(ui.trackLV->rowCount() - 1, 3, item);
    item = new QTableWidgetItem(info.descr);
    item->setFlags(item->flags() & (~Qt::ItemIsEditable));
    ui.trackLV->setItem(ui.trackLV->rowCount() - 1, 4, item);
    if (firstAddedIndex == -1)
      firstAddedIndex = ui.trackLV->rowCount() - 1;
    colorizeCurrentRow(&info, ui.trackLV->rowCount() - 1);
  }
  if (firstAddedIndex >= 0) {
    ui.trackLV->setRangeSelected(
        QTableWidgetSelectionRange(firstAddedIndex, 0,
                                   ui.trackLV->rowCount() - 1, 4),
        true);
    ui.trackLV->setCurrentCell(firstAddedIndex, 0);
  }

  QString displayName = newFileName;
  if (fileDuration > 0)
    displayName += QString(" (%1)").arg(floatToTime(fileDuration));
  ui.inputFilesLV->addItem(displayName);

  int index = ui.inputFilesLV->count() - 1;
  QListWidgetItem *fileItem = ui.inputFilesLV->item(index);
  if (!mplsFileList.empty())
    fileItem->setData(MplsItemRole, MPLS_PRIMARY);
  fileItem->setData(FileNameRole, newFileName);
  QVariant v;
  v.setValue<ChapterList>(chapters);
  fileItem->setData(ChaptersRole, v);
  fileItem->setData(FileDurationRole, fileDuration);
  chapters.clear();
  fileDuration = 0.0;

  ui.inputFilesLV->setCurrentItem(fileItem);
  if (mplsFileList.size() > 0)
    doAppendInt(mplsFileList[0].name, newFileName, mplsFileList[0].duration,
                false, MPLS_M2TS);
  for (int mplsCnt = 1; mplsCnt < mplsFileList.size(); ++mplsCnt)
    doAppendInt(mplsFileList[mplsCnt].name, mplsFileList[mplsCnt - 1].name,
                mplsFileList[mplsCnt].duration, false, MPLS_M2TS);
  ui.inputFilesLV->setCurrentItem(ui.inputFilesLV->item(index));
  updateMetaLines();
  ui.moveupBtn->setEnabled(ui.trackLV->currentRow() >= 0);
  ui.movedownBtn->setEnabled(ui.trackLV->currentRow() >= 0);
  ui.removeTrackBtn->setEnabled(ui.trackLV->currentRow() >= 0);
  if (!outFileNameModified) {
    modifyOutFileName(newFileName);
    outFileNameModified = true;
  }
  updateMaxOffsets();
  updateCustomChapters();
  disableUpdatesCnt--;
  trackLVItemSelectionChanged();
  emit fileAdded();
}

void TsMuxerWindow::updateCustomChapters() {
  QSet<qint64> chaptersSet;
  double prevDuration = 0.0;
  double offset = 0.0;
  for (int i = 0; i < ui.inputFilesLV->count(); ++i) {
    QListWidgetItem *item = ui.inputFilesLV->item(i);
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

  ui.memoChapters->clear();
  QList<qint64> mergedChapterList = chaptersSet.toList();
  qSort(mergedChapterList);
  foreach (qint64 chapter, mergedChapterList)
    ui.memoChapters->insertPlainText(floatToTime(chapter / 1000000.0) +
                                     QString('\n'));
}

void splitLines(const QString &str, QList<QString> &strList) {
  strList = str.split('\n');
  for (int i = 0; i < strList.size(); ++i) {
    if (!strList[i].isEmpty() && strList[i].at(0) == '\r')
      strList[i] = strList[i].mid(1);
    else if (!strList[i].isEmpty() &&
             strList[i].at(strList[i].length() - 1) == '\r')
      strList[i] = strList[i].mid(0, strList[i].length() - 1);
  }
}

void TsMuxerWindow::addLines(const QByteArray &arr, QList<QString> &outList,
                             bool isError) {
  QString str = QString::fromLocal8Bit(arr);
  QList<QString> strList;
  splitLines(str, strList);
  QString text;
  for (int i = 0; i < strList.size(); ++i) {
    if (strList[i].trimmed().isEmpty())
      continue;
    int p = strList[i].indexOf("% complete");
    if (p >= 0) {
      int numStartPos = 0;
      for (int j = 0; j < strList[i].length(); ++j) {
        if (strList[i].at(j) >= '0' && strList[i].at(j) <= '9') {
          numStartPos = j;
          break;
        }
      }
      QString progress = strList[i].mid(numStartPos, p - numStartPos);
      float tmpVal = progress.toFloat();
      if (qAbs(tmpVal) > 0 && runInMuxMode)
        muxForm.setProgress(
            int(double(tmpVal * 10) + 0.5)); // todo: uncomment here
    } else {
      if (runInMuxMode) {
        if (!text.isEmpty())
          text += '\n';
        text += strList[i];
      } else
        outList << strList[i];
    }
  }
  if (runInMuxMode && !text.isEmpty()) {
    if (isError)
      muxForm.addStdErrLine(text);
    else
      muxForm.addStdOutLine(text);
  }
}

void TsMuxerWindow::readFromStdout() {
  addLines(proc.readAllStandardOutput(), procStdOutput, false);
}

void TsMuxerWindow::readFromStderr() {
  QByteArray data(proc.readAllStandardError());
  addLines(data, procErrOutput, true);
}

void TsMuxerWindow::myPlaySound(const QString &fileName) {
  QFile file(fileName);
  if (!file.open(QFile::ReadOnly))
    return;
  qint64 fSize = file.size();
  char *data = new char[fSize];
  qint64 readed = file.read(data, fSize);
  QFileInfo fi(fileName);
  QString outFileName = QDir::toNativeSeparators(QDir::tempPath()) +
                        QDir::separator() + "tsMuxeR_" + fi.fileName();
  delete sound;
  delete tempSoundFile;
  tempSoundFile = new QTemporaryFile(outFileName);

  if (!tempSoundFile->open()) {
    delete[] data;
    delete tempSoundFile;
    tempSoundFile = 0;
    return;
  }

  tempSoundFile->write(data, readed);
  QString tmpFileName = tempSoundFile->fileName();
  tempSoundFile->close();
  sound = new QSound(tmpFileName);
  sound->play();
  file.close();
  delete[] data;
}

void TsMuxerWindow::onProcessFinished(int exitCode,
                                      QProcess::ExitStatus exitStatus) {
  processFinished = true;
  if (!metaName.isEmpty()) {
    QFile::remove(metaName);
    metaName.clear();
    if (ui.checkBoxSound->isChecked()) {
      if (exitCode == 0)
        myPlaySound(":/sounds/success.wav");
      else
        myPlaySound(":/sounds/fail.wav");
    }
  }
  processExitCode = exitCode;
  processExitStatus = exitStatus;
  muxForm.muxFinished(processExitCode,
                      ui.radioButtonDemux->isChecked() ? "Demux" : "Mux");
  ui.buttonMux->setEnabled(true);
  ui.addBtn->setEnabled(true);
  inputFilesLVChanged();
  if (processExitCode == 0)
    emit tsMuxerSuccessFinished();
}

void TsMuxerWindow::onProcessError(QProcess::ProcessError error) {
  processExitCode = -1;
  if (!metaName.isEmpty()) {
    QFile::remove(metaName);
    metaName.clear();
  }
  processFinished = true;
  QMessageBox msgBox(this);
  QString text;
  msgBox.setWindowTitle("tsMuxeR error");
  switch (error) {
  case QProcess::FailedToStart:
    msgBox.setText("tsMuxeR not found!");
    ui.buttonMux->setEnabled(true);
    ui.addBtn->setEnabled(true);
    inputFilesLVChanged();
    break;
  case QProcess::Crashed:
    // process killed
    if (runInMuxMode)
      return;
    for (int i = 0; i < procErrOutput.size(); ++i) {
      if (i > 0)
        text += '\n';
      text += procErrOutput[i];
    }
    msgBox.setText(text);
    break;
  default:
    msgBox.setText("Can't execute tsMuxeR!");
    break;
  }
  msgBox.setIcon(QMessageBox::Critical);
  msgBox.setStandardButtons(QMessageBox::Ok);
  msgBox.exec();
}

void TsMuxerWindow::shellExecute(const QString &process,
                                 const QStringList &args) {
  ui.buttonMux->setEnabled(false);
  procStdOutput.clear();
  procErrOutput.clear();
  processFinished = false;
  processExitCode = -1;
  proc.start(process, args);
  if (muxForm.isVisible())
    muxForm.setProcess(&proc);
}

void TsMuxerWindow::doAppendInt(const QString &fileName,
                                const QString &parentFileName, double duration,
                                bool doublePrefix, MplsType mplsRole) {
  QString itemName = FILE_JOIN_PREFIX;
  if (doublePrefix)
    itemName += FILE_JOIN_PREFIX;
  itemName += fileName;
  if (duration > 0)
    itemName += QString(" ( %1)").arg(floatToTime(duration));
  QListWidgetItem *item = new QListWidgetItem(itemName);
  ui.inputFilesLV->insertItem(ui.inputFilesLV->currentRow() + 1, item);
  item->setData(MplsItemRole, mplsRole);
  item->setData(FileNameRole, fileName);
  if (duration > 0)
    item->setData(FileDurationRole, duration);
  QVariant v;
  v.setValue<ChapterList>(chapters);
  item->setData(ChaptersRole, v);

  ui.inputFilesLV->setCurrentItem(item);

  for (int i = 0; i < ui.trackLV->rowCount(); ++i) {
    long data = ui.trackLV->item(i, 0)->data(Qt::UserRole).toLongLong();
    if (!data)
      continue;
    QtvCodecInfo *info = (QtvCodecInfo *)(void *)data;
    if (mplsRole == MPLS_PRIMARY) {
      for (int j = 0; j < info->mplsFiles.size(); ++j) {
        if (info->mplsFiles[j] == parentFileName) {
          info->mplsFiles.insert(j + 1, fileName);
          break;
        }
      }
    } else {
      for (int j = 0; j < info->fileList.size(); ++j) {
        if (info->fileList[j] == parentFileName) {
          info->fileList.insert(j + 1, fileName);
          break;
        }
      }
    }
  }
}

bool TsMuxerWindow::isVideoCropped() {
  for (int i = 0; i < ui.trackLV->rowCount(); ++i) {
    long iCodec = ui.trackLV->item(i, 0)->data(Qt::UserRole).toLongLong();
    if (!iCodec)
      continue;
    QtvCodecInfo *codecInfo = (QtvCodecInfo *)(void *)iCodec;
    if (isVideoCodec(codecInfo->displayName)) {
      if (codecInfo->height < 1080 && codecInfo->height != 720 &&
          codecInfo->height != 576 && codecInfo->height != 480)
        return true;
    }
  }
  return false;
}

bool TsMuxerWindow::isDiskOutput() const {
  return ui.radioButtonAVCHD->isChecked() ||
         ui.radioButtonBluRay->isChecked() ||
         ui.radioButtonBluRayISO->isChecked() ||
         ui.radioButtonBluRayISOUHD->isChecked();
}

QString TsMuxerWindow::getMuxOpts() {
  QString rez = "MUXOPT --no-pcr-on-video-pid ";
  if (ui.checkBoxNewAudioPes->isChecked())
    rez += "--new-audio-pes ";
  if (ui.radioButtonBluRay->isChecked())
    rez += "--blu-ray ";
  else if (ui.radioButtonBluRayISO->isChecked()) {
    rez += "--blu-ray ";
    if (!ui.DiskLabelEdit->text().isEmpty())
      rez += QString("--label=\"%1\" ").arg(ui.DiskLabelEdit->text());
  } else if (ui.radioButtonBluRayUHD->isChecked())
    rez += "--blu-ray-v3 ";
  else if (ui.radioButtonBluRayISOUHD->isChecked()) {
    rez += "--blu-ray-v3 ";
    if (!ui.DiskLabelEdit->text().isEmpty())
      rez += QString("--label=\"%1\" ").arg(ui.DiskLabelEdit->text());
  } else if (ui.radioButtonAVCHD->isChecked())
    rez += "--avchd ";
  else if (ui.radioButtonDemux->isChecked())
    rez += "--demux ";
  if (ui.checkBoxCBR->isChecked())
    rez += "--cbr --bitrate=" +
           toFixedDecPoint(QString::number(ui.editCBRBitrate->value(), 'f', 3));
  else {
    rez += "--vbr ";
    if (ui.checkBoxRVBR->isChecked()) {
      rez +=
          QString("--minbitrate=") +
          toFixedDecPoint(QString::number(ui.editMinBitrate->value(), 'f', 3));
      rez +=
          QString(" --maxbitrate=") +
          toFixedDecPoint(QString::number(ui.editMaxBitrate->value(), 'f', 3));
    }
  }
  // if (!ui.checkBoxuseAsynIO->isChecked())
  //	rez += " --no-asyncio";
  if (isDiskOutput()) {
    if (ui.checkBoxBlankPL->isChecked() && isVideoCropped()) {
      rez += " --insertBlankPL";
      if (ui.BlackplaylistCombo->value() != 1900)
        rez += QString(" --blankOffset=") +
               QString::number(ui.BlackplaylistCombo->value());
    }
    if (ui.spinBoxMplsNum->value() > 0)
      rez += " --mplsOffset=" + QString::number(ui.spinBoxMplsNum->value());
    if (ui.spinBoxM2tsNum->value() > 0)
      rez += " --m2tsOffset=" + QString::number(ui.spinBoxM2tsNum->value());
  }
  if (isDiskOutput()) {
    if (ui.radioButtonAutoChapter->isChecked())
      rez +=
          " --auto-chapters=" + QString::number(ui.spinEditChapterLen->value());
    if (ui.radioButtonCustomChapters->isChecked()) {
      QString custChapStr;
      QList<QString> lines;
      splitLines(ui.memoChapters->toPlainText(), lines);
      for (int i = 0; i < lines.size(); ++i) {
        QString tmpStr = lines[i].trimmed();
        if (!tmpStr.isEmpty()) {
          if (!custChapStr.isEmpty())
            custChapStr += ';';
          custChapStr += tmpStr;
        }
      }
      rez += QString(" --custom-chapters=") + custChapStr;
    }
  }
  if (ui.splitByDuration->isChecked())
    rez += QString(" --split-duration=") + ui.spinEditSplitDuration->text();
  if (ui.splitBySize->isChecked())
    rez += QString(" --split-size=") +
           myFloatToStr(myStrToFloat(ui.editSplitSize->text())) +
           ui.comboBoxMeasure->currentText();

  int startCut = qTimeToMsec(ui.cutStartTimeEdit->time());
  int endCut = qTimeToMsec(ui.cutEndTimeEdit->time());
  if (startCut > 0 && ui.checkBoxCut->isChecked())
    rez += QString(" --cut-start=%1ms").arg(startCut);
  if (endCut > 0 && ui.checkBoxCut->isChecked())
    rez += QString(" --cut-end=%1ms").arg(endCut);

  int vbvLen = ui.editVBVLen->value();
  if (vbvLen > 0)
    rez += " --vbv-len=" + QString::number(vbvLen);

  if (isDiskOutput() && ui.rightEyeCheckBox->isChecked())
    rez += " --right-eye";
  /*
  QString muxTimeStr = ui.muxTimeEdit->time().toString("hh:mm:ss.zzz");
  if (muxTimeStr != "00:00:00.000")
      rez += " --start-time=" + muxTimeStr;
  */
  if (ui.muxTimeClock->value())
    rez += QString(" --start-time=%1").arg(ui.muxTimeClock->value());
  return rez;
}

int getCharsetCode(const QString &name) {
  Q_UNUSED(name);
  return 0; // todo: refactor this function
}

double TsMuxerWindow::getRendererAnimationTime() const {
  switch (ui.comboBoxAnimation->currentIndex()) {
  case 1:
    return 0.1; // fast
    break;
  case 2:
    return 0.25; // medium
    break;
  case 3:
    return 0.5; // slow
    break;
  case 4:
    return 1.0; // very slow
  }
  return 0.0;
}

void TsMuxerWindow::setRendererAnimationTime(double value) {
  int index = 0;
  if (doubleCompare(value, 0.1))
    index = 1;
  else if (doubleCompare(value, 0.25))
    index = 2;
  else if (doubleCompare(value, 0.5))
    index = 3;
  else if (doubleCompare(value, 1.0))
    index = 4;

  ui.comboBoxAnimation->setCurrentIndex(index);
}

QString TsMuxerWindow::getSrtParams() {
  QString rez;
  if (ui.listViewFont->rowCount() < 5)
    return rez;
  rez = QString(",font-name=\"") + ui.listViewFont->item(0, 1)->text();
  rez += QString("\",font-size=") + ui.listViewFont->item(1, 1)->text();
  rez += QString(",font-color=") + ui.listViewFont->item(2, 1)->text();
  int charsetCode = getCharsetCode(ui.listViewFont->item(3, 1)->text());
  if (charsetCode)
    rez += QString(",font-charset=") + QString::number(charsetCode);
  if (ui.lineSpacing->value() != 1.0)
    rez += ",line-spacing=" + QString::number(ui.lineSpacing->value());

  if (ui.listViewFont->item(4, 1)->text().indexOf("Italic") >= 0)
    rez += ",font-italic";
  if (ui.listViewFont->item(4, 1)->text().indexOf("Bold") >= 0)
    rez += ",font-bold";
  if (ui.listViewFont->item(4, 1)->text().indexOf("Underline") >= 0)
    rez += ",font-underline";
  if (ui.listViewFont->item(4, 1)->text().indexOf("Strikeout") >= 0)
    rez += ",font-strikeout";
  rez += QString(",bottom-offset=") +
         QString::number(ui.spinEditOffset->value()) +
         ",font-border=" + QString::number(ui.spinEditBorder->value());
  if (ui.rbhLeft->isChecked())
    rez += ",text-align=left";
  else if (ui.rbhRight->isChecked())
    rez += ",text-align=right";
  else
    rez += ",text-align=center";

  double animationTime = getRendererAnimationTime();
  if (animationTime > 0.0)
    rez += QString(",fadein-time=%1,fadeout-time=%1").arg(animationTime);

  for (int i = 0; i < ui.trackLV->rowCount(); ++i) {
    if (ui.trackLV->item(i, 0)->checkState() == Qt::Checked) {
      long iCodec = ui.trackLV->item(i, 0)->data(Qt::UserRole).toLongLong();
      if (!iCodec)
        continue;
      QtvCodecInfo *codecInfo = (QtvCodecInfo *)(void *)iCodec;
      if (isVideoCodec(codecInfo->displayName)) {
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

QString TsMuxerWindow::getFileList(QtvCodecInfo *codecInfo) {
  QString rezStr;

  if (codecInfo->mplsFiles.isEmpty()) {
    for (int i = 0; i < codecInfo->fileList.size(); ++i) {
      if (i > 0)
        rezStr += '+';
      rezStr += QString("\"") + codecInfo->fileList[i] + "\"";
    }
  } else {
    for (int i = 0; i < codecInfo->mplsFiles.size(); ++i) {
      if (i > 0)
        rezStr += '+';
      rezStr += QString("\"") + codecInfo->mplsFiles[i] + "\"";
    }
  }

  return rezStr;
}

QString cornerToStr(int corner) {
  if (corner == 0)
    return "topLeft";
  else if (corner == 1)
    return "topRight";
  else if (corner == 2)
    return "bottomRight";
  else
    return "bottomLeft";
}

QString toPipScaleStr(int scaleIdx) {
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

QString TsMuxerWindow::getVideoMetaInfo(QtvCodecInfo *codecInfo) {
  QString fpsStr;
  QString levelStr;
  QString rezStr = codecInfo->programName + ", ";

  rezStr += getFileList(codecInfo);

  if (codecInfo->checkFPS)
    fpsStr = fpsTextToFpsStr(codecInfo->fpsText);
  if (codecInfo->checkLevel)
    levelStr = QString::number(codecInfo->levelText.toDouble(), 'f', 1);

  if (ui.checkBoxCrop->isChecked() && ui.checkBoxCrop->isEnabled())
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
  if (codecInfo->arText != "Not change" && !codecInfo->arText.isEmpty())
    rezStr += QString(", ") + "ar=" + codecInfo->arText;

  if (codecInfo->isSecondary) {
    rezStr += QString(", secondary");
    rezStr += QString(", pipCorner=%1")
                  .arg(cornerToStr(ui.comboBoxPipCorner->currentIndex()));
    rezStr += QString(" ,pipHOffset=%1").arg(ui.spinBoxPipOffsetH->value());
    rezStr += QString(" ,pipVOffset=%1").arg(ui.spinBoxPipOffsetV->value());
    rezStr += QString(", pipScale=%1")
                  .arg(toPipScaleStr(ui.comboBoxPipSize->currentIndex()));
    rezStr += QString(", pipLumma=3");
  }

  return rezStr;
}

QString TsMuxerWindow::getAudioMetaInfo(QtvCodecInfo *codecInfo) {
  QString rezStr = codecInfo->programName + ", ";
  rezStr += getFileList(codecInfo);
  if (codecInfo->delay != 0)
    rezStr +=
        QString(", timeshift=") + QString::number(codecInfo->delay) + "ms";
  if (codecInfo->dtsDownconvert && codecInfo->programName == "A_DTS")
    rezStr += ", down-to-dts";
  else if (codecInfo->dtsDownconvert && codecInfo->programName == "A_AC3")
    rezStr += ", down-to-ac3";
  if (codecInfo->isSecondary)
    rezStr += ", secondary";
  return rezStr;
}

void TsMuxerWindow::updateMuxTime1() {
  QTime t = ui.muxTimeEdit->time();
  int clock = (t.hour() * 3600 + t.minute() * 60 + t.second()) * 45000ll +
              t.msec() * 45ll;
  ui.muxTimeClock->blockSignals(true);
  ui.muxTimeClock->setValue(clock);
  ui.muxTimeClock->blockSignals(false);
  updateMetaLines();
}

void TsMuxerWindow::updateMuxTime2() {
  double timeF = ui.muxTimeClock->value() / 45000.0;
  ui.muxTimeEdit->blockSignals(true);
  ui.muxTimeEdit->setTime(qTimeFromFloat(timeF));
  ui.muxTimeEdit->blockSignals(false);
  updateMetaLines();
}

void TsMuxerWindow::updateMetaLines() {
  if (!m_updateMeta || disableUpdatesCnt > 0)
    return;

  ui.memoMeta->clear();
  ui.memoMeta->append(getMuxOpts());
  QString tmpFps;
  for (int i = 0; i < ui.trackLV->rowCount(); ++i) {
    long iCodec = ui.trackLV->item(i, 0)->data(Qt::UserRole).toLongLong();
    if (!iCodec)
      continue;

    QtvCodecInfo *codecInfo = (QtvCodecInfo *)(void *)iCodec;
    if (isVideoCodec(codecInfo->displayName)) {
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

  for (int i = 0; i < ui.trackLV->rowCount(); ++i) {
    if (ui.trackLV->item(i, 0)->checkState() == Qt::Checked)
      prefix = "";
    else
      prefix = "#";
    long iCodec = ui.trackLV->item(i, 0)->data(Qt::UserRole).toLongLong();
    if (!iCodec)
      continue;
    QtvCodecInfo *codecInfo = (QtvCodecInfo *)(void *)iCodec;

    postfix.clear();
    if (codecInfo->displayName == "PGS") {
      if (codecInfo->bindFps && !tmpFps.isEmpty())
        postfix += QString(", fps=") + tmpFps;
      if (bluray3D && codecInfo->offsetId >= 0)
        postfix += QString(", 3d-plane=%1").arg(codecInfo->offsetId);
    }
    if (codecInfo->displayName == "SRT") {
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
      ui.memoMeta->append(prefix + getVideoMetaInfo(codecInfo) + postfix);
    else
      ui.memoMeta->append(prefix + getAudioMetaInfo(codecInfo) + postfix);
  }
}

void TsMuxerWindow::onFontBtnClicked() {
  bool ok;
  QFont font;
  font.setFamily(ui.listViewFont->item(0, 1)->text());
  font.setPointSize((ui.listViewFont->item(1, 1)->text()).toInt());
  font.setItalic(ui.listViewFont->item(4, 1)->text().indexOf("Italic") >= 0);
  font.setBold(ui.listViewFont->item(4, 1)->text().indexOf("Bold") >= 0);
  font.setUnderline(ui.listViewFont->item(4, 1)->text().indexOf("Underline") >=
                    0);
  font.setStrikeOut(ui.listViewFont->item(4, 1)->text().indexOf("Strikeout") >=
                    0);
  font = QFontDialog::getFont(&ok, font, this);
  if (ok) {
    // FT_Face fontFace = fontfreetypeFace();
    ui.listViewFont->item(0, 1)->setText(font.family());
    ui.listViewFont->item(1, 1)->setText(QString::number(font.pointSize()));
    QString optStr;
    if (font.italic())
      optStr += "Italic";
    if (font.bold()) {
      if (!optStr.isEmpty())
        optStr += ',';
      optStr += "Bold";
    }
    if (font.underline()) {
      if (!optStr.isEmpty())
        optStr += ',';
      optStr += "Underline";
    }
    if (font.strikeOut()) {
      if (!optStr.isEmpty())
        optStr += ',';
      optStr += "Strikeout";
    }
    ui.listViewFont->item(4, 1)->setText(optStr);
    writeSettings();
    updateMetaLines();
  }
}

quint32 bswap(quint32 val) {
  return val; // ntohl(val);
}

int colorLight(QColor color) {
  return (0.257 * color.red()) + (0.504 * color.green()) +
         (0.098 * color.blue()) + 16;
}

void TsMuxerWindow::setTextItemColor(QString str) {
  while (str.length() < 8)
    str = '0' + str;
  QColor color = bswap(str.toLongLong(0, 16));
  QTableWidgetItem *item = ui.listViewFont->item(2, 1);
  item->setBackground(QBrush(color));
  if (colorLight(color) < 128)
    item->setForeground(QBrush(QColor(255, 255, 255, 255)));
  else
    item->setForeground(QBrush(QColor(0, 0, 0, 255)));
  item->setText(QString("0x") + str);
}

void TsMuxerWindow::onColorBtnClicked() {
  QColor color = bswap(ui.listViewFont->item(2, 1)->text().toLongLong(0, 16));
  color = QColorDialog::getColor(color, this);
  QString str = QString::number(bswap(color.rgba()), 16);
  setTextItemColor(str);

  writeSettings();
  updateMetaLines();
}

void TsMuxerWindow::onGeneralCheckboxClicked() {
  ui.editMaxBitrate->setEnabled(ui.checkBoxRVBR->isChecked());
  ui.editMinBitrate->setEnabled(ui.checkBoxRVBR->isChecked());
  ui.editCBRBitrate->setEnabled(ui.checkBoxCBR->isChecked());
  ui.BlackplaylistCombo->setEnabled(ui.checkBoxBlankPL->isChecked());
  ui.BlackplaylistLabel->setEnabled(ui.checkBoxBlankPL->isChecked());
  updateMetaLines();
}

void TsMuxerWindow::onGeneralSpinboxValueChanged() { updateMetaLines(); }

void TsMuxerWindow::onChapterParamsChanged() {
  ui.memoChapters->setEnabled(ui.radioButtonCustomChapters->isChecked());
  ui.spinEditChapterLen->setEnabled(ui.radioButtonAutoChapter->isChecked());
  updateMetaLines();
}

void TsMuxerWindow::onSplitCutParamsChanged() {
  ui.spinEditSplitDuration->setEnabled(ui.splitByDuration->isChecked());
  ui.labelSplitByDur->setEnabled(ui.splitByDuration->isChecked());
  ui.editSplitSize->setEnabled(ui.splitBySize->isChecked());
  ui.comboBoxMeasure->setEnabled(ui.splitBySize->isChecked());

  ui.cutStartTimeEdit->setEnabled(ui.checkBoxCut->isChecked());
  ui.cutEndTimeEdit->setEnabled(ui.checkBoxCut->isChecked());
  updateMetaLines();
}

void TsMuxerWindow::onSavedParamChanged() {
  writeSettings();
  updateMetaLines();
}

void TsMuxerWindow::onFontParamsChanged() { updateMetaLines(); }

void TsMuxerWindow::onRemoveBtnClick() {
  if (!ui.inputFilesLV->currentItem())
    return;
  int idx = ui.inputFilesLV->currentRow();
  bool delMplsM2ts = false;

  if (idx < ui.inputFilesLV->count() - 1) {
    if (ui.inputFilesLV->currentItem()->data(MplsItemRole).toInt() ==
        MPLS_PRIMARY)
      delMplsM2ts = true;
    else if (ui.inputFilesLV->currentItem()->text().left(4) !=
             FILE_JOIN_PREFIX) {
      QString text = ui.inputFilesLV->item(idx + 1)->text();
      if (text.length() >= 4 && text.left(4) == FILE_JOIN_PREFIX)
        ui.inputFilesLV->item(idx + 1)->setText(text.mid(4));
    }
  }

  delTracksByFileName(myUnquoteStr(
      ui.inputFilesLV->currentItem()->data(FileNameRole).toString()));
  ui.inputFilesLV->takeItem(idx);
  if (idx >= ui.inputFilesLV->count())
    idx--;
  if (delMplsM2ts) {
    while (idx < ui.inputFilesLV->count()) {
      QString text = ui.inputFilesLV->item(idx)->text();
      if (text.length() >= 4 && text.left(4) == FILE_JOIN_PREFIX) {
        delTracksByFileName(myUnquoteStr(
            ui.inputFilesLV->item(idx)->data(FileNameRole).toString()));
        ui.inputFilesLV->takeItem(idx);
      } else
        break;
    }
  }
  if (ui.inputFilesLV->count() > 0)
    ui.inputFilesLV->setCurrentRow(idx);
  updateCustomChapters();
}

void TsMuxerWindow::delTracksByFileName(const QString &fileName) {
  for (int i = ui.trackLV->rowCount() - 1; i >= 0; --i) {
    long iCodec = ui.trackLV->item(i, 0)->data(Qt::UserRole).toLongLong();
    if (iCodec) {
      QtvCodecInfo *info = (QtvCodecInfo *)(void *)iCodec;
      for (int j = info->fileList.count() - 1; j >= 0; --j) {
        if (info->fileList[j] == fileName) {
          info->fileList.removeAt(j);
          break;
        }
      }
      for (int j = info->mplsFiles.count() - 1; j >= 0; --j) {
        if (info->mplsFiles[j] == fileName) {
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

void TsMuxerWindow::deleteTrack(int idx) {
  disableUpdatesCnt++;
  long iCodec = ui.trackLV->item(idx, 0)->data(Qt::UserRole).toLongLong();
  delete (QtvCodecInfo *)(void *)iCodec;
  int lastItemIndex = idx; // trackLV.items[idx].index;
  ui.trackLV->removeRow(idx);
  if (ui.trackLV->rowCount() == 0) {
    lastSourceDir.clear();
    while (ui.tabWidgetTracks->count())
      ui.tabWidgetTracks->removeTab(0);
    ui.tabWidgetTracks->addTab(ui.tabSheetFake, TI_DEFAULT_TAB_NAME);
    ui.outFileName->setText(getDefaultOutputFileName());
    outFileNameModified = false;
  } else {
    if (lastItemIndex > ui.trackLV->rowCount() - 1)
      --lastItemIndex;
    if (lastItemIndex >= 0) {
      ui.trackLV->setCurrentCell(lastItemIndex, 0);
      ui.trackLV->setRangeSelected(
          QTableWidgetSelectionRange(lastItemIndex, 0, lastItemIndex, 4), true);
    }
    updateNum();
  }
  updateMaxOffsets();
  updateMetaLines();
  ui.moveupBtn->setEnabled(ui.trackLV->currentItem() != 0);
  ui.movedownBtn->setEnabled(ui.trackLV->currentItem() != 0);
  ui.removeTrackBtn->setEnabled(ui.trackLV->currentItem() != 0);
  disableUpdatesCnt--;
  trackLVItemSelectionChanged();
}

void TsMuxerWindow::updateNum() {
  // for (int i = 0; i < ui.trackLV->rowCount(); ++i)
  //	ui.trackLV->item(i,0)->setText(QString::number(i+1));
}

void TsMuxerWindow::onAppendButtonClick() {
  QList<QtvCodecInfo> codecList;
  if (ui.inputFilesLV->currentItem() == 0) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("No track selected");
    msgBox.setText("No track selected");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    return;
  }
  QString fileName = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
      this, tr("Append media file"), lastInputDir, tr(fileDialogFilter)));
  if (fileName.isEmpty())
    return;
  lastInputDir = fileName;
  addFileList.clear();
  addFileList << QUrl::fromLocalFile(fileName);
  appendFile();
}

void TsMuxerWindow::appendFile() {
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
  connect(this, &TsMuxerWindow::tsMuxerSuccessFinished, this, &TsMuxerWindow::getCodecInfo);
  connect(this, &TsMuxerWindow::codecListReady, this, &TsMuxerWindow::continueAppendFile);
  connect(this, &TsMuxerWindow::fileAppended, this, &TsMuxerWindow::appendFile);
  runInMuxMode = false;
  shellExecute(
      QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) +
          QDir::separator() + "tsMuxeR",
      QStringList() << newFileName);
}

void TsMuxerWindow::continueAppendFile() {
  QString parentFileName = myUnquoteStr(
      (ui.inputFilesLV->currentItem()->data(FileNameRole).toString()));
  QFileInfo newFi(unquoteStr(newFileName));
  QFileInfo oldFi(unquoteStr(parentFileName));
  if (newFi.suffix().toUpper() != oldFi.suffix().toUpper()) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Invalid file extension");
    msgBox.setText("Appended file must have same file extension.");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    return;
  }

  disableUpdatesCnt++;

  int idx = ui.inputFilesLV->currentRow();
  bool firstStep = true;
  bool doublePrefix = false;
  for (; idx < ui.inputFilesLV->count(); ++idx) {
    int mplsRole = ui.inputFilesLV->item(idx)->data(MplsItemRole).toInt();
    if (mplsRole == MPLS_PRIMARY && firstStep || mplsRole == MPLS_M2TS) {
      ui.inputFilesLV->setCurrentRow(idx);
      doublePrefix = true;
    } else
      break;
    firstStep = false;
  }

  doAppendInt(newFileName, parentFileName, fileDuration, false,
              doublePrefix ? MPLS_PRIMARY : MPLS_NONE);

  if (mplsFileList.size() > 0)
    doAppendInt(mplsFileList[0].name, newFileName, mplsFileList[0].duration,
                doublePrefix, MPLS_M2TS);
  for (int mplsCnt = 1; mplsCnt < mplsFileList.size(); ++mplsCnt)
    doAppendInt(mplsFileList[mplsCnt].name, mplsFileList[mplsCnt - 1].name,
                mplsFileList[mplsCnt].duration, doublePrefix, MPLS_M2TS);

  updateMaxOffsets();
  if (!outFileNameModified) {
    modifyOutFileName(newFileName);
    outFileNameModified = true;
  }
  disableUpdatesCnt--;
  updateMetaLines();
  updateCustomChapters();
  emit fileAppended();
}

void TsMuxerWindow::onRemoveTrackButtonClick() {
  if (ui.trackLV->currentItem())
    deleteTrack(ui.trackLV->currentRow());
}

void TsMuxerWindow::onMoveUpButtonCLick() {
  if (ui.trackLV->currentItem() == 0 || ui.trackLV->currentRow() < 1)
    return;
  disableUpdatesCnt++;
  moveRow(ui.trackLV->currentRow(), ui.trackLV->currentRow() - 1);
  updateMetaLines();
  updateNum();
  disableUpdatesCnt--;
}

void TsMuxerWindow::onMoveDownButtonCLick() {
  if (ui.trackLV->currentItem() == 0 || ui.trackLV->rowCount() == 0 ||
      ui.trackLV->currentRow() == ui.trackLV->rowCount() - 1)
    return;
  disableUpdatesCnt++;
  moveRow(ui.trackLV->currentRow(), ui.trackLV->currentRow() + 2);
  updateMetaLines();
  updateNum();
  disableUpdatesCnt--;
}

void TsMuxerWindow::moveRow(int index, int index2) {
  ui.trackLV->insertRow(index2);
  ui.trackLV->setRowHeight(index2, 18);
  if (index2 < index)
    index++;
  for (int i = 0; i < ui.trackLV->columnCount(); ++i)
    ui.trackLV->setItem(index2, i, ui.trackLV->item(index, i)->clone());
  ui.trackLV->removeRow(index);
  if (index2 > index)
    index2--;
  ui.trackLV->setRangeSelected(QTableWidgetSelectionRange(index2, 0, index2, 4),
                               true);
  ui.trackLV->setCurrentCell(index2, 0);
}

void TsMuxerWindow::RadioButtonMuxClick() {
  if (outFileNameDisableChange)
    return;
  if (ui.radioButtonDemux->isChecked())
    ui.buttonMux->setText("Sta&rt demuxing");
  else
    ui.buttonMux->setText("Sta&rt muxing");
  outFileNameDisableChange = true;
  if (ui.radioButtonBluRay->isChecked() ||
      ui.radioButtonBluRayUHD->isChecked() ||
      ui.radioButtonDemux->isChecked() || ui.radioButtonAVCHD->isChecked()) {
    QFileInfo fi(unquoteStr(ui.outFileName->text()));
    if (!fi.suffix().isEmpty()) {
      oldFileName = fi.fileName();
      ui.outFileName->setText(QDir::toNativeSeparators(fi.absolutePath()) +
                              QDir::separator());
    }
    ui.FilenameLabel->setText(tr("Folder"));
  } else {
    ui.FilenameLabel->setText(tr("File name"));
    if (!oldFileName.isEmpty()) {
      ui.outFileName->setText(QDir::toNativeSeparators(ui.outFileName->text()));
      if (!ui.outFileName->text().isEmpty() &&
          ui.outFileName->text().right(1) != QDir::separator())
        ui.outFileName->setText(ui.outFileName->text() + QDir::separator());
      ui.outFileName->setText(ui.outFileName->text() + oldFileName);
      oldFileName.clear();
    }
    if (ui.radioButtonTS->isChecked()) {
      ui.outFileName->setText(changeFileExt(ui.outFileName->text(), "ts"));
      saveDialogFilter = TS_SAVE_DIALOG_FILTER;
    } else if (ui.radioButtonBluRayISO->isChecked()) {
      ui.outFileName->setText(changeFileExt(ui.outFileName->text(), "iso"));
      saveDialogFilter = ISO_SAVE_DIALOG_FILTER;
    } else if (ui.radioButtonBluRayISOUHD->isChecked()) {
      ui.outFileName->setText(changeFileExt(ui.outFileName->text(), "iso"));
      saveDialogFilter = ISO_SAVE_DIALOG_FILTER;
    } else {
      ui.outFileName->setText(changeFileExt(ui.outFileName->text(), "m2ts"));
      saveDialogFilter = M2TS_SAVE_DIALOG_FILTER;
    }
  }
  ui.DiskLabel->setVisible(ui.radioButtonBluRayISO->isChecked());
  ui.DiskLabelEdit->setVisible(ui.radioButtonBluRayISO->isChecked());
  ui.editDelay->setEnabled(!ui.radioButtonDemux->isChecked());
  updateMetaLines();
  outFileNameDisableChange = false;
}

void TsMuxerWindow::outFileNameChanged() {
  outFileNameModified = true;
  if (outFileNameDisableChange)
    return;
  if (ui.radioButtonDemux->isChecked() || ui.radioButtonBluRay->isChecked() ||
      ui.radioButtonBluRayUHD->isChecked() || ui.radioButtonAVCHD->isChecked())
    return;

  outFileNameDisableChange = true;
  QFileInfo fi(unquoteStr(ui.outFileName->text().trimmed()));
  QString ext = fi.suffix().toUpper();

  bool isISOMode = ui.radioButtonBluRayISO->isChecked() ||
                   ui.radioButtonBluRayISOUHD->isChecked();

  if (ext == "M2TS" || ext == "M2TS\"")
    ui.radioButtonM2TS->setChecked(true);
  else if (ext == "ISO" || ext == "ISO\"") {
    ui.radioButtonBluRayISO->setChecked(true);
  } else
    ui.radioButtonTS->setChecked(true);

  bool isISOModeNew = ui.radioButtonBluRayISO->isChecked() ||
                      ui.radioButtonBluRayISOUHD->isChecked();

  ui.DiskLabel->setVisible(ui.radioButtonBluRayISO->isChecked());
  ui.DiskLabelEdit->setVisible(ui.radioButtonBluRayISO->isChecked());
  if (isISOMode != isISOModeNew)
    updateMetaLines();
  outFileNameDisableChange = false;
}

void TsMuxerWindow::saveFileDialog() {
  if (ui.radioButtonDemux->isChecked() || ui.radioButtonBluRay->isChecked() ||
      ui.radioButtonAVCHD->isChecked()) {
    QString folder = QDir::toNativeSeparators(
        QFileDialog::getExistingDirectory(this, getOutputDir()));
    if (!folder.isEmpty()) {
      ui.outFileName->setText(folder + QDir::separator());
      outFileNameModified = true;
      lastOutputDir = folder;
      writeSettings();
    }
  } else {
    QString path = getOutputDir();
    if (path.isEmpty()) {
      QString fileName = unquoteStr(ui.outFileName->text());
      path = QFileInfo(fileName).absolutePath();
    }
    QString fileName = QDir::toNativeSeparators(QFileDialog::getSaveFileName(
        this, "Select file for muxing", path, saveDialogFilter));
    if (!fileName.isEmpty()) {
      ui.outFileName->setText(fileName);
      lastOutputDir = QFileInfo(fileName).absolutePath();
      writeSettings();
    }
  }
}

void TsMuxerWindow::startMuxing() {
  QString outputName = unquoteStr(ui.outFileName->text().trimmed());
  ui.outFileName->setText(outputName);
  lastOutputDir = QFileInfo(outputName).absolutePath();
  writeSettings();
  if (ui.radioButtonM2TS->isChecked()) {
    QFileInfo fi(ui.outFileName->text());
    if (fi.suffix().toUpper() != "M2TS") {
      QMessageBox msgBox(this);
      msgBox.setWindowTitle("Invalid file name");
      msgBox.setText(QString("The output file \"") + ui.outFileName->text() +
                     "\" has invalid extension. Please, change file extension "
                     "to \".m2ts\"");
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.exec();
      return;
    }
  } else if (ui.radioButtonBluRayISO->isChecked()) {
    QFileInfo fi(ui.outFileName->text());
    if (fi.suffix().toUpper() != "ISO") {
      QMessageBox msgBox(this);
      msgBox.setWindowTitle("Invalid file name");
      msgBox.setText(QString("The output file \"") + ui.outFileName->text() +
                     "\" has invalid extension. Please, change file extension "
                     "to \".iso\"");
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.exec();
      return;
    }
  } else if (ui.radioButtonBluRayISOUHD->isChecked()) {
    QFileInfo fi(ui.outFileName->text());
    if (fi.suffix().toUpper() != "ISO") {
      QMessageBox msgBox(this);
      msgBox.setWindowTitle("Invalid file name");
      msgBox.setText(QString("The output file \"") + ui.outFileName->text() +
                     "\" has invalid extension. Please, change file extension "
                     "to \".iso\"");
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.exec();
      return;
    }
  }

  bool isFile = ui.radioButtonM2TS->isChecked() ||
                ui.radioButtonTS->isChecked() ||
                ui.radioButtonBluRayISO->isChecked() ||
                ui.radioButtonBluRayISOUHD->isChecked();
  if (isFile && QFile::exists(ui.outFileName->text())) {
    QString fileOrDir = isFile ? "file" : "directory";
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QString("Overwrite existing ") + fileOrDir + "?");
    msgBox.setText(QString("The output ") + fileOrDir + " \"" +
                   ui.outFileName->text() +
                   "\" already exists. Do you want to overwrite it?\"");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    if (msgBox.exec() != QMessageBox::Yes)
      return;
  }

  QFileInfo fi(ui.outFileName->text());
  metaName = QDir::toNativeSeparators(QDir::tempPath()) + QDir::separator() +
             "tsMuxeR_" + fi.completeBaseName() + ".meta";
  metaName = metaName.replace(' ', '_');
  if (!saveMetaFile(metaName)) {
    metaName.clear();
    return;
  }
  muxForm.prepare(!ui.radioButtonDemux->isChecked() ? "Muxing in progress"
                                                    : "Demuxing in progress");
  ui.buttonMux->setEnabled(false);
  ui.addBtn->setEnabled(false);
  ui.btnAppend->setEnabled(false);
  muxForm.show();
  disconnect();
  // QCoreApplication::dir
  runInMuxMode = true;
  shellExecute(
      QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) +
          QDir::separator() + "tsMuxeR",
      QStringList() << metaName << quoteStr(ui.outFileName->text()));
}

void TsMuxerWindow::saveMetaFileBtnClick() {
  QString metaName = QFileDialog::getSaveFileName(
      this, "", changeFileExt(ui.outFileName->text(), "meta"), saveMetaFilter);
  if (metaName.isEmpty())
    return;
  QFileInfo fi(metaName);
  QDir dir;
  dir.mkpath(fi.absolutePath());
  saveMetaFile(metaName);
}

bool TsMuxerWindow::saveMetaFile(const QString &metaName) {
  QFile file(metaName);
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Can't create temporary meta file");
    msgBox.setText(QString("Can't create temporary meta file \"") + metaName +
                   "\"");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
    return false;
  }
  QByteArray metaText = ui.memoMeta->toPlainText().toLocal8Bit();
  file.write(metaText);
  file.close();
  return true;
}

void TsMuxerWindow::closeEvent(QCloseEvent *event) {
  if (!metaName.isEmpty()) {
    QFile::remove(metaName);
    metaName.clear();
  }
  muxForm.close();
  event->accept();
}

void TsMuxerWindow::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat("text/plain") ||
      event->mimeData()->hasFormat("text/uri-list")) {
    if (ui.addBtn->isEnabled()) {
      opacityTimer.stop();
      setWindowOpacity(0.9);
      event->acceptProposedAction();
      QWidget *w = childAt(event->pos());
      updateBtns(w);
    }
  }
}

void TsMuxerWindow::dropEvent(QDropEvent *event) {
  setWindowOpacity(1.0);
  updateBtns(0);
  QWidget *w = childAt(event->pos());
  QString wName;
  if (w)
    wName = w->objectName();
  if (event->mimeData()->hasFormat("text/uri-list")) {
    addFileList = event->mimeData()->urls();
    event->acceptProposedAction();
  } else if (event->mimeData()->hasFormat("text/plain")) {
    QList<QString> strList;
    addFileList.clear();
    splitLines(event->mimeData()->text(), strList);
    QList<QUrl> urls = event->mimeData()->urls();
    for (int i = 0; i < strList.size(); ++i)
      addFileList << QUrl::fromLocalFile(strList[i]);
    event->acceptProposedAction();
  }
  if (addFileList.isEmpty())
    return;
  if (wName == "btnAppend" && ui.btnAppend->isEnabled())
    appendFile();
  else if (ui.addBtn->isEnabled())
    addFile();
}

void TsMuxerWindow::dragMoveEvent(QDragMoveEvent *event) {
  event->acceptProposedAction();
  QWidget *w = childAt(event->pos());
  updateBtns(w);
}

void TsMuxerWindow::updateBtns(QWidget *w) {
  if (w) {
    QString wName = w->objectName();
    ui.btnAppend->setDefault(wName == "btnAppend" && ui.btnAppend->isEnabled());
    ui.addBtn->setDefault(wName == "addBtn" && ui.addBtn->isEnabled());
  } else {
    ui.btnAppend->setDefault(false);
    ui.addBtn->setDefault(false);
  }
  QFont font = ui.removeFile->font();
  QFont bFont(font);
  bFont.setBold(true);
  if (ui.btnAppend->isDefault())
    ui.btnAppend->setFont(bFont);
  else
    ui.btnAppend->setFont(font);
  if (ui.addBtn->isDefault())
    ui.addBtn->setFont(bFont);
  else
    ui.addBtn->setFont(font);
}

void TsMuxerWindow::dragLeaveEvent(QDragLeaveEvent *event) {
  opacityTimer.start(100);
}

void TsMuxerWindow::onOpacityTimer() {
  opacityTimer.stop();
  setWindowOpacity(1.0);
  updateBtns(0);
}

void TsMuxerWindow::updateMaxOffsets() {
  int maxPGOffsets = 0;
  m_3dMode = false;

  for (int i = 0; i < ui.trackLV->rowCount(); ++i) {
    long iCodec = ui.trackLV->item(i, 0)->data(Qt::UserRole).toLongLong();
    if (!iCodec)
      continue;

    QtvCodecInfo *codecInfo = (QtvCodecInfo *)(void *)iCodec;
    if (codecInfo->displayName == "MVC") {
      m_3dMode = true;
      maxPGOffsets = qMax(maxPGOffsets, codecInfo->maxPgOffsets);
    }
  }

  disableUpdatesCnt++;

  int oldIndex = ui.offsetsComboBox->currentIndex();
  ui.offsetsComboBox->clear();
  ui.offsetsComboBox->addItem(QString("zero"));
  for (int i = 0; i < maxPGOffsets; ++i)
    ui.offsetsComboBox->addItem(QString("plane #%1").arg(i));
  if (oldIndex >= 0 && oldIndex < ui.offsetsComboBox->count())
    ui.offsetsComboBox->setCurrentIndex(oldIndex);

  disableUpdatesCnt--;
}

bool TsMuxerWindow::eventFilter(QObject *obj, QEvent *event) {
  if (obj == ui.label_Donate && event->type() == QEvent::MouseButtonPress) {
    QDesktopServices::openUrl(QUrl("https://github.com/justdan96/tsMuxer"));
    return true;
  } else {
    return QWidget::eventFilter(obj, event);
  }
}

void TsMuxerWindow::at_sectionCheckstateChanged(Qt::CheckState state) {
  if (disableUpdatesCnt > 0)
    return;

  disableUpdatesCnt++;
  for (int i = 0; i < ui.trackLV->rowCount(); ++i)
    ui.trackLV->item(i, 0)->setCheckState(state);
  disableUpdatesCnt--;
  trackLVItemSelectionChanged();
}

void TsMuxerWindow::writeSettings() {
  if (disableUpdatesCnt > 0)
    return;

  disableUpdatesCnt++;

  settings->beginGroup("general");
  // settings->setValue("asyncIO", ui.checkBoxuseAsynIO->isChecked());
  settings->setValue("soundEnabled", ui.checkBoxSound->isChecked());
  settings->setValue("hdmvPES", ui.checkBoxNewAudioPes->isChecked());
  if (ui.checkBoxCrop->isEnabled())
    settings->setValue("restoreCropEnabled", ui.checkBoxCrop->isChecked());
  settings->setValue("outputDir", lastOutputDir);
  settings->setValue("useBlankPL", ui.checkBoxBlankPL->isChecked());
  settings->setValue("blankPLNum", ui.BlackplaylistCombo->value());

  settings->setValue("outputToInputFolder",
                     ui.radioButtonOutoutInInput->isChecked());

  settings->endGroup();

  settings->beginGroup("subtitles");
  settings->setValue("fontBorder", ui.spinEditBorder->value());
  settings->setValue("fontLineSpacing", ui.lineSpacing->value());
  settings->setValue("offset", ui.spinEditOffset->value());
  settings->setValue("fadeTime", getRendererAnimationTime());
  settings->setValue("famaly", ui.listViewFont->item(0, 1)->text());
  settings->setValue("size", ui.listViewFont->item(1, 1)->text().toUInt());
  settings->setValue("color",
                     ui.listViewFont->item(2, 1)->text().mid(2).toUInt(0, 16));
  settings->setValue("options", ui.listViewFont->item(4, 1)->text());
  settings->endGroup();

  settings->beginGroup("pip");
  settings->setValue("corner", ui.comboBoxPipCorner->currentIndex());
  settings->setValue("h_offset", ui.spinBoxPipOffsetH->value());
  settings->setValue("v_offset", ui.spinBoxPipOffsetV->value());
  settings->setValue("size", ui.comboBoxPipSize->currentIndex());
  settings->endGroup();

  disableUpdatesCnt--;
}

bool TsMuxerWindow::readSettings() {
  settings->beginGroup("general");

  QString outputDir = settings->value("outputDir").toString();
  if (!outputDir.isEmpty())
    lastOutputDir = outputDir;
  else {
    settings->endGroup();
    return false; // no settings still written
  }

  // ui.checkBoxuseAsynIO->setChecked(settings->value("asyncIO").toBool());
  ui.checkBoxSound->setChecked(settings->value("soundEnabled").toBool());
  ui.checkBoxNewAudioPes->setChecked(settings->value("hdmvPES").toBool());
  ui.checkBoxCrop->setChecked(settings->value("restoreCropEnabled").toBool());
  ui.checkBoxBlankPL->setChecked(settings->value("useBlankPL").toBool());
  int plNum = settings->value("blankPLNum").toInt();
  if (plNum)
    ui.BlackplaylistCombo->setValue(plNum);

  ui.radioButtonOutoutInInput->setChecked(
      settings->value("outputToInputFolder").toBool());
  ui.radioButtonStoreOutput->setChecked(
      !ui.radioButtonOutoutInInput->isChecked());

  settings->endGroup();

  // checkBoxVBR checkBoxRVBR    editMaxBitrate  editMinBitrate  checkBoxCBR
  // editCBRBitrate  editVBVLen

  settings->beginGroup("subtitles");
  ui.spinEditBorder->setValue(settings->value("fontBorder").toInt());
  ui.lineSpacing->setValue(settings->value("fontLineSpacing").toDouble());
  setRendererAnimationTime(settings->value("fadeTime").toDouble());
  ui.spinEditOffset->setValue(settings->value("offset").toInt());
  QString fontName = settings->value("famaly").toString();
  if (!fontName.isEmpty())
    ui.listViewFont->item(0, 1)->setText(fontName);
  int fontSize = settings->value("size").toInt();
  if (fontSize > 0)
    ui.listViewFont->item(1, 1)->setText(QString::number(fontSize));
  if (!settings->value("color").isNull()) {
    quint32 color = settings->value("color").toUInt();
    setTextItemColor(QString::number(color, 16));
  }
  ui.listViewFont->item(4, 1)->setText(settings->value("options").toString());
  settings->endGroup();

  settings->beginGroup("pip");
  ui.comboBoxPipCorner->setCurrentIndex(settings->value("corner").toInt());
  ui.spinBoxPipOffsetH->setValue(settings->value("h_offset").toInt());
  ui.spinBoxPipOffsetV->setValue(settings->value("v_offset").toInt());
  ui.comboBoxPipSize->setCurrentIndex(settings->value("size").toInt());
  settings->endGroup();

  return true;
}
