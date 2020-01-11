#ifndef MUX_FORM_H_
#define MUX_FORM_H_

#include <QDialog>
class QProcess;
class Ui_muxForm;

class MuxForm: public QDialog//QWidget
{
    Q_OBJECT
public:
  explicit MuxForm(QWidget* parent);
  void prepare(const QString& label);
  void setProgress(int value);
  void addStdOutLine(const QString& line);
  void addStdErrLine(const QString& line);
  void muxFinished(int exitCode, const QString& prefix);
  void setProcess(QProcess* proc);
protected:
  void closeEvent (QCloseEvent * event ) override;
private slots:
  void onProgressChanged();
  void onAbort();
private:
  int errCnt;
  Ui_muxForm* ui;
  QProcess* muxProcess;
};

#endif
