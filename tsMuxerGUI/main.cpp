#include <QApplication>
#include <QLibraryInfo>
#include <QTranslator>
#include <QUrl>

#include "tsmuxerwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTranslator tsMuxerTranslator;
    tsMuxerTranslator.load(QLocale::system(), "tsmuxergui_", "", ":/i18n");
    app.installTranslator(&tsMuxerTranslator);
    QTranslator qtCoreTranslator;
    qtCoreTranslator.load(QLocale::system(), "qtbase_", "", QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtCoreTranslator);

    TsMuxerWindow win;
    win.show();
    QList<QUrl> files;
    for (int i = 1; i < argc; ++i) files << QUrl::fromLocalFile(QString::fromLocal8Bit(argv[i]));
    if (!files.isEmpty())
        win.addFiles(files);
    return app.exec();
}
