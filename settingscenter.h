#ifndef SETTINGSCENTER_H
#define SETTINGSCENTER_H

#include <QDialog>

namespace Ui {
class SettingsCenter;
}

class SettingsCenter : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsCenter(QWidget *parent = nullptr);

    void showWindow();  // ZH: 右鍵選單點擊後顯示視窗 | EN: Right-clicking the menu displays a window

    ~SettingsCenter();

private:
    Ui::SettingsCenter *ui;

protected:
};

#endif // SETTINGSCENTER_H
