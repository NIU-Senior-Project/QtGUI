#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QFile>

static void applyGlassTheme(QApplication &app)
{
    QFile f(":/glass.qss");   // ✅ 走 qrc
    if (f.open(QFile::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(f.readAll()));
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    applyGlassTheme(a);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "GpuClient_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
