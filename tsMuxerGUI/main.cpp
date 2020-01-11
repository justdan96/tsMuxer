#include "tsmuxerwindow.h"

#include <QApplication>
#include <QUrl>

int main(int argc, char *argv[]) {
  Q_INIT_RESOURCE(images);
  QApplication app(argc, argv);
  TsMuxerWindow win;
  win.show();
  QList<QUrl> files;
  for (int i = 1; i < argc; ++i)
    files << QUrl::fromLocalFile(QString::fromLocal8Bit(argv[i]));
  if (!files.isEmpty())
    win.addFiles(files);
  return app.exec();
}
