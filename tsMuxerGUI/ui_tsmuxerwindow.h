/********************************************************************************
** Form generated from reading UI file 'tsmuxerwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TSMUXERWINDOW_H
#define UI_TSMUXERWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTimeEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TsMuxerWindow
{
public:
    QVBoxLayout *verticalLayout_10;
    QSplitter *splitter;
    QTabWidget *tabWidget;
    QWidget *GeneralTab;
    QVBoxLayout *verticalLayout_22;
    QSplitter *trackSplitter;
    QWidget *layoutWidget11;
    QVBoxLayout *verticalLayout_3;
    QLabel *label_3;
    QHBoxLayout *horizontalLayout;
    QListWidget *inputFilesLV;
    QVBoxLayout *verticalLayout;
    QPushButton *addBtn;
    QPushButton *btnAppend;
    QSpacerItem *verticalSpacer;
    QPushButton *removeFile;
    QSpacerItem *verticalSpacer_10;
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout_21;
    QLabel *label_2;
    QHBoxLayout *horizontalLayout_2;
    QTableWidget *trackLV;
    QVBoxLayout *verticalLayout_4;
    QPushButton *moveupBtn;
    QPushButton *movedownBtn;
    QSpacerItem *verticalSpacer_3;
    QPushButton *removeTrackBtn;
    QSpacerItem *verticalSpacer_4;
    QTabWidget *tabWidgetTracks;
    QWidget *tabSheetVideo;
    QHBoxLayout *horizontalLayout_8;
    QLabel *imageVideo;
    QGridLayout *gridLayout;
    QCheckBox *checkFPS;
    QComboBox *comboBoxFPS;
    QCheckBox *checkBoxRemovePulldown;
    QCheckBox *checkBoxSecondaryVideo;
    QComboBox *comboBoxLevel;
    QLabel *labelAR;
    QComboBox *comboBoxAR;
    QCheckBox *checkBoxLevel;
    QCheckBox *checkBoxSPS;
    QSpacerItem *horizontalSpacer_24;
    QComboBox *comboBoxSEI;
    QWidget *tabSheetAudio;
    QHBoxLayout *horizontalLayout_13;
    QLabel *imageSubtitles;
    QLabel *imageAudio;
    QGridLayout *gridLayout_2;
    QLabel *label_18;
    QHBoxLayout *horizontalLayout_12;
    QSpinBox *editDelay;
    QHBoxLayout *horizontalLayout_11;
    QCheckBox *dtsDwnConvert;
    QCheckBox *secondaryCheckBox;
    QCheckBox *checkBoxKeepFps;
    QLabel *offsetsLabel;
    QComboBox *offsetsComboBox;
    QSpacerItem *horizontalSpacer_6;
    QLabel *label_19;
    QComboBox *langComboBox;
    QWidget *demuxLpcmOptions;
    QVBoxLayout *verticalLayout_5;
    QRadioButton *radioButton_4;
    QRadioButton *radioButton_5;
    QRadioButton *radioButton_6;
    QWidget *tabSheetFake;
    QWidget *tab_3;
    QVBoxLayout *verticalLayout_7;
    QGridLayout *gridLayout_14;
    QGroupBox *groupBox7;
    QVBoxLayout *verticalLayout_17;
    QVBoxLayout *verticalLayout_14;
    QHBoxLayout *horizontalLayout_18;
    QRadioButton *checkBoxVBR;
    QHBoxLayout *horizontalLayout_17;
    QRadioButton *checkBoxRVBR;
    QHBoxLayout *horizontalLayout_19;
    QGridLayout *gridLayout_3;
    QLabel *label_5;
    QDoubleSpinBox *editMaxBitrate;
    QLabel *label_6;
    QDoubleSpinBox *editMinBitrate;
    QSpacerItem *horizontalSpacer_7;
    QSpacerItem *horizontalSpacer_8;
    QHBoxLayout *horizontalLayout_16;
    QRadioButton *checkBoxCBR;
    QHBoxLayout *horizontalLayout_14;
    QGridLayout *gridLayout_4;
    QLabel *label_7;
    QDoubleSpinBox *editCBRBitrate;
    QSpacerItem *horizontalSpacer_9;
    QHBoxLayout *horizontalLayout_15;
    QLabel *label_8;
    QSpinBox *editVBVLen;
    QSpacerItem *horizontalSpacer_10;
    QSpacerItem *verticalSpacer_2;
    QGroupBox *groupBox_5;
    QVBoxLayout *verticalLayout_18;
    QCheckBox *checkBoxSound;
    QCheckBox *checkBoxNewAudioPes;
    QCheckBox *checkBoxCrop;
    QGroupBox *GroupBox114;
    QVBoxLayout *verticalLayout_24;
    QRadioButton *radioButtonStoreOutput;
    QRadioButton *radioButtonOutoutInInput;
    QSpacerItem *verticalSpacer_6;
    QWidget *tab_2;
    QHBoxLayout *horizontalLayout_5;
    QGroupBox *groupBox_3;
    QVBoxLayout *verticalLayout_11;
    QRadioButton *radioButtonNoChapters;
    QHBoxLayout *horizontalLayout_6;
    QRadioButton *radioButtonAutoChapter;
    QSpinBox *spinEditChapterLen;
    QLabel *label_4;
    QRadioButton *radioButtonCustomChapters;
    QLabel *label_9;
    QTextEdit *memoChapters;
    QGroupBox *BDOptionsGroupBox;
    QVBoxLayout *verticalLayout_19;
    QCheckBox *checkBoxBlankPL;
    QHBoxLayout *horizontalLayout_32;
    QSpacerItem *horizontalSpacer_22;
    QLabel *BlackplaylistLabel;
    QSpinBox *BlackplaylistCombo;
    QSpacerItem *horizontalSpacer_23;
    QGridLayout *gridLayout_10;
    QLabel *label;
    QSpinBox *spinBoxMplsNum;
    QLabel *label_10;
    QSpinBox *spinBoxM2tsNum;
    QTimeEdit *muxTimeEdit;
    QLabel *label_20;
    QSpinBox *muxTimeClock;
    QLabel *label_23;
    QSpacerItem *horizontalSpacer_19;
    QGroupBox *groupBox_11;
    QVBoxLayout *verticalLayout_20;
    QCheckBox *rightEyeCheckBox;
    QGroupBox *groupBoxPip;
    QVBoxLayout *verticalLayout_23;
    QGridLayout *gridLayout_13;
    QLabel *label_24;
    QComboBox *comboBoxPipCorner;
    QLabel *label_25;
    QSpinBox *spinBoxPipOffsetH;
    QLabel *label_26;
    QComboBox *comboBoxPipSize;
    QSpacerItem *horizontalSpacer_3;
    QLabel *label_27;
    QSpinBox *spinBoxPipOffsetV;
    QSpacerItem *verticalSpacer_9;
    QWidget *tab;
    QVBoxLayout *verticalLayout_8;
    QGroupBox *groupBox_6;
    QHBoxLayout *horizontalLayout_21;
    QGridLayout *gridLayout_5;
    QRadioButton *noSplit;
    QRadioButton *splitByDuration;
    QSpinBox *spinEditSplitDuration;
    QLabel *labelSplitByDur;
    QRadioButton *splitBySize;
    QDoubleSpinBox *editSplitSize;
    QComboBox *comboBoxMeasure;
    QSpacerItem *horizontalSpacer_16;
    QGroupBox *groupBox_7;
    QHBoxLayout *horizontalLayout_23;
    QVBoxLayout *verticalLayout_15;
    QCheckBox *checkBoxCut;
    QHBoxLayout *horizontalLayout_22;
    QGridLayout *gridLayout_6;
    QLabel *label_11;
    QLabel *label_12;
    QTimeEdit *cutStartTimeEdit;
    QTimeEdit *cutEndTimeEdit;
    QLabel *label_28;
    QLabel *label_29;
    QSpacerItem *horizontalSpacer_18;
    QSpacerItem *verticalSpacer_5;
    QWidget *tab_4;
    QVBoxLayout *verticalLayout_9;
    QGroupBox *groupBox_8;
    QHBoxLayout *horizontalLayout_25;
    QGridLayout *gridLayout_7;
    QTableWidget *listViewFont;
    QVBoxLayout *verticalLayout_16;
    QPushButton *fontButton;
    QPushButton *colorButton;
    QSpacerItem *verticalSpacer_8;
    QGridLayout *gridLayout_12;
    QLabel *label_13;
    QSpinBox *spinEditBorder;
    QLabel *label_21;
    QDoubleSpinBox *lineSpacing;
    QLabel *label_22;
    QComboBox *comboBoxAnimation;
    QSpacerItem *horizontalSpacer_20;
    QHBoxLayout *horizontalLayout_3;
    QGroupBox *groupBox_9;
    QHBoxLayout *horizontalLayout_28;
    QGridLayout *gridLayout_9;
    QRadioButton *radioButton;
    QHBoxLayout *horizontalLayout_26;
    QSpacerItem *horizontalSpacer_11;
    QLabel *label_14;
    QSpinBox *spinBox;
    QRadioButton *radioButton_2;
    QRadioButton *radioButton_3;
    QHBoxLayout *horizontalLayout_27;
    QSpacerItem *horizontalSpacer_13;
    QLabel *label_15;
    QSpinBox *spinEditOffset;
    QSpacerItem *horizontalSpacer_14;
    QGroupBox *groupBox_10;
    QHBoxLayout *horizontalLayout_31;
    QGridLayout *gridLayout_8;
    QRadioButton *rbhLeft;
    QHBoxLayout *horizontalLayout_29;
    QSpacerItem *horizontalSpacer_12;
    QLabel *label_17;
    QSpinBox *spinBox_2;
    QRadioButton *rbhCenter;
    QRadioButton *rbhRight;
    QHBoxLayout *horizontalLayout_30;
    QSpacerItem *horizontalSpacer_15;
    QLabel *label_16;
    QSpinBox *SpinEditOffset_2;
    QSpacerItem *horizontalSpacer_21;
    QSpacerItem *verticalSpacer_7;
    QWidget *About;
    QVBoxLayout *verticalLayout_12;
    QTextEdit *textEdit;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout_13;
    QGroupBox *groupBox_4;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout_7;
    QRadioButton *radioButtonTS;
    QRadioButton *radioButtonM2TS;
    QRadioButton *radioButtonBluRayISO;
    QRadioButton *radioButtonBluRay;
    QRadioButton *radioButtonAVCHD;
    QRadioButton *radioButtonDemux;
    QSpacerItem *horizontalSpacer_4;
    QGridLayout *gridLayout_11;
    QLabel *DiskLabel;
    QLineEdit *DiskLabelEdit;
    QLabel *FilenameLabel;
    QLineEdit *outFileName;
    QPushButton *btnBrowse;
    QGroupBox *groupBox_2;
    QVBoxLayout *verticalLayout_6;
    QTextEdit *memoMeta;
    QHBoxLayout *horizontalLayout_4;
    QSpacerItem *horizontalSpacer;
    QPushButton *buttonMux;
    QPushButton *buttonSaveMeta;
    QSpacerItem *horizontalSpacer_2;
    QLabel *label_Donate;

    void setupUi(QWidget *TsMuxerWindow)
    {
        if (TsMuxerWindow->objectName().isEmpty())
            TsMuxerWindow->setObjectName(QString::fromUtf8("TsMuxerWindow"));
        TsMuxerWindow->resize(893, 794);
        TsMuxerWindow->setMinimumSize(QSize(660, 650));
        TsMuxerWindow->setAcceptDrops(true);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/images/icon.png"), QSize(), QIcon::Normal, QIcon::Off);
        TsMuxerWindow->setWindowIcon(icon);
        verticalLayout_10 = new QVBoxLayout(TsMuxerWindow);
        verticalLayout_10->setSpacing(6);
        verticalLayout_10->setContentsMargins(5, 5, 5, 5);
        verticalLayout_10->setObjectName(QString::fromUtf8("verticalLayout_10"));
        splitter = new QSplitter(TsMuxerWindow);
        splitter->setObjectName(QString::fromUtf8("splitter"));
        splitter->setOrientation(Qt::Vertical);
        tabWidget = new QTabWidget(splitter);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        GeneralTab = new QWidget();
        GeneralTab->setObjectName(QString::fromUtf8("GeneralTab"));
        GeneralTab->setEnabled(true);
        verticalLayout_22 = new QVBoxLayout(GeneralTab);
        verticalLayout_22->setSpacing(6);
        verticalLayout_22->setContentsMargins(5, 5, 5, 5);
        verticalLayout_22->setObjectName(QString::fromUtf8("verticalLayout_22"));
        trackSplitter = new QSplitter(GeneralTab);
        trackSplitter->setObjectName(QString::fromUtf8("trackSplitter"));
        trackSplitter->setOrientation(Qt::Vertical);
        layoutWidget11 = new QWidget(trackSplitter);
        layoutWidget11->setObjectName(QString::fromUtf8("layoutWidget11"));
        verticalLayout_3 = new QVBoxLayout(layoutWidget11);
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setContentsMargins(5, 5, 5, 5);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        verticalLayout_3->setContentsMargins(0, 0, 0, 0);
        label_3 = new QLabel(layoutWidget11);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        verticalLayout_3->addWidget(label_3);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        inputFilesLV = new QListWidget(layoutWidget11);
        inputFilesLV->setObjectName(QString::fromUtf8("inputFilesLV"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(inputFilesLV->sizePolicy().hasHeightForWidth());
        inputFilesLV->setSizePolicy(sizePolicy);
        inputFilesLV->setMinimumSize(QSize(0, 100));
        inputFilesLV->setMaximumSize(QSize(16777215, 16777215));
        inputFilesLV->setAcceptDrops(true);
        inputFilesLV->setSelectionBehavior(QAbstractItemView::SelectRows);

        horizontalLayout->addWidget(inputFilesLV);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(6);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        addBtn = new QPushButton(layoutWidget11);
        addBtn->setObjectName(QString::fromUtf8("addBtn"));
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(addBtn->sizePolicy().hasHeightForWidth());
        addBtn->setSizePolicy(sizePolicy1);
        addBtn->setFlat(false);

        verticalLayout->addWidget(addBtn);

        btnAppend = new QPushButton(layoutWidget11);
        btnAppend->setObjectName(QString::fromUtf8("btnAppend"));
        btnAppend->setEnabled(false);

        verticalLayout->addWidget(btnAppend);

        verticalSpacer = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout->addItem(verticalSpacer);

        removeFile = new QPushButton(layoutWidget11);
        removeFile->setObjectName(QString::fromUtf8("removeFile"));
        removeFile->setEnabled(false);
        sizePolicy1.setHeightForWidth(removeFile->sizePolicy().hasHeightForWidth());
        removeFile->setSizePolicy(sizePolicy1);

        verticalLayout->addWidget(removeFile);

        verticalSpacer_10 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_10);


        horizontalLayout->addLayout(verticalLayout);


        verticalLayout_3->addLayout(horizontalLayout);

        trackSplitter->addWidget(layoutWidget11);
        layoutWidget = new QWidget(trackSplitter);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        verticalLayout_21 = new QVBoxLayout(layoutWidget);
        verticalLayout_21->setSpacing(6);
        verticalLayout_21->setContentsMargins(5, 5, 5, 5);
        verticalLayout_21->setObjectName(QString::fromUtf8("verticalLayout_21"));
        verticalLayout_21->setContentsMargins(0, 0, 0, 0);
        label_2 = new QLabel(layoutWidget);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        verticalLayout_21->addWidget(label_2);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setSizeConstraint(QLayout::SetMinimumSize);
        trackLV = new QTableWidget(layoutWidget);
        if (trackLV->columnCount() < 5)
            trackLV->setColumnCount(5);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        trackLV->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        trackLV->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        trackLV->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        trackLV->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        trackLV->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        trackLV->setObjectName(QString::fromUtf8("trackLV"));
        trackLV->setMinimumSize(QSize(0, 100));
        trackLV->setMaximumSize(QSize(16777215, 1000000));
        trackLV->setSelectionBehavior(QAbstractItemView::SelectRows);

        horizontalLayout_2->addWidget(trackLV);

        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setSpacing(6);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        moveupBtn = new QPushButton(layoutWidget);
        moveupBtn->setObjectName(QString::fromUtf8("moveupBtn"));
        moveupBtn->setEnabled(false);

        verticalLayout_4->addWidget(moveupBtn);

        movedownBtn = new QPushButton(layoutWidget);
        movedownBtn->setObjectName(QString::fromUtf8("movedownBtn"));
        movedownBtn->setEnabled(false);

        verticalLayout_4->addWidget(movedownBtn);

        verticalSpacer_3 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_4->addItem(verticalSpacer_3);

        removeTrackBtn = new QPushButton(layoutWidget);
        removeTrackBtn->setObjectName(QString::fromUtf8("removeTrackBtn"));
        removeTrackBtn->setEnabled(false);

        verticalLayout_4->addWidget(removeTrackBtn);

        verticalSpacer_4 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_4->addItem(verticalSpacer_4);


        horizontalLayout_2->addLayout(verticalLayout_4);


        verticalLayout_21->addLayout(horizontalLayout_2);

        trackSplitter->addWidget(layoutWidget);

        verticalLayout_22->addWidget(trackSplitter);

        tabWidgetTracks = new QTabWidget(GeneralTab);
        tabWidgetTracks->setObjectName(QString::fromUtf8("tabWidgetTracks"));
        tabWidgetTracks->setEnabled(true);
        QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(tabWidgetTracks->sizePolicy().hasHeightForWidth());
        tabWidgetTracks->setSizePolicy(sizePolicy2);
        tabWidgetTracks->setMinimumSize(QSize(0, 0));
        tabSheetVideo = new QWidget();
        tabSheetVideo->setObjectName(QString::fromUtf8("tabSheetVideo"));
        QSizePolicy sizePolicy3(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(tabSheetVideo->sizePolicy().hasHeightForWidth());
        tabSheetVideo->setSizePolicy(sizePolicy3);
        horizontalLayout_8 = new QHBoxLayout(tabSheetVideo);
        horizontalLayout_8->setSpacing(6);
        horizontalLayout_8->setContentsMargins(5, 5, 5, 5);
        horizontalLayout_8->setObjectName(QString::fromUtf8("horizontalLayout_8"));
        imageVideo = new QLabel(tabSheetVideo);
        imageVideo->setObjectName(QString::fromUtf8("imageVideo"));
        imageVideo->setPixmap(QPixmap(QString::fromUtf8(":/images/track_video.png")));

        horizontalLayout_8->addWidget(imageVideo);

        gridLayout = new QGridLayout();
        gridLayout->setSpacing(6);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        checkFPS = new QCheckBox(tabSheetVideo);
        checkFPS->setObjectName(QString::fromUtf8("checkFPS"));

        gridLayout->addWidget(checkFPS, 0, 0, 1, 1);

        comboBoxFPS = new QComboBox(tabSheetVideo);
        comboBoxFPS->addItem(QString());
        comboBoxFPS->addItem(QString());
        comboBoxFPS->addItem(QString());
        comboBoxFPS->addItem(QString());
        comboBoxFPS->addItem(QString());
        comboBoxFPS->setObjectName(QString::fromUtf8("comboBoxFPS"));
        comboBoxFPS->setEnabled(false);
        sizePolicy1.setHeightForWidth(comboBoxFPS->sizePolicy().hasHeightForWidth());
        comboBoxFPS->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(comboBoxFPS, 0, 1, 1, 1);

        checkBoxRemovePulldown = new QCheckBox(tabSheetVideo);
        checkBoxRemovePulldown->setObjectName(QString::fromUtf8("checkBoxRemovePulldown"));
        QSizePolicy sizePolicy4(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(checkBoxRemovePulldown->sizePolicy().hasHeightForWidth());
        checkBoxRemovePulldown->setSizePolicy(sizePolicy4);

        gridLayout->addWidget(checkBoxRemovePulldown, 0, 3, 1, 2);

        checkBoxSecondaryVideo = new QCheckBox(tabSheetVideo);
        checkBoxSecondaryVideo->setObjectName(QString::fromUtf8("checkBoxSecondaryVideo"));

        gridLayout->addWidget(checkBoxSecondaryVideo, 0, 5, 1, 1);

        comboBoxLevel = new QComboBox(tabSheetVideo);
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->addItem(QString());
        comboBoxLevel->setObjectName(QString::fromUtf8("comboBoxLevel"));
        comboBoxLevel->setEnabled(false);
        sizePolicy1.setHeightForWidth(comboBoxLevel->sizePolicy().hasHeightForWidth());
        comboBoxLevel->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(comboBoxLevel, 1, 1, 3, 1);

        labelAR = new QLabel(tabSheetVideo);
        labelAR->setObjectName(QString::fromUtf8("labelAR"));
        labelAR->setEnabled(false);
        QSizePolicy sizePolicy5(QSizePolicy::Minimum, QSizePolicy::Preferred);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(labelAR->sizePolicy().hasHeightForWidth());
        labelAR->setSizePolicy(sizePolicy5);
        labelAR->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(labelAR, 1, 3, 3, 1);

        comboBoxAR = new QComboBox(tabSheetVideo);
        comboBoxAR->addItem(QString());
        comboBoxAR->addItem(QString());
        comboBoxAR->addItem(QString());
        comboBoxAR->addItem(QString());
        comboBoxAR->addItem(QString());
        comboBoxAR->setObjectName(QString::fromUtf8("comboBoxAR"));
        comboBoxAR->setEnabled(false);
        sizePolicy1.setHeightForWidth(comboBoxAR->sizePolicy().hasHeightForWidth());
        comboBoxAR->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(comboBoxAR, 1, 4, 3, 1);

        checkBoxLevel = new QCheckBox(tabSheetVideo);
        checkBoxLevel->setObjectName(QString::fromUtf8("checkBoxLevel"));

        gridLayout->addWidget(checkBoxLevel, 3, 0, 1, 1);

        checkBoxSPS = new QCheckBox(tabSheetVideo);
        checkBoxSPS->setObjectName(QString::fromUtf8("checkBoxSPS"));
        sizePolicy1.setHeightForWidth(checkBoxSPS->sizePolicy().hasHeightForWidth());
        checkBoxSPS->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(checkBoxSPS, 3, 2, 1, 1);

        horizontalSpacer_24 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_24, 0, 6, 2, 1);

        comboBoxSEI = new QComboBox(tabSheetVideo);
        comboBoxSEI->addItem(QString());
        comboBoxSEI->addItem(QString());
        comboBoxSEI->addItem(QString());
        comboBoxSEI->setObjectName(QString::fromUtf8("comboBoxSEI"));
        sizePolicy2.setHeightForWidth(comboBoxSEI->sizePolicy().hasHeightForWidth());
        comboBoxSEI->setSizePolicy(sizePolicy2);

        gridLayout->addWidget(comboBoxSEI, 0, 2, 1, 1);


        horizontalLayout_8->addLayout(gridLayout);

        tabWidgetTracks->addTab(tabSheetVideo, QString());
        tabSheetAudio = new QWidget();
        tabSheetAudio->setObjectName(QString::fromUtf8("tabSheetAudio"));
        horizontalLayout_13 = new QHBoxLayout(tabSheetAudio);
        horizontalLayout_13->setSpacing(6);
        horizontalLayout_13->setContentsMargins(5, 5, 5, 5);
        horizontalLayout_13->setObjectName(QString::fromUtf8("horizontalLayout_13"));
        imageSubtitles = new QLabel(tabSheetAudio);
        imageSubtitles->setObjectName(QString::fromUtf8("imageSubtitles"));
        imageSubtitles->setPixmap(QPixmap(QString::fromUtf8(":/images/track_subtitles.png")));

        horizontalLayout_13->addWidget(imageSubtitles);

        imageAudio = new QLabel(tabSheetAudio);
        imageAudio->setObjectName(QString::fromUtf8("imageAudio"));
        imageAudio->setPixmap(QPixmap(QString::fromUtf8(":/images/track_audio.png")));

        horizontalLayout_13->addWidget(imageAudio);

        gridLayout_2 = new QGridLayout();
        gridLayout_2->setSpacing(6);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        label_18 = new QLabel(tabSheetAudio);
        label_18->setObjectName(QString::fromUtf8("label_18"));
        label_18->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_2->addWidget(label_18, 1, 0, 1, 1);

        horizontalLayout_12 = new QHBoxLayout();
        horizontalLayout_12->setSpacing(6);
        horizontalLayout_12->setObjectName(QString::fromUtf8("horizontalLayout_12"));
        editDelay = new QSpinBox(tabSheetAudio);
        editDelay->setObjectName(QString::fromUtf8("editDelay"));
        QSizePolicy sizePolicy6(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy6.setHorizontalStretch(0);
        sizePolicy6.setVerticalStretch(0);
        sizePolicy6.setHeightForWidth(editDelay->sizePolicy().hasHeightForWidth());
        editDelay->setSizePolicy(sizePolicy6);
        editDelay->setMinimum(-999999);
        editDelay->setMaximum(999999);
        editDelay->setSingleStep(1);

        horizontalLayout_12->addWidget(editDelay);

        horizontalLayout_11 = new QHBoxLayout();
        horizontalLayout_11->setSpacing(6);
        horizontalLayout_11->setObjectName(QString::fromUtf8("horizontalLayout_11"));
        dtsDwnConvert = new QCheckBox(tabSheetAudio);
        dtsDwnConvert->setObjectName(QString::fromUtf8("dtsDwnConvert"));

        horizontalLayout_11->addWidget(dtsDwnConvert);

        secondaryCheckBox = new QCheckBox(tabSheetAudio);
        secondaryCheckBox->setObjectName(QString::fromUtf8("secondaryCheckBox"));

        horizontalLayout_11->addWidget(secondaryCheckBox);

        checkBoxKeepFps = new QCheckBox(tabSheetAudio);
        checkBoxKeepFps->setObjectName(QString::fromUtf8("checkBoxKeepFps"));

        horizontalLayout_11->addWidget(checkBoxKeepFps);

        offsetsLabel = new QLabel(tabSheetAudio);
        offsetsLabel->setObjectName(QString::fromUtf8("offsetsLabel"));

        horizontalLayout_11->addWidget(offsetsLabel);

        offsetsComboBox = new QComboBox(tabSheetAudio);
        offsetsComboBox->setObjectName(QString::fromUtf8("offsetsComboBox"));
        offsetsComboBox->setMinimumSize(QSize(0, 0));
        offsetsComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        horizontalLayout_11->addWidget(offsetsComboBox);

        horizontalSpacer_6 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_11->addItem(horizontalSpacer_6);


        horizontalLayout_12->addLayout(horizontalLayout_11);


        gridLayout_2->addLayout(horizontalLayout_12, 1, 2, 1, 1);

        label_19 = new QLabel(tabSheetAudio);
        label_19->setObjectName(QString::fromUtf8("label_19"));
        label_19->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_2->addWidget(label_19, 2, 0, 1, 1);

        langComboBox = new QComboBox(tabSheetAudio);
        langComboBox->setObjectName(QString::fromUtf8("langComboBox"));

        gridLayout_2->addWidget(langComboBox, 2, 2, 1, 1);


        horizontalLayout_13->addLayout(gridLayout_2);

        tabWidgetTracks->addTab(tabSheetAudio, QString());
        demuxLpcmOptions = new QWidget();
        demuxLpcmOptions->setObjectName(QString::fromUtf8("demuxLpcmOptions"));
        verticalLayout_5 = new QVBoxLayout(demuxLpcmOptions);
        verticalLayout_5->setSpacing(6);
        verticalLayout_5->setContentsMargins(5, 5, 5, 5);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        radioButton_4 = new QRadioButton(demuxLpcmOptions);
        radioButton_4->setObjectName(QString::fromUtf8("radioButton_4"));
        radioButton_4->setEnabled(false);
        radioButton_4->setChecked(true);

        verticalLayout_5->addWidget(radioButton_4);

        radioButton_5 = new QRadioButton(demuxLpcmOptions);
        radioButton_5->setObjectName(QString::fromUtf8("radioButton_5"));
        radioButton_5->setEnabled(false);

        verticalLayout_5->addWidget(radioButton_5);

        radioButton_6 = new QRadioButton(demuxLpcmOptions);
        radioButton_6->setObjectName(QString::fromUtf8("radioButton_6"));
        radioButton_6->setEnabled(false);

        verticalLayout_5->addWidget(radioButton_6);

        tabWidgetTracks->addTab(demuxLpcmOptions, QString());
        tabSheetFake = new QWidget();
        tabSheetFake->setObjectName(QString::fromUtf8("tabSheetFake"));
        tabWidgetTracks->addTab(tabSheetFake, QString());

        verticalLayout_22->addWidget(tabWidgetTracks);

        tabWidget->addTab(GeneralTab, QString());
        tab_3 = new QWidget();
        tab_3->setObjectName(QString::fromUtf8("tab_3"));
        verticalLayout_7 = new QVBoxLayout(tab_3);
        verticalLayout_7->setSpacing(6);
        verticalLayout_7->setContentsMargins(5, 5, 5, 5);
        verticalLayout_7->setObjectName(QString::fromUtf8("verticalLayout_7"));
        gridLayout_14 = new QGridLayout();
        gridLayout_14->setSpacing(6);
        gridLayout_14->setObjectName(QString::fromUtf8("gridLayout_14"));
        groupBox7 = new QGroupBox(tab_3);
        groupBox7->setObjectName(QString::fromUtf8("groupBox7"));
        verticalLayout_17 = new QVBoxLayout(groupBox7);
        verticalLayout_17->setSpacing(6);
        verticalLayout_17->setContentsMargins(5, 5, 5, 5);
        verticalLayout_17->setObjectName(QString::fromUtf8("verticalLayout_17"));
        verticalLayout_14 = new QVBoxLayout();
        verticalLayout_14->setSpacing(6);
        verticalLayout_14->setObjectName(QString::fromUtf8("verticalLayout_14"));
        horizontalLayout_18 = new QHBoxLayout();
        horizontalLayout_18->setSpacing(6);
        horizontalLayout_18->setObjectName(QString::fromUtf8("horizontalLayout_18"));
        checkBoxVBR = new QRadioButton(groupBox7);
        checkBoxVBR->setObjectName(QString::fromUtf8("checkBoxVBR"));
        checkBoxVBR->setChecked(true);

        horizontalLayout_18->addWidget(checkBoxVBR);


        verticalLayout_14->addLayout(horizontalLayout_18);

        horizontalLayout_17 = new QHBoxLayout();
        horizontalLayout_17->setSpacing(6);
        horizontalLayout_17->setObjectName(QString::fromUtf8("horizontalLayout_17"));
        checkBoxRVBR = new QRadioButton(groupBox7);
        checkBoxRVBR->setObjectName(QString::fromUtf8("checkBoxRVBR"));

        horizontalLayout_17->addWidget(checkBoxRVBR);


        verticalLayout_14->addLayout(horizontalLayout_17);

        horizontalLayout_19 = new QHBoxLayout();
        horizontalLayout_19->setSpacing(6);
        horizontalLayout_19->setObjectName(QString::fromUtf8("horizontalLayout_19"));
        gridLayout_3 = new QGridLayout();
        gridLayout_3->setSpacing(6);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        label_5 = new QLabel(groupBox7);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setMinimumSize(QSize(105, 0));
        label_5->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_3->addWidget(label_5, 0, 0, 1, 1);

        editMaxBitrate = new QDoubleSpinBox(groupBox7);
        editMaxBitrate->setObjectName(QString::fromUtf8("editMaxBitrate"));
        editMaxBitrate->setEnabled(false);
        editMaxBitrate->setDecimals(3);
        editMaxBitrate->setMaximum(99999.990000000005239);
        editMaxBitrate->setValue(99.989999999999995);

        gridLayout_3->addWidget(editMaxBitrate, 0, 1, 1, 1);

        label_6 = new QLabel(groupBox7);
        label_6->setObjectName(QString::fromUtf8("label_6"));
        label_6->setMinimumSize(QSize(105, 0));
        label_6->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_3->addWidget(label_6, 1, 0, 1, 1);

        editMinBitrate = new QDoubleSpinBox(groupBox7);
        editMinBitrate->setObjectName(QString::fromUtf8("editMinBitrate"));
        editMinBitrate->setEnabled(false);
        editMinBitrate->setDecimals(3);
        editMinBitrate->setMaximum(99999.990000000005239);

        gridLayout_3->addWidget(editMinBitrate, 1, 1, 1, 1);

        horizontalSpacer_7 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_7, 0, 2, 1, 1);

        horizontalSpacer_8 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_8, 1, 2, 1, 1);


        horizontalLayout_19->addLayout(gridLayout_3);


        verticalLayout_14->addLayout(horizontalLayout_19);

        horizontalLayout_16 = new QHBoxLayout();
        horizontalLayout_16->setSpacing(6);
        horizontalLayout_16->setObjectName(QString::fromUtf8("horizontalLayout_16"));
        checkBoxCBR = new QRadioButton(groupBox7);
        checkBoxCBR->setObjectName(QString::fromUtf8("checkBoxCBR"));

        horizontalLayout_16->addWidget(checkBoxCBR);


        verticalLayout_14->addLayout(horizontalLayout_16);

        horizontalLayout_14 = new QHBoxLayout();
        horizontalLayout_14->setSpacing(6);
        horizontalLayout_14->setObjectName(QString::fromUtf8("horizontalLayout_14"));
        gridLayout_4 = new QGridLayout();
        gridLayout_4->setSpacing(6);
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        label_7 = new QLabel(groupBox7);
        label_7->setObjectName(QString::fromUtf8("label_7"));
        label_7->setMinimumSize(QSize(105, 0));
        label_7->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_4->addWidget(label_7, 0, 0, 1, 1);

        editCBRBitrate = new QDoubleSpinBox(groupBox7);
        editCBRBitrate->setObjectName(QString::fromUtf8("editCBRBitrate"));
        editCBRBitrate->setEnabled(false);
        editCBRBitrate->setDecimals(3);
        editCBRBitrate->setMaximum(99999.990000000005239);
        editCBRBitrate->setValue(8000.000000000000000);

        gridLayout_4->addWidget(editCBRBitrate, 0, 1, 1, 1);

        horizontalSpacer_9 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_4->addItem(horizontalSpacer_9, 0, 2, 1, 1);


        horizontalLayout_14->addLayout(gridLayout_4);


        verticalLayout_14->addLayout(horizontalLayout_14);

        horizontalLayout_15 = new QHBoxLayout();
        horizontalLayout_15->setSpacing(6);
        horizontalLayout_15->setObjectName(QString::fromUtf8("horizontalLayout_15"));
        label_8 = new QLabel(groupBox7);
        label_8->setObjectName(QString::fromUtf8("label_8"));

        horizontalLayout_15->addWidget(label_8);

        editVBVLen = new QSpinBox(groupBox7);
        editVBVLen->setObjectName(QString::fromUtf8("editVBVLen"));
        editVBVLen->setMaximum(60000);
        editVBVLen->setValue(500);

        horizontalLayout_15->addWidget(editVBVLen);

        horizontalSpacer_10 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_15->addItem(horizontalSpacer_10);


        verticalLayout_14->addLayout(horizontalLayout_15);


        verticalLayout_17->addLayout(verticalLayout_14);

        verticalSpacer_2 = new QSpacerItem(20, 77, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_17->addItem(verticalSpacer_2);


        gridLayout_14->addWidget(groupBox7, 0, 0, 2, 1);

        groupBox_5 = new QGroupBox(tab_3);
        groupBox_5->setObjectName(QString::fromUtf8("groupBox_5"));
        sizePolicy3.setHeightForWidth(groupBox_5->sizePolicy().hasHeightForWidth());
        groupBox_5->setSizePolicy(sizePolicy3);
        verticalLayout_18 = new QVBoxLayout(groupBox_5);
        verticalLayout_18->setSpacing(6);
        verticalLayout_18->setContentsMargins(5, 5, 5, 5);
        verticalLayout_18->setObjectName(QString::fromUtf8("verticalLayout_18"));
        checkBoxSound = new QCheckBox(groupBox_5);
        checkBoxSound->setObjectName(QString::fromUtf8("checkBoxSound"));
        checkBoxSound->setChecked(true);

        verticalLayout_18->addWidget(checkBoxSound);

        checkBoxNewAudioPes = new QCheckBox(groupBox_5);
        checkBoxNewAudioPes->setObjectName(QString::fromUtf8("checkBoxNewAudioPes"));
        checkBoxNewAudioPes->setChecked(true);

        verticalLayout_18->addWidget(checkBoxNewAudioPes);

        checkBoxCrop = new QCheckBox(groupBox_5);
        checkBoxCrop->setObjectName(QString::fromUtf8("checkBoxCrop"));
        checkBoxCrop->setEnabled(false);

        verticalLayout_18->addWidget(checkBoxCrop);


        gridLayout_14->addWidget(groupBox_5, 0, 1, 1, 1);

        GroupBox114 = new QGroupBox(tab_3);
        GroupBox114->setObjectName(QString::fromUtf8("GroupBox114"));
        verticalLayout_24 = new QVBoxLayout(GroupBox114);
        verticalLayout_24->setSpacing(6);
        verticalLayout_24->setContentsMargins(5, 5, 5, 5);
        verticalLayout_24->setObjectName(QString::fromUtf8("verticalLayout_24"));
        radioButtonStoreOutput = new QRadioButton(GroupBox114);
        radioButtonStoreOutput->setObjectName(QString::fromUtf8("radioButtonStoreOutput"));
        radioButtonStoreOutput->setChecked(true);

        verticalLayout_24->addWidget(radioButtonStoreOutput);

        radioButtonOutoutInInput = new QRadioButton(GroupBox114);
        radioButtonOutoutInInput->setObjectName(QString::fromUtf8("radioButtonOutoutInInput"));

        verticalLayout_24->addWidget(radioButtonOutoutInInput);

        verticalSpacer_6 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_24->addItem(verticalSpacer_6);


        gridLayout_14->addWidget(GroupBox114, 1, 1, 1, 1);


        verticalLayout_7->addLayout(gridLayout_14);

        tabWidget->addTab(tab_3, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName(QString::fromUtf8("tab_2"));
        horizontalLayout_5 = new QHBoxLayout(tab_2);
        horizontalLayout_5->setSpacing(6);
        horizontalLayout_5->setContentsMargins(5, 5, 5, 5);
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        groupBox_3 = new QGroupBox(tab_2);
        groupBox_3->setObjectName(QString::fromUtf8("groupBox_3"));
        verticalLayout_11 = new QVBoxLayout(groupBox_3);
        verticalLayout_11->setSpacing(6);
        verticalLayout_11->setContentsMargins(5, 5, 5, 5);
        verticalLayout_11->setObjectName(QString::fromUtf8("verticalLayout_11"));
        radioButtonNoChapters = new QRadioButton(groupBox_3);
        radioButtonNoChapters->setObjectName(QString::fromUtf8("radioButtonNoChapters"));

        verticalLayout_11->addWidget(radioButtonNoChapters);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setSpacing(6);
        horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
        radioButtonAutoChapter = new QRadioButton(groupBox_3);
        radioButtonAutoChapter->setObjectName(QString::fromUtf8("radioButtonAutoChapter"));
        radioButtonAutoChapter->setChecked(true);

        horizontalLayout_6->addWidget(radioButtonAutoChapter);

        spinEditChapterLen = new QSpinBox(groupBox_3);
        spinEditChapterLen->setObjectName(QString::fromUtf8("spinEditChapterLen"));
        spinEditChapterLen->setMinimum(1);
        spinEditChapterLen->setMaximum(9999);
        spinEditChapterLen->setValue(5);

        horizontalLayout_6->addWidget(spinEditChapterLen);

        label_4 = new QLabel(groupBox_3);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        horizontalLayout_6->addWidget(label_4);


        verticalLayout_11->addLayout(horizontalLayout_6);

        radioButtonCustomChapters = new QRadioButton(groupBox_3);
        radioButtonCustomChapters->setObjectName(QString::fromUtf8("radioButtonCustomChapters"));

        verticalLayout_11->addWidget(radioButtonCustomChapters);

        label_9 = new QLabel(groupBox_3);
        label_9->setObjectName(QString::fromUtf8("label_9"));

        verticalLayout_11->addWidget(label_9);

        memoChapters = new QTextEdit(groupBox_3);
        memoChapters->setObjectName(QString::fromUtf8("memoChapters"));
        memoChapters->setEnabled(false);
        memoChapters->setTabChangesFocus(true);
        memoChapters->setTabStopWidth(20);
        memoChapters->setAcceptRichText(false);

        verticalLayout_11->addWidget(memoChapters);


        horizontalLayout_5->addWidget(groupBox_3);

        BDOptionsGroupBox = new QGroupBox(tab_2);
        BDOptionsGroupBox->setObjectName(QString::fromUtf8("BDOptionsGroupBox"));
        verticalLayout_19 = new QVBoxLayout(BDOptionsGroupBox);
        verticalLayout_19->setSpacing(6);
        verticalLayout_19->setContentsMargins(5, 5, 5, 5);
        verticalLayout_19->setObjectName(QString::fromUtf8("verticalLayout_19"));
        checkBoxBlankPL = new QCheckBox(BDOptionsGroupBox);
        checkBoxBlankPL->setObjectName(QString::fromUtf8("checkBoxBlankPL"));
        checkBoxBlankPL->setChecked(true);

        verticalLayout_19->addWidget(checkBoxBlankPL);

        horizontalLayout_32 = new QHBoxLayout();
        horizontalLayout_32->setSpacing(6);
        horizontalLayout_32->setObjectName(QString::fromUtf8("horizontalLayout_32"));
        horizontalSpacer_22 = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_32->addItem(horizontalSpacer_22);

        BlackplaylistLabel = new QLabel(BDOptionsGroupBox);
        BlackplaylistLabel->setObjectName(QString::fromUtf8("BlackplaylistLabel"));

        horizontalLayout_32->addWidget(BlackplaylistLabel);

        BlackplaylistCombo = new QSpinBox(BDOptionsGroupBox);
        BlackplaylistCombo->setObjectName(QString::fromUtf8("BlackplaylistCombo"));
        BlackplaylistCombo->setMinimumSize(QSize(18, 0));
        BlackplaylistCombo->setMaximum(1999);
        BlackplaylistCombo->setValue(1900);

        horizontalLayout_32->addWidget(BlackplaylistCombo);

        horizontalSpacer_23 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_32->addItem(horizontalSpacer_23);


        verticalLayout_19->addLayout(horizontalLayout_32);

        gridLayout_10 = new QGridLayout();
        gridLayout_10->setSpacing(6);
        gridLayout_10->setObjectName(QString::fromUtf8("gridLayout_10"));
        label = new QLabel(BDOptionsGroupBox);
        label->setObjectName(QString::fromUtf8("label"));
        label->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        gridLayout_10->addWidget(label, 0, 0, 1, 1);

        spinBoxMplsNum = new QSpinBox(BDOptionsGroupBox);
        spinBoxMplsNum->setObjectName(QString::fromUtf8("spinBoxMplsNum"));
        sizePolicy1.setHeightForWidth(spinBoxMplsNum->sizePolicy().hasHeightForWidth());
        spinBoxMplsNum->setSizePolicy(sizePolicy1);
        spinBoxMplsNum->setMaximum(1999);

        gridLayout_10->addWidget(spinBoxMplsNum, 0, 1, 1, 1);

        label_10 = new QLabel(BDOptionsGroupBox);
        label_10->setObjectName(QString::fromUtf8("label_10"));
        label_10->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        gridLayout_10->addWidget(label_10, 1, 0, 1, 1);

        spinBoxM2tsNum = new QSpinBox(BDOptionsGroupBox);
        spinBoxM2tsNum->setObjectName(QString::fromUtf8("spinBoxM2tsNum"));
        sizePolicy1.setHeightForWidth(spinBoxM2tsNum->sizePolicy().hasHeightForWidth());
        spinBoxM2tsNum->setSizePolicy(sizePolicy1);
        spinBoxM2tsNum->setMaximum(99999);

        gridLayout_10->addWidget(spinBoxM2tsNum, 1, 1, 1, 1);

        muxTimeEdit = new QTimeEdit(BDOptionsGroupBox);
        muxTimeEdit->setObjectName(QString::fromUtf8("muxTimeEdit"));

        gridLayout_10->addWidget(muxTimeEdit, 2, 1, 1, 1);

        label_20 = new QLabel(BDOptionsGroupBox);
        label_20->setObjectName(QString::fromUtf8("label_20"));
        label_20->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_10->addWidget(label_20, 2, 0, 1, 1);

        muxTimeClock = new QSpinBox(BDOptionsGroupBox);
        muxTimeClock->setObjectName(QString::fromUtf8("muxTimeClock"));
        muxTimeClock->setButtonSymbols(QAbstractSpinBox::NoButtons);
        muxTimeClock->setMaximum(2147483647);

        gridLayout_10->addWidget(muxTimeClock, 2, 3, 1, 1);

        label_23 = new QLabel(BDOptionsGroupBox);
        label_23->setObjectName(QString::fromUtf8("label_23"));
        label_23->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_10->addWidget(label_23, 2, 2, 1, 1);

        horizontalSpacer_19 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_10->addItem(horizontalSpacer_19, 2, 4, 1, 1);


        verticalLayout_19->addLayout(gridLayout_10);

        groupBox_11 = new QGroupBox(BDOptionsGroupBox);
        groupBox_11->setObjectName(QString::fromUtf8("groupBox_11"));
        verticalLayout_20 = new QVBoxLayout(groupBox_11);
        verticalLayout_20->setSpacing(6);
        verticalLayout_20->setContentsMargins(5, 5, 5, 5);
        verticalLayout_20->setObjectName(QString::fromUtf8("verticalLayout_20"));
        rightEyeCheckBox = new QCheckBox(groupBox_11);
        rightEyeCheckBox->setObjectName(QString::fromUtf8("rightEyeCheckBox"));

        verticalLayout_20->addWidget(rightEyeCheckBox);


        verticalLayout_19->addWidget(groupBox_11);

        groupBoxPip = new QGroupBox(BDOptionsGroupBox);
        groupBoxPip->setObjectName(QString::fromUtf8("groupBoxPip"));
        groupBoxPip->setMinimumSize(QSize(0, 120));
        verticalLayout_23 = new QVBoxLayout(groupBoxPip);
        verticalLayout_23->setSpacing(6);
        verticalLayout_23->setContentsMargins(5, 5, 5, 5);
        verticalLayout_23->setObjectName(QString::fromUtf8("verticalLayout_23"));
        gridLayout_13 = new QGridLayout();
        gridLayout_13->setSpacing(6);
        gridLayout_13->setObjectName(QString::fromUtf8("gridLayout_13"));
        label_24 = new QLabel(groupBoxPip);
        label_24->setObjectName(QString::fromUtf8("label_24"));
        label_24->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_13->addWidget(label_24, 0, 0, 1, 1);

        comboBoxPipCorner = new QComboBox(groupBoxPip);
        comboBoxPipCorner->addItem(QString());
        comboBoxPipCorner->addItem(QString());
        comboBoxPipCorner->addItem(QString());
        comboBoxPipCorner->addItem(QString());
        comboBoxPipCorner->setObjectName(QString::fromUtf8("comboBoxPipCorner"));

        gridLayout_13->addWidget(comboBoxPipCorner, 0, 1, 1, 1);

        label_25 = new QLabel(groupBoxPip);
        label_25->setObjectName(QString::fromUtf8("label_25"));
        label_25->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_13->addWidget(label_25, 0, 2, 1, 1);

        spinBoxPipOffsetH = new QSpinBox(groupBoxPip);
        spinBoxPipOffsetH->setObjectName(QString::fromUtf8("spinBoxPipOffsetH"));
        spinBoxPipOffsetH->setMaximum(9999);

        gridLayout_13->addWidget(spinBoxPipOffsetH, 0, 3, 1, 1);

        label_26 = new QLabel(groupBoxPip);
        label_26->setObjectName(QString::fromUtf8("label_26"));
        label_26->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_13->addWidget(label_26, 1, 0, 1, 1);

        comboBoxPipSize = new QComboBox(groupBoxPip);
        comboBoxPipSize->addItem(QString());
        comboBoxPipSize->addItem(QString());
        comboBoxPipSize->addItem(QString());
        comboBoxPipSize->addItem(QString());
        comboBoxPipSize->addItem(QString());
        comboBoxPipSize->setObjectName(QString::fromUtf8("comboBoxPipSize"));

        gridLayout_13->addWidget(comboBoxPipSize, 1, 1, 1, 1);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_13->addItem(horizontalSpacer_3, 0, 4, 1, 1);

        label_27 = new QLabel(groupBoxPip);
        label_27->setObjectName(QString::fromUtf8("label_27"));
        label_27->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_13->addWidget(label_27, 1, 2, 1, 1);

        spinBoxPipOffsetV = new QSpinBox(groupBoxPip);
        spinBoxPipOffsetV->setObjectName(QString::fromUtf8("spinBoxPipOffsetV"));
        spinBoxPipOffsetV->setMaximum(9999);

        gridLayout_13->addWidget(spinBoxPipOffsetV, 1, 3, 1, 1);


        verticalLayout_23->addLayout(gridLayout_13);


        verticalLayout_19->addWidget(groupBoxPip);

        verticalSpacer_9 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_19->addItem(verticalSpacer_9);


        horizontalLayout_5->addWidget(BDOptionsGroupBox);

        tabWidget->addTab(tab_2, QString());
        tab = new QWidget();
        tab->setObjectName(QString::fromUtf8("tab"));
        verticalLayout_8 = new QVBoxLayout(tab);
        verticalLayout_8->setSpacing(6);
        verticalLayout_8->setContentsMargins(5, 5, 5, 5);
        verticalLayout_8->setObjectName(QString::fromUtf8("verticalLayout_8"));
        groupBox_6 = new QGroupBox(tab);
        groupBox_6->setObjectName(QString::fromUtf8("groupBox_6"));
        QSizePolicy sizePolicy7(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy7.setHorizontalStretch(0);
        sizePolicy7.setVerticalStretch(0);
        sizePolicy7.setHeightForWidth(groupBox_6->sizePolicy().hasHeightForWidth());
        groupBox_6->setSizePolicy(sizePolicy7);
        horizontalLayout_21 = new QHBoxLayout(groupBox_6);
        horizontalLayout_21->setSpacing(6);
        horizontalLayout_21->setContentsMargins(5, 5, 5, 5);
        horizontalLayout_21->setObjectName(QString::fromUtf8("horizontalLayout_21"));
        gridLayout_5 = new QGridLayout();
        gridLayout_5->setSpacing(6);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        noSplit = new QRadioButton(groupBox_6);
        noSplit->setObjectName(QString::fromUtf8("noSplit"));
        noSplit->setChecked(true);

        gridLayout_5->addWidget(noSplit, 0, 0, 1, 1);

        splitByDuration = new QRadioButton(groupBox_6);
        splitByDuration->setObjectName(QString::fromUtf8("splitByDuration"));

        gridLayout_5->addWidget(splitByDuration, 1, 0, 1, 1);

        spinEditSplitDuration = new QSpinBox(groupBox_6);
        spinEditSplitDuration->setObjectName(QString::fromUtf8("spinEditSplitDuration"));
        spinEditSplitDuration->setEnabled(false);
        spinEditSplitDuration->setMinimum(1);
        spinEditSplitDuration->setMaximum(99999);
        spinEditSplitDuration->setValue(60);

        gridLayout_5->addWidget(spinEditSplitDuration, 1, 1, 1, 1);

        labelSplitByDur = new QLabel(groupBox_6);
        labelSplitByDur->setObjectName(QString::fromUtf8("labelSplitByDur"));
        labelSplitByDur->setEnabled(false);

        gridLayout_5->addWidget(labelSplitByDur, 1, 2, 1, 1);

        splitBySize = new QRadioButton(groupBox_6);
        splitBySize->setObjectName(QString::fromUtf8("splitBySize"));

        gridLayout_5->addWidget(splitBySize, 2, 0, 1, 1);

        editSplitSize = new QDoubleSpinBox(groupBox_6);
        editSplitSize->setObjectName(QString::fromUtf8("editSplitSize"));
        editSplitSize->setEnabled(false);
        editSplitSize->setDecimals(3);
        editSplitSize->setMinimum(0.001000000000000);
        editSplitSize->setMaximum(99999.990000000005239);
        editSplitSize->setValue(1.000000000000000);

        gridLayout_5->addWidget(editSplitSize, 2, 1, 1, 1);

        comboBoxMeasure = new QComboBox(groupBox_6);
        comboBoxMeasure->addItem(QString());
        comboBoxMeasure->addItem(QString());
        comboBoxMeasure->addItem(QString());
        comboBoxMeasure->addItem(QString());
        comboBoxMeasure->addItem(QString());
        comboBoxMeasure->addItem(QString());
        comboBoxMeasure->setObjectName(QString::fromUtf8("comboBoxMeasure"));
        comboBoxMeasure->setEnabled(false);

        gridLayout_5->addWidget(comboBoxMeasure, 2, 2, 1, 1);


        horizontalLayout_21->addLayout(gridLayout_5);

        horizontalSpacer_16 = new QSpacerItem(245, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_21->addItem(horizontalSpacer_16);


        verticalLayout_8->addWidget(groupBox_6);

        groupBox_7 = new QGroupBox(tab);
        groupBox_7->setObjectName(QString::fromUtf8("groupBox_7"));
        sizePolicy7.setHeightForWidth(groupBox_7->sizePolicy().hasHeightForWidth());
        groupBox_7->setSizePolicy(sizePolicy7);
        horizontalLayout_23 = new QHBoxLayout(groupBox_7);
        horizontalLayout_23->setSpacing(6);
        horizontalLayout_23->setContentsMargins(5, 5, 5, 5);
        horizontalLayout_23->setObjectName(QString::fromUtf8("horizontalLayout_23"));
        verticalLayout_15 = new QVBoxLayout();
        verticalLayout_15->setSpacing(6);
        verticalLayout_15->setObjectName(QString::fromUtf8("verticalLayout_15"));
        checkBoxCut = new QCheckBox(groupBox_7);
        checkBoxCut->setObjectName(QString::fromUtf8("checkBoxCut"));

        verticalLayout_15->addWidget(checkBoxCut);

        horizontalLayout_22 = new QHBoxLayout();
        horizontalLayout_22->setSpacing(6);
        horizontalLayout_22->setObjectName(QString::fromUtf8("horizontalLayout_22"));
        gridLayout_6 = new QGridLayout();
        gridLayout_6->setSpacing(6);
        gridLayout_6->setObjectName(QString::fromUtf8("gridLayout_6"));
        label_11 = new QLabel(groupBox_7);
        label_11->setObjectName(QString::fromUtf8("label_11"));
        label_11->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        gridLayout_6->addWidget(label_11, 0, 0, 1, 1);

        label_12 = new QLabel(groupBox_7);
        label_12->setObjectName(QString::fromUtf8("label_12"));
        label_12->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        gridLayout_6->addWidget(label_12, 1, 0, 1, 1);

        cutStartTimeEdit = new QTimeEdit(groupBox_7);
        cutStartTimeEdit->setObjectName(QString::fromUtf8("cutStartTimeEdit"));
        cutStartTimeEdit->setEnabled(false);

        gridLayout_6->addWidget(cutStartTimeEdit, 0, 2, 1, 1);

        cutEndTimeEdit = new QTimeEdit(groupBox_7);
        cutEndTimeEdit->setObjectName(QString::fromUtf8("cutEndTimeEdit"));
        cutEndTimeEdit->setEnabled(false);

        gridLayout_6->addWidget(cutEndTimeEdit, 1, 2, 1, 1);

        label_28 = new QLabel(groupBox_7);
        label_28->setObjectName(QString::fromUtf8("label_28"));

        gridLayout_6->addWidget(label_28, 0, 3, 1, 1);

        label_29 = new QLabel(groupBox_7);
        label_29->setObjectName(QString::fromUtf8("label_29"));

        gridLayout_6->addWidget(label_29, 1, 3, 1, 1);


        horizontalLayout_22->addLayout(gridLayout_6);


        verticalLayout_15->addLayout(horizontalLayout_22);


        horizontalLayout_23->addLayout(verticalLayout_15);

        horizontalSpacer_18 = new QSpacerItem(331, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_23->addItem(horizontalSpacer_18);


        verticalLayout_8->addWidget(groupBox_7);

        verticalSpacer_5 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_8->addItem(verticalSpacer_5);

        tabWidget->addTab(tab, QString());
        tab_4 = new QWidget();
        tab_4->setObjectName(QString::fromUtf8("tab_4"));
        verticalLayout_9 = new QVBoxLayout(tab_4);
        verticalLayout_9->setSpacing(6);
        verticalLayout_9->setContentsMargins(5, 5, 5, 5);
        verticalLayout_9->setObjectName(QString::fromUtf8("verticalLayout_9"));
        groupBox_8 = new QGroupBox(tab_4);
        groupBox_8->setObjectName(QString::fromUtf8("groupBox_8"));
        sizePolicy7.setHeightForWidth(groupBox_8->sizePolicy().hasHeightForWidth());
        groupBox_8->setSizePolicy(sizePolicy7);
        horizontalLayout_25 = new QHBoxLayout(groupBox_8);
        horizontalLayout_25->setSpacing(6);
        horizontalLayout_25->setContentsMargins(5, 5, 5, 5);
        horizontalLayout_25->setObjectName(QString::fromUtf8("horizontalLayout_25"));
        gridLayout_7 = new QGridLayout();
        gridLayout_7->setSpacing(6);
        gridLayout_7->setObjectName(QString::fromUtf8("gridLayout_7"));
        listViewFont = new QTableWidget(groupBox_8);
        if (listViewFont->columnCount() < 2)
            listViewFont->setColumnCount(2);
        QTableWidgetItem *__qtablewidgetitem5 = new QTableWidgetItem();
        listViewFont->setHorizontalHeaderItem(0, __qtablewidgetitem5);
        QTableWidgetItem *__qtablewidgetitem6 = new QTableWidgetItem();
        listViewFont->setHorizontalHeaderItem(1, __qtablewidgetitem6);
        if (listViewFont->rowCount() < 5)
            listViewFont->setRowCount(5);
        QTableWidgetItem *__qtablewidgetitem7 = new QTableWidgetItem();
        listViewFont->setVerticalHeaderItem(0, __qtablewidgetitem7);
        QTableWidgetItem *__qtablewidgetitem8 = new QTableWidgetItem();
        listViewFont->setVerticalHeaderItem(1, __qtablewidgetitem8);
        QTableWidgetItem *__qtablewidgetitem9 = new QTableWidgetItem();
        listViewFont->setVerticalHeaderItem(2, __qtablewidgetitem9);
        QTableWidgetItem *__qtablewidgetitem10 = new QTableWidgetItem();
        listViewFont->setVerticalHeaderItem(3, __qtablewidgetitem10);
        QTableWidgetItem *__qtablewidgetitem11 = new QTableWidgetItem();
        listViewFont->setVerticalHeaderItem(4, __qtablewidgetitem11);
        QTableWidgetItem *__qtablewidgetitem12 = new QTableWidgetItem();
        listViewFont->setItem(0, 0, __qtablewidgetitem12);
        QTableWidgetItem *__qtablewidgetitem13 = new QTableWidgetItem();
        listViewFont->setItem(0, 1, __qtablewidgetitem13);
        QTableWidgetItem *__qtablewidgetitem14 = new QTableWidgetItem();
        listViewFont->setItem(1, 0, __qtablewidgetitem14);
        QTableWidgetItem *__qtablewidgetitem15 = new QTableWidgetItem();
        listViewFont->setItem(1, 1, __qtablewidgetitem15);
        QTableWidgetItem *__qtablewidgetitem16 = new QTableWidgetItem();
        listViewFont->setItem(2, 0, __qtablewidgetitem16);
        QTableWidgetItem *__qtablewidgetitem17 = new QTableWidgetItem();
        listViewFont->setItem(2, 1, __qtablewidgetitem17);
        QTableWidgetItem *__qtablewidgetitem18 = new QTableWidgetItem();
        listViewFont->setItem(3, 0, __qtablewidgetitem18);
        QTableWidgetItem *__qtablewidgetitem19 = new QTableWidgetItem();
        listViewFont->setItem(3, 1, __qtablewidgetitem19);
        QTableWidgetItem *__qtablewidgetitem20 = new QTableWidgetItem();
        listViewFont->setItem(4, 0, __qtablewidgetitem20);
        QTableWidgetItem *__qtablewidgetitem21 = new QTableWidgetItem();
        listViewFont->setItem(4, 1, __qtablewidgetitem21);
        listViewFont->setObjectName(QString::fromUtf8("listViewFont"));
        QSizePolicy sizePolicy8(QSizePolicy::Expanding, QSizePolicy::Minimum);
        sizePolicy8.setHorizontalStretch(0);
        sizePolicy8.setVerticalStretch(0);
        sizePolicy8.setHeightForWidth(listViewFont->sizePolicy().hasHeightForWidth());
        listViewFont->setSizePolicy(sizePolicy8);
        listViewFont->setMaximumSize(QSize(16777215, 84));

        gridLayout_7->addWidget(listViewFont, 1, 0, 1, 1);

        verticalLayout_16 = new QVBoxLayout();
        verticalLayout_16->setSpacing(6);
        verticalLayout_16->setObjectName(QString::fromUtf8("verticalLayout_16"));
        fontButton = new QPushButton(groupBox_8);
        fontButton->setObjectName(QString::fromUtf8("fontButton"));
        sizePolicy1.setHeightForWidth(fontButton->sizePolicy().hasHeightForWidth());
        fontButton->setSizePolicy(sizePolicy1);

        verticalLayout_16->addWidget(fontButton);

        colorButton = new QPushButton(groupBox_8);
        colorButton->setObjectName(QString::fromUtf8("colorButton"));

        verticalLayout_16->addWidget(colorButton);

        verticalSpacer_8 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_16->addItem(verticalSpacer_8);


        gridLayout_7->addLayout(verticalLayout_16, 1, 1, 1, 1);

        gridLayout_12 = new QGridLayout();
        gridLayout_12->setSpacing(6);
        gridLayout_12->setObjectName(QString::fromUtf8("gridLayout_12"));
        label_13 = new QLabel(groupBox_8);
        label_13->setObjectName(QString::fromUtf8("label_13"));
        label_13->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_12->addWidget(label_13, 0, 0, 1, 1);

        spinEditBorder = new QSpinBox(groupBox_8);
        spinEditBorder->setObjectName(QString::fromUtf8("spinEditBorder"));
        spinEditBorder->setMinimumSize(QSize(0, 23));
        spinEditBorder->setMaximum(9);
        spinEditBorder->setValue(5);

        gridLayout_12->addWidget(spinEditBorder, 0, 1, 1, 1);

        label_21 = new QLabel(groupBox_8);
        label_21->setObjectName(QString::fromUtf8("label_21"));

        gridLayout_12->addWidget(label_21, 0, 2, 1, 1);

        lineSpacing = new QDoubleSpinBox(groupBox_8);
        lineSpacing->setObjectName(QString::fromUtf8("lineSpacing"));
        lineSpacing->setDecimals(1);
        lineSpacing->setMinimum(0.500000000000000);
        lineSpacing->setMaximum(2.000000000000000);
        lineSpacing->setSingleStep(0.100000000000000);
        lineSpacing->setValue(1.000000000000000);

        gridLayout_12->addWidget(lineSpacing, 0, 3, 1, 1);

        label_22 = new QLabel(groupBox_8);
        label_22->setObjectName(QString::fromUtf8("label_22"));
        label_22->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_12->addWidget(label_22, 1, 0, 1, 1);

        comboBoxAnimation = new QComboBox(groupBox_8);
        comboBoxAnimation->addItem(QString());
        comboBoxAnimation->addItem(QString());
        comboBoxAnimation->addItem(QString());
        comboBoxAnimation->addItem(QString());
        comboBoxAnimation->addItem(QString());
        comboBoxAnimation->setObjectName(QString::fromUtf8("comboBoxAnimation"));

        gridLayout_12->addWidget(comboBoxAnimation, 1, 1, 1, 1);


        gridLayout_7->addLayout(gridLayout_12, 2, 0, 1, 1);


        horizontalLayout_25->addLayout(gridLayout_7);

        horizontalSpacer_20 = new QSpacerItem(168, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_25->addItem(horizontalSpacer_20);


        verticalLayout_9->addWidget(groupBox_8);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setSpacing(6);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        groupBox_9 = new QGroupBox(tab_4);
        groupBox_9->setObjectName(QString::fromUtf8("groupBox_9"));
        sizePolicy3.setHeightForWidth(groupBox_9->sizePolicy().hasHeightForWidth());
        groupBox_9->setSizePolicy(sizePolicy3);
        horizontalLayout_28 = new QHBoxLayout(groupBox_9);
        horizontalLayout_28->setSpacing(6);
        horizontalLayout_28->setContentsMargins(5, 5, 5, 5);
        horizontalLayout_28->setObjectName(QString::fromUtf8("horizontalLayout_28"));
        gridLayout_9 = new QGridLayout();
        gridLayout_9->setSpacing(6);
        gridLayout_9->setObjectName(QString::fromUtf8("gridLayout_9"));
        radioButton = new QRadioButton(groupBox_9);
        radioButton->setObjectName(QString::fromUtf8("radioButton"));
        radioButton->setEnabled(false);

        gridLayout_9->addWidget(radioButton, 0, 0, 1, 1);

        horizontalLayout_26 = new QHBoxLayout();
        horizontalLayout_26->setSpacing(6);
        horizontalLayout_26->setObjectName(QString::fromUtf8("horizontalLayout_26"));
        horizontalSpacer_11 = new QSpacerItem(15, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_26->addItem(horizontalSpacer_11);

        label_14 = new QLabel(groupBox_9);
        label_14->setObjectName(QString::fromUtf8("label_14"));
        label_14->setEnabled(false);
        sizePolicy3.setHeightForWidth(label_14->sizePolicy().hasHeightForWidth());
        label_14->setSizePolicy(sizePolicy3);
        label_14->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_26->addWidget(label_14);


        gridLayout_9->addLayout(horizontalLayout_26, 1, 0, 1, 1);

        spinBox = new QSpinBox(groupBox_9);
        spinBox->setObjectName(QString::fromUtf8("spinBox"));
        spinBox->setEnabled(false);
        sizePolicy6.setHeightForWidth(spinBox->sizePolicy().hasHeightForWidth());
        spinBox->setSizePolicy(sizePolicy6);
        spinBox->setMaximum(1080);
        spinBox->setValue(24);

        gridLayout_9->addWidget(spinBox, 1, 1, 1, 1);

        radioButton_2 = new QRadioButton(groupBox_9);
        radioButton_2->setObjectName(QString::fromUtf8("radioButton_2"));
        radioButton_2->setEnabled(false);

        gridLayout_9->addWidget(radioButton_2, 2, 0, 1, 1);

        radioButton_3 = new QRadioButton(groupBox_9);
        radioButton_3->setObjectName(QString::fromUtf8("radioButton_3"));
        radioButton_3->setChecked(true);

        gridLayout_9->addWidget(radioButton_3, 3, 0, 1, 1);

        horizontalLayout_27 = new QHBoxLayout();
        horizontalLayout_27->setSpacing(6);
        horizontalLayout_27->setObjectName(QString::fromUtf8("horizontalLayout_27"));
        horizontalSpacer_13 = new QSpacerItem(15, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_27->addItem(horizontalSpacer_13);

        label_15 = new QLabel(groupBox_9);
        label_15->setObjectName(QString::fromUtf8("label_15"));
        sizePolicy3.setHeightForWidth(label_15->sizePolicy().hasHeightForWidth());
        label_15->setSizePolicy(sizePolicy3);
        label_15->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_27->addWidget(label_15);


        gridLayout_9->addLayout(horizontalLayout_27, 4, 0, 1, 1);

        spinEditOffset = new QSpinBox(groupBox_9);
        spinEditOffset->setObjectName(QString::fromUtf8("spinEditOffset"));
        spinEditOffset->setMinimumSize(QSize(0, 0));
        spinEditOffset->setMaximum(1080);
        spinEditOffset->setValue(24);

        gridLayout_9->addWidget(spinEditOffset, 4, 1, 1, 1);


        horizontalLayout_28->addLayout(gridLayout_9);

        horizontalSpacer_14 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_28->addItem(horizontalSpacer_14);


        horizontalLayout_3->addWidget(groupBox_9);

        groupBox_10 = new QGroupBox(tab_4);
        groupBox_10->setObjectName(QString::fromUtf8("groupBox_10"));
        sizePolicy3.setHeightForWidth(groupBox_10->sizePolicy().hasHeightForWidth());
        groupBox_10->setSizePolicy(sizePolicy3);
        horizontalLayout_31 = new QHBoxLayout(groupBox_10);
        horizontalLayout_31->setSpacing(6);
        horizontalLayout_31->setContentsMargins(5, 5, 5, 5);
        horizontalLayout_31->setObjectName(QString::fromUtf8("horizontalLayout_31"));
        gridLayout_8 = new QGridLayout();
        gridLayout_8->setSpacing(6);
        gridLayout_8->setObjectName(QString::fromUtf8("gridLayout_8"));
        rbhLeft = new QRadioButton(groupBox_10);
        rbhLeft->setObjectName(QString::fromUtf8("rbhLeft"));
        rbhLeft->setEnabled(false);

        gridLayout_8->addWidget(rbhLeft, 0, 0, 1, 1);

        horizontalLayout_29 = new QHBoxLayout();
        horizontalLayout_29->setSpacing(6);
        horizontalLayout_29->setObjectName(QString::fromUtf8("horizontalLayout_29"));
        horizontalSpacer_12 = new QSpacerItem(15, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_29->addItem(horizontalSpacer_12);

        label_17 = new QLabel(groupBox_10);
        label_17->setObjectName(QString::fromUtf8("label_17"));
        label_17->setEnabled(false);
        sizePolicy3.setHeightForWidth(label_17->sizePolicy().hasHeightForWidth());
        label_17->setSizePolicy(sizePolicy3);
        label_17->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_29->addWidget(label_17);


        gridLayout_8->addLayout(horizontalLayout_29, 1, 0, 1, 1);

        spinBox_2 = new QSpinBox(groupBox_10);
        spinBox_2->setObjectName(QString::fromUtf8("spinBox_2"));
        spinBox_2->setEnabled(false);
        sizePolicy6.setHeightForWidth(spinBox_2->sizePolicy().hasHeightForWidth());
        spinBox_2->setSizePolicy(sizePolicy6);
        spinBox_2->setMaximum(1080);
        spinBox_2->setValue(24);

        gridLayout_8->addWidget(spinBox_2, 1, 1, 1, 1);

        rbhCenter = new QRadioButton(groupBox_10);
        rbhCenter->setObjectName(QString::fromUtf8("rbhCenter"));
        rbhCenter->setEnabled(false);
        rbhCenter->setChecked(true);

        gridLayout_8->addWidget(rbhCenter, 2, 0, 1, 1);

        rbhRight = new QRadioButton(groupBox_10);
        rbhRight->setObjectName(QString::fromUtf8("rbhRight"));
        rbhRight->setEnabled(false);
        rbhRight->setChecked(false);

        gridLayout_8->addWidget(rbhRight, 3, 0, 1, 1);

        horizontalLayout_30 = new QHBoxLayout();
        horizontalLayout_30->setSpacing(6);
        horizontalLayout_30->setObjectName(QString::fromUtf8("horizontalLayout_30"));
        horizontalSpacer_15 = new QSpacerItem(15, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_30->addItem(horizontalSpacer_15);

        label_16 = new QLabel(groupBox_10);
        label_16->setObjectName(QString::fromUtf8("label_16"));
        label_16->setEnabled(false);
        sizePolicy3.setHeightForWidth(label_16->sizePolicy().hasHeightForWidth());
        label_16->setSizePolicy(sizePolicy3);
        label_16->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_30->addWidget(label_16);


        gridLayout_8->addLayout(horizontalLayout_30, 4, 0, 1, 1);

        SpinEditOffset_2 = new QSpinBox(groupBox_10);
        SpinEditOffset_2->setObjectName(QString::fromUtf8("SpinEditOffset_2"));
        SpinEditOffset_2->setEnabled(false);
        sizePolicy6.setHeightForWidth(SpinEditOffset_2->sizePolicy().hasHeightForWidth());
        SpinEditOffset_2->setSizePolicy(sizePolicy6);
        SpinEditOffset_2->setMaximum(1080);
        SpinEditOffset_2->setValue(24);

        gridLayout_8->addWidget(SpinEditOffset_2, 4, 1, 1, 1);


        horizontalLayout_31->addLayout(gridLayout_8);

        horizontalSpacer_21 = new QSpacerItem(117, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_31->addItem(horizontalSpacer_21);


        horizontalLayout_3->addWidget(groupBox_10);


        verticalLayout_9->addLayout(horizontalLayout_3);

        verticalSpacer_7 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_9->addItem(verticalSpacer_7);

        tabWidget->addTab(tab_4, QString());
        About = new QWidget();
        About->setObjectName(QString::fromUtf8("About"));
        verticalLayout_12 = new QVBoxLayout(About);
        verticalLayout_12->setSpacing(6);
        verticalLayout_12->setContentsMargins(5, 5, 5, 5);
        verticalLayout_12->setObjectName(QString::fromUtf8("verticalLayout_12"));
        textEdit = new QTextEdit(About);
        textEdit->setObjectName(QString::fromUtf8("textEdit"));

        verticalLayout_12->addWidget(textEdit);

        tabWidget->addTab(About, QString());
        splitter->addWidget(tabWidget);
        groupBox = new QGroupBox(splitter);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        QSizePolicy sizePolicy9(QSizePolicy::Preferred, QSizePolicy::Maximum);
        sizePolicy9.setHorizontalStretch(0);
        sizePolicy9.setVerticalStretch(0);
        sizePolicy9.setHeightForWidth(groupBox->sizePolicy().hasHeightForWidth());
        groupBox->setSizePolicy(sizePolicy9);
        groupBox->setFlat(true);
        verticalLayout_13 = new QVBoxLayout(groupBox);
        verticalLayout_13->setSpacing(4);
        verticalLayout_13->setContentsMargins(0, 0, 0, 0);
        verticalLayout_13->setObjectName(QString::fromUtf8("verticalLayout_13"));
        groupBox_4 = new QGroupBox(groupBox);
        groupBox_4->setObjectName(QString::fromUtf8("groupBox_4"));
        sizePolicy7.setHeightForWidth(groupBox_4->sizePolicy().hasHeightForWidth());
        groupBox_4->setSizePolicy(sizePolicy7);
        verticalLayout_2 = new QVBoxLayout(groupBox_4);
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setContentsMargins(5, 5, 5, 5);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setSpacing(6);
        horizontalLayout_7->setObjectName(QString::fromUtf8("horizontalLayout_7"));
        radioButtonTS = new QRadioButton(groupBox_4);
        radioButtonTS->setObjectName(QString::fromUtf8("radioButtonTS"));
        sizePolicy6.setHeightForWidth(radioButtonTS->sizePolicy().hasHeightForWidth());
        radioButtonTS->setSizePolicy(sizePolicy6);
        radioButtonTS->setChecked(true);

        horizontalLayout_7->addWidget(radioButtonTS);

        radioButtonM2TS = new QRadioButton(groupBox_4);
        radioButtonM2TS->setObjectName(QString::fromUtf8("radioButtonM2TS"));
        sizePolicy6.setHeightForWidth(radioButtonM2TS->sizePolicy().hasHeightForWidth());
        radioButtonM2TS->setSizePolicy(sizePolicy6);

        horizontalLayout_7->addWidget(radioButtonM2TS);

        radioButtonBluRayISO = new QRadioButton(groupBox_4);
        radioButtonBluRayISO->setObjectName(QString::fromUtf8("radioButtonBluRayISO"));

        horizontalLayout_7->addWidget(radioButtonBluRayISO);

        radioButtonBluRay = new QRadioButton(groupBox_4);
        radioButtonBluRay->setObjectName(QString::fromUtf8("radioButtonBluRay"));
        sizePolicy6.setHeightForWidth(radioButtonBluRay->sizePolicy().hasHeightForWidth());
        radioButtonBluRay->setSizePolicy(sizePolicy6);

        horizontalLayout_7->addWidget(radioButtonBluRay);

        radioButtonAVCHD = new QRadioButton(groupBox_4);
        radioButtonAVCHD->setObjectName(QString::fromUtf8("radioButtonAVCHD"));

        horizontalLayout_7->addWidget(radioButtonAVCHD);

        radioButtonDemux = new QRadioButton(groupBox_4);
        radioButtonDemux->setObjectName(QString::fromUtf8("radioButtonDemux"));
        sizePolicy6.setHeightForWidth(radioButtonDemux->sizePolicy().hasHeightForWidth());
        radioButtonDemux->setSizePolicy(sizePolicy6);

        horizontalLayout_7->addWidget(radioButtonDemux);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_7->addItem(horizontalSpacer_4);


        verticalLayout_2->addLayout(horizontalLayout_7);

        gridLayout_11 = new QGridLayout();
        gridLayout_11->setSpacing(6);
        gridLayout_11->setObjectName(QString::fromUtf8("gridLayout_11"));
        DiskLabel = new QLabel(groupBox_4);
        DiskLabel->setObjectName(QString::fromUtf8("DiskLabel"));

        gridLayout_11->addWidget(DiskLabel, 0, 0, 1, 1);

        DiskLabelEdit = new QLineEdit(groupBox_4);
        DiskLabelEdit->setObjectName(QString::fromUtf8("DiskLabelEdit"));

        gridLayout_11->addWidget(DiskLabelEdit, 0, 1, 1, 1);

        FilenameLabel = new QLabel(groupBox_4);
        FilenameLabel->setObjectName(QString::fromUtf8("FilenameLabel"));
        FilenameLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout_11->addWidget(FilenameLabel, 1, 0, 1, 1);

        outFileName = new QLineEdit(groupBox_4);
        outFileName->setObjectName(QString::fromUtf8("outFileName"));
        outFileName->setMinimumSize(QSize(0, 23));

        gridLayout_11->addWidget(outFileName, 1, 1, 1, 1);

        btnBrowse = new QPushButton(groupBox_4);
        btnBrowse->setObjectName(QString::fromUtf8("btnBrowse"));
        btnBrowse->setMinimumSize(QSize(75, 23));

        gridLayout_11->addWidget(btnBrowse, 1, 2, 1, 1);


        verticalLayout_2->addLayout(gridLayout_11);


        verticalLayout_13->addWidget(groupBox_4);

        groupBox_2 = new QGroupBox(groupBox);
        groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));
        groupBox_2->setMinimumSize(QSize(0, 100));
        groupBox_2->setMaximumSize(QSize(16777215, 1000000));
        verticalLayout_6 = new QVBoxLayout(groupBox_2);
        verticalLayout_6->setSpacing(6);
        verticalLayout_6->setContentsMargins(5, 5, 5, 5);
        verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
        memoMeta = new QTextEdit(groupBox_2);
        memoMeta->setObjectName(QString::fromUtf8("memoMeta"));
        memoMeta->setUndoRedoEnabled(false);
        memoMeta->setReadOnly(true);
        memoMeta->setAcceptRichText(false);
        memoMeta->setTextInteractionFlags(Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        verticalLayout_6->addWidget(memoMeta);


        verticalLayout_13->addWidget(groupBox_2);

        splitter->addWidget(groupBox);

        verticalLayout_10->addWidget(splitter);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setSpacing(6);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(80, -1, 0, -1);
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer);

        buttonMux = new QPushButton(TsMuxerWindow);
        buttonMux->setObjectName(QString::fromUtf8("buttonMux"));
        buttonMux->setMinimumSize(QSize(128, 0));
        buttonMux->setMaximumSize(QSize(128, 16777215));

        horizontalLayout_4->addWidget(buttonMux);

        buttonSaveMeta = new QPushButton(TsMuxerWindow);
        buttonSaveMeta->setObjectName(QString::fromUtf8("buttonSaveMeta"));
        buttonSaveMeta->setMinimumSize(QSize(128, 0));
        buttonSaveMeta->setMaximumSize(QSize(128, 16777215));

        horizontalLayout_4->addWidget(buttonSaveMeta);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer_2);

        label_Donate = new QLabel(TsMuxerWindow);
        label_Donate->setObjectName(QString::fromUtf8("label_Donate"));
        label_Donate->setEnabled(true);
        label_Donate->setMaximumSize(QSize(80, 32));
        label_Donate->setCursor(QCursor(Qt::PointingHandCursor));
        label_Donate->setText(QString::fromUtf8(""));
        label_Donate->setTextFormat(Qt::RichText);
        label_Donate->setPixmap(QPixmap(QString::fromUtf8(":/images/btn_donate.png")));
        label_Donate->setScaledContents(true);
        label_Donate->setAlignment(Qt::AlignCenter);
        label_Donate->setOpenExternalLinks(false);
        label_Donate->setTextInteractionFlags(Qt::NoTextInteraction);

        horizontalLayout_4->addWidget(label_Donate);


        verticalLayout_10->addLayout(horizontalLayout_4);

#if QT_CONFIG(shortcut)
#endif // QT_CONFIG(shortcut)
        QWidget::setTabOrder(tabWidget, inputFilesLV);
        QWidget::setTabOrder(inputFilesLV, addBtn);
        QWidget::setTabOrder(addBtn, trackLV);
        QWidget::setTabOrder(trackLV, moveupBtn);
        QWidget::setTabOrder(moveupBtn, movedownBtn);
        QWidget::setTabOrder(movedownBtn, removeTrackBtn);
        QWidget::setTabOrder(removeTrackBtn, tabWidgetTracks);
        QWidget::setTabOrder(tabWidgetTracks, checkFPS);
        QWidget::setTabOrder(checkFPS, checkBoxLevel);
        QWidget::setTabOrder(checkBoxLevel, comboBoxFPS);
        QWidget::setTabOrder(comboBoxFPS, comboBoxLevel);
        QWidget::setTabOrder(comboBoxLevel, checkBoxSPS);
        QWidget::setTabOrder(checkBoxSPS, checkBoxRemovePulldown);
        QWidget::setTabOrder(checkBoxRemovePulldown, comboBoxAR);
        QWidget::setTabOrder(comboBoxAR, editDelay);
        QWidget::setTabOrder(editDelay, langComboBox);
        QWidget::setTabOrder(langComboBox, dtsDwnConvert);
        QWidget::setTabOrder(dtsDwnConvert, checkBoxKeepFps);
        QWidget::setTabOrder(checkBoxKeepFps, radioButtonTS);
        QWidget::setTabOrder(radioButtonTS, radioButtonM2TS);
        QWidget::setTabOrder(radioButtonM2TS, radioButtonBluRay);
        QWidget::setTabOrder(radioButtonBluRay, radioButtonDemux);
        QWidget::setTabOrder(radioButtonDemux, outFileName);
        QWidget::setTabOrder(outFileName, btnBrowse);
        QWidget::setTabOrder(btnBrowse, memoMeta);
        QWidget::setTabOrder(memoMeta, checkBoxVBR);
        QWidget::setTabOrder(checkBoxVBR, editMaxBitrate);
        QWidget::setTabOrder(editMaxBitrate, editMinBitrate);
        QWidget::setTabOrder(editMinBitrate, checkBoxCBR);
        QWidget::setTabOrder(checkBoxCBR, editCBRBitrate);
        QWidget::setTabOrder(editCBRBitrate, editVBVLen);
        QWidget::setTabOrder(editVBVLen, checkBoxCrop);
        QWidget::setTabOrder(checkBoxCrop, radioButtonNoChapters);
        QWidget::setTabOrder(radioButtonNoChapters, radioButtonAutoChapter);
        QWidget::setTabOrder(radioButtonAutoChapter, spinEditChapterLen);
        QWidget::setTabOrder(spinEditChapterLen, radioButtonCustomChapters);
        QWidget::setTabOrder(radioButtonCustomChapters, memoChapters);
        QWidget::setTabOrder(memoChapters, noSplit);
        QWidget::setTabOrder(noSplit, splitByDuration);
        QWidget::setTabOrder(splitByDuration, spinEditSplitDuration);
        QWidget::setTabOrder(spinEditSplitDuration, splitBySize);
        QWidget::setTabOrder(splitBySize, editSplitSize);
        QWidget::setTabOrder(editSplitSize, comboBoxMeasure);
        QWidget::setTabOrder(comboBoxMeasure, checkBoxCut);
        QWidget::setTabOrder(checkBoxCut, listViewFont);
        QWidget::setTabOrder(listViewFont, fontButton);
        QWidget::setTabOrder(fontButton, colorButton);
        QWidget::setTabOrder(colorButton, spinEditBorder);
        QWidget::setTabOrder(spinEditBorder, radioButton);
        QWidget::setTabOrder(radioButton, spinBox);
        QWidget::setTabOrder(spinBox, radioButton_2);
        QWidget::setTabOrder(radioButton_2, radioButton_3);
        QWidget::setTabOrder(radioButton_3, spinEditOffset);
        QWidget::setTabOrder(spinEditOffset, rbhLeft);
        QWidget::setTabOrder(rbhLeft, spinBox_2);
        QWidget::setTabOrder(spinBox_2, rbhCenter);
        QWidget::setTabOrder(rbhCenter, rbhRight);
        QWidget::setTabOrder(rbhRight, SpinEditOffset_2);
        QWidget::setTabOrder(SpinEditOffset_2, textEdit);
        QWidget::setTabOrder(textEdit, buttonMux);
        QWidget::setTabOrder(buttonMux, buttonSaveMeta);
        QWidget::setTabOrder(buttonSaveMeta, radioButton_6);
        QWidget::setTabOrder(radioButton_6, radioButton_4);
        QWidget::setTabOrder(radioButton_4, radioButton_5);

        retranslateUi(TsMuxerWindow);

        tabWidget->setCurrentIndex(0);
        addBtn->setDefault(false);
        tabWidgetTracks->setCurrentIndex(0);
        comboBoxMeasure->setCurrentIndex(4);


        QMetaObject::connectSlotsByName(TsMuxerWindow);
    } // setupUi

    void retranslateUi(QWidget *TsMuxerWindow)
    {
        TsMuxerWindow->setWindowTitle(QCoreApplication::translate("TsMuxerWindow", "tsMuxeR GUI 2.6.13", nullptr));
        label_3->setText(QCoreApplication::translate("TsMuxerWindow", "Input files:", nullptr));
        addBtn->setText(QCoreApplication::translate("TsMuxerWindow", "add", nullptr));
        btnAppend->setText(QCoreApplication::translate("TsMuxerWindow", "join", nullptr));
        removeFile->setText(QCoreApplication::translate("TsMuxerWindow", "remove", nullptr));
        label_2->setText(QCoreApplication::translate("TsMuxerWindow", "Tracks:", nullptr));
        QTableWidgetItem *___qtablewidgetitem = trackLV->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("TsMuxerWindow", "#", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = trackLV->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("TsMuxerWindow", "source file", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = trackLV->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("TsMuxerWindow", "codec", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = trackLV->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("TsMuxerWindow", "lang", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = trackLV->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("TsMuxerWindow", "track info", nullptr));
        moveupBtn->setText(QCoreApplication::translate("TsMuxerWindow", "up", nullptr));
        movedownBtn->setText(QCoreApplication::translate("TsMuxerWindow", "down", nullptr));
        removeTrackBtn->setText(QCoreApplication::translate("TsMuxerWindow", "remove", nullptr));
        imageVideo->setText(QString());
        checkFPS->setText(QCoreApplication::translate("TsMuxerWindow", "Change fps:", nullptr));
        comboBoxFPS->setItemText(0, QCoreApplication::translate("TsMuxerWindow", "24", nullptr));
        comboBoxFPS->setItemText(1, QCoreApplication::translate("TsMuxerWindow", "25", nullptr));
        comboBoxFPS->setItemText(2, QCoreApplication::translate("TsMuxerWindow", "30", nullptr));
        comboBoxFPS->setItemText(3, QCoreApplication::translate("TsMuxerWindow", "24000/1001", nullptr));
        comboBoxFPS->setItemText(4, QCoreApplication::translate("TsMuxerWindow", "30000/1001", nullptr));

        checkBoxRemovePulldown->setText(QCoreApplication::translate("TsMuxerWindow", "Remove pulldown", nullptr));
        checkBoxSecondaryVideo->setText(QCoreApplication::translate("TsMuxerWindow", "Secondary (PIP)", nullptr));
        comboBoxLevel->setItemText(0, QCoreApplication::translate("TsMuxerWindow", "1.0", nullptr));
        comboBoxLevel->setItemText(1, QCoreApplication::translate("TsMuxerWindow", "1.1", nullptr));
        comboBoxLevel->setItemText(2, QCoreApplication::translate("TsMuxerWindow", "1.2", nullptr));
        comboBoxLevel->setItemText(3, QCoreApplication::translate("TsMuxerWindow", "1.3", nullptr));
        comboBoxLevel->setItemText(4, QCoreApplication::translate("TsMuxerWindow", "2.0", nullptr));
        comboBoxLevel->setItemText(5, QCoreApplication::translate("TsMuxerWindow", "2.1", nullptr));
        comboBoxLevel->setItemText(6, QCoreApplication::translate("TsMuxerWindow", "2.2", nullptr));
        comboBoxLevel->setItemText(7, QCoreApplication::translate("TsMuxerWindow", "3.0", nullptr));
        comboBoxLevel->setItemText(8, QCoreApplication::translate("TsMuxerWindow", "3.1", nullptr));
        comboBoxLevel->setItemText(9, QCoreApplication::translate("TsMuxerWindow", "3.2", nullptr));
        comboBoxLevel->setItemText(10, QCoreApplication::translate("TsMuxerWindow", "4.0", nullptr));
        comboBoxLevel->setItemText(11, QCoreApplication::translate("TsMuxerWindow", "4.1", nullptr));
        comboBoxLevel->setItemText(12, QCoreApplication::translate("TsMuxerWindow", "4.2", nullptr));
        comboBoxLevel->setItemText(13, QCoreApplication::translate("TsMuxerWindow", "5.0", nullptr));
        comboBoxLevel->setItemText(14, QCoreApplication::translate("TsMuxerWindow", "5.1", nullptr));

        labelAR->setText(QCoreApplication::translate("TsMuxerWindow", "AR", nullptr));
        comboBoxAR->setItemText(0, QCoreApplication::translate("TsMuxerWindow", "As source", nullptr));
        comboBoxAR->setItemText(1, QCoreApplication::translate("TsMuxerWindow", "1:1 (Square)", nullptr));
        comboBoxAR->setItemText(2, QCoreApplication::translate("TsMuxerWindow", "4:3", nullptr));
        comboBoxAR->setItemText(3, QCoreApplication::translate("TsMuxerWindow", "16:9", nullptr));
        comboBoxAR->setItemText(4, QCoreApplication::translate("TsMuxerWindow", "2.21:1", nullptr));

        checkBoxLevel->setText(QCoreApplication::translate("TsMuxerWindow", "Change level:", nullptr));
        checkBoxSPS->setText(QCoreApplication::translate("TsMuxerWindow", "Continually insert SPS/PPS", nullptr));
        comboBoxSEI->setItemText(0, QCoreApplication::translate("TsMuxerWindow", "Do not change SEI and VUI data", nullptr));
        comboBoxSEI->setItemText(1, QCoreApplication::translate("TsMuxerWindow", "Insert SEI and VUI data if absent", nullptr));
        comboBoxSEI->setItemText(2, QCoreApplication::translate("TsMuxerWindow", "Always rebuild  SEI and VUI data", nullptr));

        tabWidgetTracks->setTabText(tabWidgetTracks->indexOf(tabSheetVideo), QCoreApplication::translate("TsMuxerWindow", "General track options", nullptr));
        imageSubtitles->setText(QString());
        imageAudio->setText(QString());
        label_18->setText(QCoreApplication::translate("TsMuxerWindow", "Delay (in ms):", nullptr));
        dtsDwnConvert->setText(QCoreApplication::translate("TsMuxerWindow", "Downconvert HD audio", nullptr));
        secondaryCheckBox->setText(QCoreApplication::translate("TsMuxerWindow", "Seconday", nullptr));
        checkBoxKeepFps->setText(QCoreApplication::translate("TsMuxerWindow", "Bind to video FPS", nullptr));
        offsetsLabel->setText(QCoreApplication::translate("TsMuxerWindow", "3d offset:", nullptr));
        label_19->setText(QCoreApplication::translate("TsMuxerWindow", "Language:", nullptr));
        tabWidgetTracks->setTabText(tabWidgetTracks->indexOf(tabSheetAudio), QCoreApplication::translate("TsMuxerWindow", "General track options", nullptr));
        radioButton_4->setText(QCoreApplication::translate("TsMuxerWindow", "Demux to multi channels WAVE file", nullptr));
        radioButton_5->setText(QCoreApplication::translate("TsMuxerWindow", "Demux to several mono WAVE files", nullptr));
        radioButton_6->setText(QCoreApplication::translate("TsMuxerWindow", "Demux to RAW PCM stream", nullptr));
        tabWidgetTracks->setTabText(tabWidgetTracks->indexOf(demuxLpcmOptions), QCoreApplication::translate("TsMuxerWindow", "Demux options", nullptr));
        tabWidgetTracks->setTabText(tabWidgetTracks->indexOf(tabSheetFake), QCoreApplication::translate("TsMuxerWindow", "General track options", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(GeneralTab), QCoreApplication::translate("TsMuxerWindow", "Input", nullptr));
        groupBox7->setTitle(QCoreApplication::translate("TsMuxerWindow", "Bitrate", nullptr));
        checkBoxVBR->setText(QCoreApplication::translate("TsMuxerWindow", "Mux VBR", nullptr));
        checkBoxRVBR->setText(QCoreApplication::translate("TsMuxerWindow", "Restricted VBR", nullptr));
        label_5->setText(QCoreApplication::translate("TsMuxerWindow", "Max bitrate, kbps", nullptr));
        label_6->setText(QCoreApplication::translate("TsMuxerWindow", "Min bitrate, kbps", nullptr));
        checkBoxCBR->setText(QCoreApplication::translate("TsMuxerWindow", "Mux CBR", nullptr));
        label_7->setText(QCoreApplication::translate("TsMuxerWindow", "Bitrate, kbps", nullptr));
        label_8->setText(QCoreApplication::translate("TsMuxerWindow", "VBV Buffer size, ms", nullptr));
        groupBox_5->setTitle(QCoreApplication::translate("TsMuxerWindow", "General", nullptr));
        checkBoxSound->setText(QCoreApplication::translate("TsMuxerWindow", "Play sound at end", nullptr));
        checkBoxNewAudioPes->setText(QCoreApplication::translate("TsMuxerWindow", "Generate HDMV compatible TS", nullptr));
        checkBoxCrop->setText(QCoreApplication::translate("TsMuxerWindow", "Restore cropped video to full size", nullptr));
        GroupBox114->setTitle(QCoreApplication::translate("TsMuxerWindow", "Default output folder", nullptr));
        radioButtonStoreOutput->setText(QCoreApplication::translate("TsMuxerWindow", "Use the latest output folder name", nullptr));
        radioButtonOutoutInInput->setText(QCoreApplication::translate("TsMuxerWindow", "Place output folder to a input folder", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_3), QCoreApplication::translate("TsMuxerWindow", "General", nullptr));
        groupBox_3->setTitle(QCoreApplication::translate("TsMuxerWindow", "Chapters", nullptr));
        radioButtonNoChapters->setText(QCoreApplication::translate("TsMuxerWindow", "No chapters", nullptr));
        radioButtonAutoChapter->setText(QCoreApplication::translate("TsMuxerWindow", "Insert chapter every", nullptr));
        label_4->setText(QCoreApplication::translate("TsMuxerWindow", "minutes", nullptr));
        radioButtonCustomChapters->setText(QCoreApplication::translate("TsMuxerWindow", "Custom chapters list", nullptr));
        label_9->setText(QCoreApplication::translate("TsMuxerWindow", "Chapters:", nullptr));
        memoChapters->setHtml(QCoreApplication::translate("TsMuxerWindow", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">\n"
"<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-size:8pt;\"></p></body></html>", nullptr));
        BDOptionsGroupBox->setTitle(QCoreApplication::translate("TsMuxerWindow", "Options", nullptr));
        checkBoxBlankPL->setText(QCoreApplication::translate("TsMuxerWindow", "Add blank playlist for cropped video", nullptr));
        BlackplaylistLabel->setText(QCoreApplication::translate("TsMuxerWindow", "Blank playlist", nullptr));
        label->setText(QCoreApplication::translate("TsMuxerWindow", "First MPLS file", nullptr));
        label_10->setText(QCoreApplication::translate("TsMuxerWindow", "First M2TS file", nullptr));
        muxTimeEdit->setDisplayFormat(QCoreApplication::translate("TsMuxerWindow", "H:mm:ss.zzz", nullptr));
        label_20->setText(QCoreApplication::translate("TsMuxerWindow", "Start mux time", nullptr));
        muxTimeClock->setSuffix(QString());
        muxTimeClock->setPrefix(QString());
        label_23->setText(QCoreApplication::translate("TsMuxerWindow", "45 Khz clock:", nullptr));
        groupBox_11->setTitle(QCoreApplication::translate("TsMuxerWindow", "3D settings", nullptr));
        rightEyeCheckBox->setText(QCoreApplication::translate("TsMuxerWindow", "Use base video stream for right eye", nullptr));
        groupBoxPip->setTitle(QCoreApplication::translate("TsMuxerWindow", "PIP settings", nullptr));
        label_24->setText(QCoreApplication::translate("TsMuxerWindow", "Corner", nullptr));
        comboBoxPipCorner->setItemText(0, QCoreApplication::translate("TsMuxerWindow", "Top Left", nullptr));
        comboBoxPipCorner->setItemText(1, QCoreApplication::translate("TsMuxerWindow", "Top Right", nullptr));
        comboBoxPipCorner->setItemText(2, QCoreApplication::translate("TsMuxerWindow", "Bottom Right", nullptr));
        comboBoxPipCorner->setItemText(3, QCoreApplication::translate("TsMuxerWindow", "Bottom Left", nullptr));

        label_25->setText(QCoreApplication::translate("TsMuxerWindow", "Horizontal offset", nullptr));
        label_26->setText(QCoreApplication::translate("TsMuxerWindow", "Size", nullptr));
        comboBoxPipSize->setItemText(0, QCoreApplication::translate("TsMuxerWindow", "Not scale", nullptr));
        comboBoxPipSize->setItemText(1, QCoreApplication::translate("TsMuxerWindow", "Half (x 1/2)", nullptr));
        comboBoxPipSize->setItemText(2, QCoreApplication::translate("TsMuxerWindow", "Quoter (x 1/4)", nullptr));
        comboBoxPipSize->setItemText(3, QCoreApplication::translate("TsMuxerWindow", "One and Half (x 1.5)", nullptr));
        comboBoxPipSize->setItemText(4, QCoreApplication::translate("TsMuxerWindow", "Full Screen", nullptr));

        label_27->setText(QCoreApplication::translate("TsMuxerWindow", "Vertical offset", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_2), QCoreApplication::translate("TsMuxerWindow", "Blu-ray", nullptr));
        groupBox_6->setTitle(QCoreApplication::translate("TsMuxerWindow", "Splitting", nullptr));
        noSplit->setText(QCoreApplication::translate("TsMuxerWindow", "No split", nullptr));
        splitByDuration->setText(QCoreApplication::translate("TsMuxerWindow", "Split by duration every", nullptr));
        labelSplitByDur->setText(QCoreApplication::translate("TsMuxerWindow", "sec", nullptr));
        splitBySize->setText(QCoreApplication::translate("TsMuxerWindow", "Split by size every", nullptr));
        comboBoxMeasure->setItemText(0, QCoreApplication::translate("TsMuxerWindow", "KB", nullptr));
        comboBoxMeasure->setItemText(1, QCoreApplication::translate("TsMuxerWindow", "KiB", nullptr));
        comboBoxMeasure->setItemText(2, QCoreApplication::translate("TsMuxerWindow", "MB", nullptr));
        comboBoxMeasure->setItemText(3, QCoreApplication::translate("TsMuxerWindow", "MiB", nullptr));
        comboBoxMeasure->setItemText(4, QCoreApplication::translate("TsMuxerWindow", "GB", nullptr));
        comboBoxMeasure->setItemText(5, QCoreApplication::translate("TsMuxerWindow", "GiB", nullptr));

        groupBox_7->setTitle(QCoreApplication::translate("TsMuxerWindow", "Cutting", nullptr));
        checkBoxCut->setText(QCoreApplication::translate("TsMuxerWindow", "Enable cutting", nullptr));
        label_11->setText(QCoreApplication::translate("TsMuxerWindow", "Start", nullptr));
        label_12->setText(QCoreApplication::translate("TsMuxerWindow", "End", nullptr));
        cutStartTimeEdit->setDisplayFormat(QCoreApplication::translate("TsMuxerWindow", "H:mm:ss.zzz", nullptr));
        cutEndTimeEdit->setDisplayFormat(QCoreApplication::translate("TsMuxerWindow", "H:mm:ss.zzz", nullptr));
        label_28->setText(QCoreApplication::translate("TsMuxerWindow", "(h:mm:ss.ms)", nullptr));
        label_29->setText(QCoreApplication::translate("TsMuxerWindow", "(h:mm:ss.ms)", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab), QCoreApplication::translate("TsMuxerWindow", "Split && cut", nullptr));
        groupBox_8->setTitle(QCoreApplication::translate("TsMuxerWindow", " Default text based subtitles font: ", nullptr));
        QTableWidgetItem *___qtablewidgetitem5 = listViewFont->horizontalHeaderItem(0);
        ___qtablewidgetitem5->setText(QCoreApplication::translate("TsMuxerWindow", "New Column", nullptr));
        QTableWidgetItem *___qtablewidgetitem6 = listViewFont->horizontalHeaderItem(1);
        ___qtablewidgetitem6->setText(QCoreApplication::translate("TsMuxerWindow", "New Column", nullptr));
        QTableWidgetItem *___qtablewidgetitem7 = listViewFont->verticalHeaderItem(0);
        ___qtablewidgetitem7->setText(QCoreApplication::translate("TsMuxerWindow", "New Row", nullptr));
        QTableWidgetItem *___qtablewidgetitem8 = listViewFont->verticalHeaderItem(1);
        ___qtablewidgetitem8->setText(QCoreApplication::translate("TsMuxerWindow", "New Row", nullptr));
        QTableWidgetItem *___qtablewidgetitem9 = listViewFont->verticalHeaderItem(2);
        ___qtablewidgetitem9->setText(QCoreApplication::translate("TsMuxerWindow", "New Row", nullptr));
        QTableWidgetItem *___qtablewidgetitem10 = listViewFont->verticalHeaderItem(3);
        ___qtablewidgetitem10->setText(QCoreApplication::translate("TsMuxerWindow", "New Row", nullptr));
        QTableWidgetItem *___qtablewidgetitem11 = listViewFont->verticalHeaderItem(4);
        ___qtablewidgetitem11->setText(QCoreApplication::translate("TsMuxerWindow", "New Row", nullptr));

        const bool __sortingEnabled = listViewFont->isSortingEnabled();
        listViewFont->setSortingEnabled(false);
        QTableWidgetItem *___qtablewidgetitem12 = listViewFont->item(0, 0);
        ___qtablewidgetitem12->setText(QCoreApplication::translate("TsMuxerWindow", "Name:", nullptr));
        QTableWidgetItem *___qtablewidgetitem13 = listViewFont->item(0, 1);
        ___qtablewidgetitem13->setText(QCoreApplication::translate("TsMuxerWindow", "Arial", nullptr));
        QTableWidgetItem *___qtablewidgetitem14 = listViewFont->item(1, 0);
        ___qtablewidgetitem14->setText(QCoreApplication::translate("TsMuxerWindow", "Size:", nullptr));
        QTableWidgetItem *___qtablewidgetitem15 = listViewFont->item(1, 1);
        ___qtablewidgetitem15->setText(QCoreApplication::translate("TsMuxerWindow", "65", nullptr));
        QTableWidgetItem *___qtablewidgetitem16 = listViewFont->item(2, 0);
        ___qtablewidgetitem16->setText(QCoreApplication::translate("TsMuxerWindow", "Color:", nullptr));
        QTableWidgetItem *___qtablewidgetitem17 = listViewFont->item(2, 1);
        ___qtablewidgetitem17->setText(QCoreApplication::translate("TsMuxerWindow", "0xffffffff", nullptr));
        QTableWidgetItem *___qtablewidgetitem18 = listViewFont->item(3, 0);
        ___qtablewidgetitem18->setText(QCoreApplication::translate("TsMuxerWindow", "Charset:", nullptr));
        QTableWidgetItem *___qtablewidgetitem19 = listViewFont->item(3, 1);
        ___qtablewidgetitem19->setText(QCoreApplication::translate("TsMuxerWindow", "Default", nullptr));
        QTableWidgetItem *___qtablewidgetitem20 = listViewFont->item(4, 0);
        ___qtablewidgetitem20->setText(QCoreApplication::translate("TsMuxerWindow", "Options:", nullptr));
        listViewFont->setSortingEnabled(__sortingEnabled);

        fontButton->setText(QCoreApplication::translate("TsMuxerWindow", "Font", nullptr));
        colorButton->setText(QCoreApplication::translate("TsMuxerWindow", "Color", nullptr));
        label_13->setText(QCoreApplication::translate("TsMuxerWindow", "Additional border, pixels:", nullptr));
        label_21->setText(QCoreApplication::translate("TsMuxerWindow", "line spacing:", nullptr));
        label_22->setText(QCoreApplication::translate("TsMuxerWindow", "Fade in/out animation:", nullptr));
        comboBoxAnimation->setItemText(0, QCoreApplication::translate("TsMuxerWindow", "None", nullptr));
        comboBoxAnimation->setItemText(1, QCoreApplication::translate("TsMuxerWindow", "Fast", nullptr));
        comboBoxAnimation->setItemText(2, QCoreApplication::translate("TsMuxerWindow", "Medium", nullptr));
        comboBoxAnimation->setItemText(3, QCoreApplication::translate("TsMuxerWindow", "Slow", nullptr));
        comboBoxAnimation->setItemText(4, QCoreApplication::translate("TsMuxerWindow", "Very slow", nullptr));

        groupBox_9->setTitle(QCoreApplication::translate("TsMuxerWindow", " Vertical position: ", nullptr));
        radioButton->setText(QCoreApplication::translate("TsMuxerWindow", "Top of screen", nullptr));
        label_14->setText(QCoreApplication::translate("TsMuxerWindow", "top offset, pixels:", nullptr));
        radioButton_2->setText(QCoreApplication::translate("TsMuxerWindow", "Screen center", nullptr));
        radioButton_3->setText(QCoreApplication::translate("TsMuxerWindow", "Bottom of screen", nullptr));
        label_15->setText(QCoreApplication::translate("TsMuxerWindow", "bottom offset, pixels:", nullptr));
        groupBox_10->setTitle(QCoreApplication::translate("TsMuxerWindow", " Horizontal position: ", nullptr));
        rbhLeft->setText(QCoreApplication::translate("TsMuxerWindow", "Left of screen", nullptr));
        label_17->setText(QCoreApplication::translate("TsMuxerWindow", "left offset, pixels:", nullptr));
        rbhCenter->setText(QCoreApplication::translate("TsMuxerWindow", "Screen center", nullptr));
        rbhRight->setText(QCoreApplication::translate("TsMuxerWindow", "Right of screen", nullptr));
        label_16->setText(QCoreApplication::translate("TsMuxerWindow", "right offset, pixels:", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_4), QCoreApplication::translate("TsMuxerWindow", "Subtitles", nullptr));
        textEdit->setHtml(QCoreApplication::translate("TsMuxerWindow", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><title>tsMuxeR</title><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><a href=\"https://github.com/justdan96/tsMuxer\"><span style=\" font-size:8pt; font-weight:600; text-decoration: underline; color:#0000ff;\">tsMuxeR</span></a><span style=\" font-size:8pt;\"> \342\200\223 the software utility to create TS and M2TS files for IP broadcasting as well as for viewing at hardware video players (i.e., Dune HD Ultra, Sony Playstation3, Samsung Smart TV and others). </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span"
                        " style=\" font-size:8pt;\">tsMuxeR is open source. </span></p>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt;\">Supported incoming formats: </span></p>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" font-size:8pt;\" style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">TS; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">M2TS; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">SIFF;</li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">MOV;</li>\n"
""
                        "<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">MP4;</li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">MKV;</li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Blu-ray; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Demux option. </li></ul>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt;\">Supported videocodecs: </span></p>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" font-size:8pt;\" style=\" margin-top:12px"
                        "; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">H.264/MVC </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Microsoft VC-1; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">MPEG-2. </li></ul>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt;\">Supported audiocodecs: </span></p>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" font-size:8pt;\" style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">AAC; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-"
                        "right:0px; -qt-block-indent:0; text-indent:0px;\">AC3 / E-AC3(DD+); </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Dolby True HD (for streams with AC3 core only); </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">DTS/ DTS-HD; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">LPCM. </li></ul>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt;\">Supported subtitle types: </span></p>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" font-size:8pt;\" style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; mar"
                        "gin-right:0px; -qt-block-indent:0; text-indent:0px;\">M2TS Presentation graphic stream. </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">SRT text subtitles </li></ul>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt;\">Supported containers and formats: </span></p>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" font-size:8pt;\" style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Elementary stream; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Transport stream TS and M2TS; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bot"
                        "tom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Program stream EVO/VOB/MPG; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Matroska MKV/MKA. </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">MP4/MOV. </li></ul>\n"
"<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:8pt;\">Main features: </span></p>\n"
"<p style=\"-qt-paragraph-type:empty; margin-top:12px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-size:8pt;\"></p>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px;"
                        " margin-right:0px; -qt-block-indent:0; text-indent:0px;\">3D blu-ray support;</li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Automatic or manual fps adjustment while mixing; </li></ul>\n"
"<ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Level changing as well as SEI, SPS/PPS elements and NAL unit delimiter cycle insertion while mixing H.264; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Audio tracks and subtitles time shifting; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Abili"
                        "ty to extract DTS core from DTS-HD; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to extract AC3 core from True-HD; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to join files; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to adjust fps for subtitles; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to convert LPCM streams into WAVE and vice versa; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Track language information injection in"
                        "to blu-ray structure and TS header; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to cut source files; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to split output file; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to detect audio delay for TS/M2TS/MPG/VOB/EVO sources; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to remove pulldown info from stream; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to open Blu-ray playlist"
                        " (MPLS) files; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Ability to convert SRT subtitles to PGS; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Tags for SRT subtitles support - tags for changing font, color, size, etc.; tag's syntax is similar to HTML; </li>\n"
"<li style=\" font-size:8pt;\" style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">United cross-platform GUI - Windows, Linux, MacOS. </li></ul></body></html>", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(About), QCoreApplication::translate("TsMuxerWindow", "About", nullptr));
        groupBox->setTitle(QString());
        groupBox_4->setTitle(QCoreApplication::translate("TsMuxerWindow", "Output", nullptr));
        radioButtonTS->setText(QCoreApplication::translate("TsMuxerWindow", "TS muxing", nullptr));
        radioButtonM2TS->setText(QCoreApplication::translate("TsMuxerWindow", "M2TS muxing", nullptr));
        radioButtonBluRayISO->setText(QCoreApplication::translate("TsMuxerWindow", "Blu-ray ISO", nullptr));
        radioButtonBluRay->setText(QCoreApplication::translate("TsMuxerWindow", "Blu-ray folder", nullptr));
        radioButtonAVCHD->setText(QCoreApplication::translate("TsMuxerWindow", "AVCHD folder", nullptr));
        radioButtonDemux->setText(QCoreApplication::translate("TsMuxerWindow", "Demux", nullptr));
        DiskLabel->setText(QCoreApplication::translate("TsMuxerWindow", "Disk label", nullptr));
        FilenameLabel->setText(QCoreApplication::translate("TsMuxerWindow", "File name", nullptr));
        btnBrowse->setText(QCoreApplication::translate("TsMuxerWindow", "Browse", nullptr));
        groupBox_2->setTitle(QCoreApplication::translate("TsMuxerWindow", "Meta file", nullptr));
        buttonMux->setText(QCoreApplication::translate("TsMuxerWindow", "Sta&rt muxing", nullptr));
        buttonSaveMeta->setText(QCoreApplication::translate("TsMuxerWindow", "Save meta file", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TsMuxerWindow: public Ui_TsMuxerWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TSMUXERWINDOW_H
