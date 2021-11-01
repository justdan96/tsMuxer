#include <QApplication>
#include <QUrl>

#include "tsmuxerwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("Network Optix");
    app.setApplicationName("tsMuxeR");
    TsMuxerWindow win;
    win.show();
    QList<QUrl> files;
    for (int i = 1; i < argc; ++i) files << QUrl::fromLocalFile(QString::fromLocal8Bit(argv[i]));
    if (!files.isEmpty())
        win.addFiles(files);
    return app.exec();
}
