#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ZH: 應用程式品牌資訊（影響工作列、Alt-Tab、QSettings 等）| EN: App branding (taskbar, Alt-Tab, QSettings, ...)
    QApplication::setApplicationName("YaChiYo");
    QApplication::setApplicationDisplayName("YaChiYo Desktop Pet");
    QApplication::setOrganizationName("YaChiYo");
    QApplication::setApplicationVersion("0.1");
    QApplication::setWindowIcon(QIcon(":/res/icons/app.png"));   // ZH: 視窗 / 工作列圖示 | EN: Window / taskbar icon

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
