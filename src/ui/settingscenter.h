#ifndef SETTINGSCENTER_H
#define SETTINGSCENTER_H

#include <QDialog>

namespace Ui {
class SettingsCenter;
}

class MainWindow;
class QSlider;
class QCheckBox;
class QComboBox;

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

    void initSettingsInterface();   // ZH: 設定介面初始化邏輯 | EN: Settings interface initialization logic
    void initDeveloperInterface();  // ZH: 開發者介面初始化邏輯 | EN: Developer interface initialization logic
    void populateSkinList();        // ZH: 重新掃描並填入皮膚下拉 (每次開啟刷新) | EN: rescan & fill the skin combo (refresh on open)

    QComboBox *skinCombo = nullptr; // ZH: 皮膚下拉 (成員，供刷新用) | EN: skin combo (member, for refresh)
    QTimer *updateTimer;

protected:
};

#endif // SETTINGSCENTER_H

