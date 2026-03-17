#include "settingscenter.h"
#include "ui_settingscenter.h"

#include <QScreen>              // ZH: 獲取螢幕資訊 | EN: Get screen information
#include <QGuiApplication>      // ZH: 監聽螢幕變化 | EN: Listen to screen changes

SettingsCenter::SettingsCenter(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsCenter)
{
    ui->setupUi(this);
}

void SettingsCenter::showWindow()
{
    // ZH: 將視窗移至螢幕中央
    QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();   // ZH: 獲取當前主螢幕尺寸（排除工作列後）
    this->move(screenGeometry.center() - this->rect().center());
    this->show();
    this->activateWindow();   // ZH: 使視窗保持在最上層 | EN: Keep the window on top
}

SettingsCenter::~SettingsCenter()
{
    delete ui;
}
