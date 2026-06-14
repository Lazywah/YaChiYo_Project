#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

#ifdef YACHIYO_HAS_LIVE2D
#include "live2dwidget.h"
#include <QFileInfo>
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ZH: 應用程式品牌資訊（影響工作列、Alt-Tab、QSettings 等）| EN: App branding (taskbar, Alt-Tab, QSettings, ...)
    QApplication::setApplicationName("YaChiYo");
    QApplication::setApplicationDisplayName("YaChiYo Desktop Pet");
    QApplication::setOrganizationName("YaChiYo");
    QApplication::setApplicationVersion("0.1");
    QApplication::setWindowIcon(QIcon(":/res/icons/app.png"));   // ZH: 視窗 / 工作列圖示 | EN: Window / taskbar icon

#ifdef YACHIYO_HAS_LIVE2D
    // ZH: L1.4 隔離測試 — 加參數 --live2d-test 即開一個普通視窗渲染 Hiyori (路徑已內建)
    //     可選: --live2d-test=<模型資料夾> 指定其他模型
    // EN: L1.4 isolated test — pass --live2d-test to open a plain window rendering Hiyori (path built-in)
    {
        QString testDir;
        bool runTest = false;
        for (const QString &arg : a.arguments())
        {
            if (arg == "--live2d-test") { runTest = true; }
            else if (arg.startsWith("--live2d-test=")) { runTest = true; testDir = arg.section('=', 1); }
        }
        if (runTest)
        {
            const QString dir  = testDir.isEmpty() ? QStringLiteral(YACHIYO_LIVE2D_SAMPLE_DIR) : testDir;
            const QString name = QFileInfo(dir).fileName();   // ZH: 資料夾名即模型名 | EN: folder name = model name
            Live2DWidget *test = new Live2DWidget();
            test->setWindowTitle("Live2D Test — " + name);
            test->resize(600, 800);
            test->setModel(dir, name);
            test->show();
            return a.exec();
        }
    }
#endif

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
