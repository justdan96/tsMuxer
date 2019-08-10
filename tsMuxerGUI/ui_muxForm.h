/********************************************************************************
** Form generated from reading UI file 'muxForm.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MUXFORM_H
#define UI_MUXFORM_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_muxForm
{
public:
    QVBoxLayout *verticalLayout_3;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_2;
    QLabel *muxLabel;
    QSpacerItem *horizontalSpacer;
    QLabel *progressLabel;
    QProgressBar *progressBar;
    QGroupBox *groupBox_2;
    QVBoxLayout *verticalLayout_5;
    QSplitter *splitter;
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout_2;
    QLabel *label_3;
    QTextEdit *stdoutText;
    QWidget *layoutWidget1;
    QVBoxLayout *verticalLayout_4;
    QLabel *label_4;
    QTextEdit *stderrText;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *okBtn;
    QPushButton *abortBtn;
    QSpacerItem *horizontalSpacer_3;

    void setupUi(QDialog *muxForm)
    {
        if (muxForm->objectName().isEmpty())
            muxForm->setObjectName(QString::fromUtf8("muxForm"));
        muxForm->resize(596, 477);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/images/icon.png"), QSize(), QIcon::Normal, QIcon::Off);
        muxForm->setWindowIcon(icon);
        verticalLayout_3 = new QVBoxLayout(muxForm);
        verticalLayout_3->setContentsMargins(6, 6, 6, 6);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        groupBox = new QGroupBox(muxForm);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        groupBox->setMinimumSize(QSize(0, 0));
        verticalLayout = new QVBoxLayout(groupBox);
        verticalLayout->setContentsMargins(6, 6, 6, 6);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        muxLabel = new QLabel(groupBox);
        muxLabel->setObjectName(QString::fromUtf8("muxLabel"));

        horizontalLayout_2->addWidget(muxLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        progressLabel = new QLabel(groupBox);
        progressLabel->setObjectName(QString::fromUtf8("progressLabel"));
        QFont font;
        font.setBold(true);
        font.setWeight(75);
        font.setKerning(true);
        progressLabel->setFont(font);
        progressLabel->setFrameShadow(QFrame::Plain);

        horizontalLayout_2->addWidget(progressLabel);


        verticalLayout->addLayout(horizontalLayout_2);

        progressBar = new QProgressBar(groupBox);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setMaximum(1000);
        progressBar->setValue(0);
        progressBar->setTextVisible(false);

        verticalLayout->addWidget(progressBar);


        verticalLayout_3->addWidget(groupBox);

        groupBox_2 = new QGroupBox(muxForm);
        groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));
        verticalLayout_5 = new QVBoxLayout(groupBox_2);
        verticalLayout_5->setContentsMargins(6, 6, 6, 6);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        splitter = new QSplitter(groupBox_2);
        splitter->setObjectName(QString::fromUtf8("splitter"));
        splitter->setOrientation(Qt::Vertical);
        layoutWidget = new QWidget(splitter);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        verticalLayout_2 = new QVBoxLayout(layoutWidget);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        label_3 = new QLabel(layoutWidget);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        verticalLayout_2->addWidget(label_3);

        stdoutText = new QTextEdit(layoutWidget);
        stdoutText->setObjectName(QString::fromUtf8("stdoutText"));
        stdoutText->setTabChangesFocus(true);
        stdoutText->setUndoRedoEnabled(false);
        stdoutText->setReadOnly(true);
        stdoutText->setAcceptRichText(false);
        stdoutText->setTextInteractionFlags(Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        verticalLayout_2->addWidget(stdoutText);

        splitter->addWidget(layoutWidget);
        layoutWidget1 = new QWidget(splitter);
        layoutWidget1->setObjectName(QString::fromUtf8("layoutWidget1"));
        verticalLayout_4 = new QVBoxLayout(layoutWidget1);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        verticalLayout_4->setContentsMargins(0, 0, 0, 0);
        label_4 = new QLabel(layoutWidget1);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        verticalLayout_4->addWidget(label_4);

        stderrText = new QTextEdit(layoutWidget1);
        stderrText->setObjectName(QString::fromUtf8("stderrText"));
        stderrText->setTabChangesFocus(true);
        stderrText->setUndoRedoEnabled(false);
        stderrText->setReadOnly(true);
        stderrText->setAcceptRichText(false);
        stderrText->setTextInteractionFlags(Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        verticalLayout_4->addWidget(stderrText);

        splitter->addWidget(layoutWidget1);

        verticalLayout_5->addWidget(splitter);


        verticalLayout_3->addWidget(groupBox_2);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        okBtn = new QPushButton(muxForm);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));
        okBtn->setMaximumSize(QSize(16777215, 25));

        horizontalLayout->addWidget(okBtn);

        abortBtn = new QPushButton(muxForm);
        abortBtn->setObjectName(QString::fromUtf8("abortBtn"));
        abortBtn->setMaximumSize(QSize(16777215, 25));

        horizontalLayout->addWidget(abortBtn);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_3);


        verticalLayout_3->addLayout(horizontalLayout);


        retranslateUi(muxForm);

        QMetaObject::connectSlotsByName(muxForm);
    } // setupUi

    void retranslateUi(QDialog *muxForm)
    {
        muxForm->setWindowTitle(QCoreApplication::translate("muxForm", "Muxing in progress", nullptr));
        groupBox->setTitle(QCoreApplication::translate("muxForm", "Status and progress", nullptr));
        muxLabel->setText(QCoreApplication::translate("muxForm", "Muxing in progress.", nullptr));
        progressLabel->setText(QCoreApplication::translate("muxForm", "Progress: 100.0%", nullptr));
        groupBox_2->setTitle(QCoreApplication::translate("muxForm", "Output", nullptr));
        label_3->setText(QCoreApplication::translate("muxForm", "tsMuxeR output:", nullptr));
        label_4->setText(QCoreApplication::translate("muxForm", "Errors:", nullptr));
        okBtn->setText(QCoreApplication::translate("muxForm", "OK", nullptr));
        abortBtn->setText(QCoreApplication::translate("muxForm", "Abort", nullptr));
    } // retranslateUi

};

namespace Ui {
    class muxForm: public Ui_muxForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MUXFORM_H
