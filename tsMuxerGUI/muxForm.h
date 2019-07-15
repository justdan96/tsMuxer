#ifndef __MUX_FORM_H
#define __MUX_FORM_H

#include <QtGui>
#include "ui_muxForm.h"

class MuxForm: public QDialog//QWidget
{
    Q_OBJECT
public:
	MuxForm(QWidget* parent);
	void prepare(const QString& label);
	void setProgress(int value);
	void addStdOutLine(const QString& line);
	void addStdErrLine(const QString& line);
	void muxFinished(int exitCode, const QString& prefix);
	void setProcess(QProcess* proc);
protected:
	virtual void closeEvent (QCloseEvent * event );
private slots:
	void onProgressChanged();
	void onAbort();
private:
	int errCnt;
	Ui_muxForm ui;
	QProcess* muxProcess;
};

#endif
