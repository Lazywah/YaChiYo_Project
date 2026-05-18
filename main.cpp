#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    PetConfig config;
    // ZH: 可在此關閉各功能模組以方便除錯
    // EN: Feature modules can be disabled here for debugging
    // config.physicsEnabled  = false;
    // config.behaviorEnabled = false;
    // config.aiEnabled       = false;

    MainWindow w(config);
    w.show();

    return a.exec();
}
