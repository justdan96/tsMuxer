#include "muxForm.h"

#include <QCloseEvent>
#include <QProcess>

#include "ui_muxForm.h"

const static int MAX_ERRORS_CNT = 10000;

MuxForm::MuxForm(QWidget *parent) : QDialog(parent, Qt::WindowMaximizeButtonHint), ui(new Ui_muxForm), muxProcess(0)
{
    ui->setupUi(this);
    connect(ui->progressBar, &QProgressBar::valueChanged, this, &MuxForm::onProgressChanged);
    connect(ui->abortBtn, &QPushButton::clicked, this, &MuxForm::onAbort);
    connect(ui->okBtn, &QPushButton::clicked, this, &MuxForm::close);
}

void MuxForm::closeEvent(QCloseEvent *event)
{
    onAbort();
    event->accept();
}

void MuxForm::prepare(const QString &label)
{
    muxProcess = 0;
    errCnt = 0;
    setWindowTitle(label);
    ui->muxLabel->setText(label + '.');
    ui->progressBar->setValue(0);
    ui->stdoutText->clear();
    ui->stderrText->clear();
    ui->abortBtn->setEnabled(true);
    ui->okBtn->setEnabled(false);
}

void MuxForm::onProgressChanged()
{
    ui->progressLabel->setText(QObject::tr("Progress: ") + QString::number(ui->progressBar->value() / 10.0, 'f', 1) +
                               '%');
}

void MuxForm::setProgress(int value) { ui->progressBar->setValue(value); }

void MuxForm::addStdOutLine(const QString &line)
{
    ui->stdoutText->append(line);
    QTextCursor c = ui->stdoutText->textCursor();
    c.movePosition(QTextCursor::End);
    ui->stdoutText->setTextCursor(c);
}

void MuxForm::addStdErrLine(const QString &line)
{
    if (errCnt >= MAX_ERRORS_CNT)
        return;
    ui->stderrText->append(line);
    errCnt = ui->stderrText->document()->blockCount();
    if (errCnt >= MAX_ERRORS_CNT)
    {
        ui->stderrText->append("---------------------------------------");
        ui->stderrText->append(QObject::tr("Too many errors! tsMuxeR is terminated."));
        onAbort();
    }
    QTextCursor c = ui->stderrText->textCursor();
    c.movePosition(QTextCursor::End);
    ui->stderrText->setTextCursor(c);
}

void MuxForm::muxFinished(int exitCode, const QString &)
{
    if (muxProcess && ui->abortBtn->isEnabled())
    {
        if (exitCode == 0)
            setWindowTitle(QObject::tr("tsMuxeR successfully finished"));
        else
            setWindowTitle(QObject::tr("tsMuxeR finished with error code %1").arg(exitCode));
        ui->muxLabel->setText(windowTitle() + '.');
        ui->abortBtn->setEnabled(false);
        ui->okBtn->setEnabled(true);
    }
}

void MuxForm::onAbort()
{
    if (muxProcess == nullptr)
        return;
    ui->abortBtn->setEnabled(false);
    ui->okBtn->setEnabled(true);
    setWindowTitle(QObject::tr("terminating tsMuxeR..."));
    muxProcess->kill();
    muxProcess->waitForFinished();
    setWindowTitle(QObject::tr("tsMuxeR is terminated"));
    muxProcess = nullptr;
}

void MuxForm::setProcess(QProcess *proc) { muxProcess = proc; }
