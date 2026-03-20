#ifndef SETTINGSCENTER_H
#define SETTINGSCENTER_H

#include <QDialog>

namespace Ui {
class SettingsCenter;
}

class MainWindow;

class SettingsCenter : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsCenter(MainWindow *mainApp, QWidget *parent = nullptr);

    void showWindow();  // ZH: 右鍵選單點擊後顯示視窗 | EN: Right-clicking the menu displays a window

    ~SettingsCenter();

private slots:
    void refreshDebugInfo();        // ZH: 定時重新整理邏輯 | EN: Regularly reorganize the logic

private:
    Ui::SettingsCenter *ui;
    MainWindow *mainApp;

    void initDeveloperInterface();  // ZH: 開發者介面初始化邏輯 | EN: Developer interface initialization logic

    QTimer *updateTimer;

protected:
};

#endif // SETTINGSCENTER_H
