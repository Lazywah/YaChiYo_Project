#include "settingscenter.h"
#include "ui_settingscenter.h"

#include <QScreen>              // ZH: 獲取螢幕資訊 | EN: Get screen information
#include <QGuiApplication>      // ZH: 監聽螢幕變化 | EN: Listen to screen changes
#include <QMetaProperty>        // ZH: 用於操作屬性的元資訊(如獲取屬性名稱) | EN: Meta-information used to manipulate properties(such as retrieving property names)
#include <QMetaObject>          // ZH: 用於遍歷屬性清單 | EN: Used for iterating over the attribute list
#include <QVariant>             // ZH: property() 回傳的是 QVariant，必須包含此檔才能轉換數據 | EN: The property() function returns a QVariant; this file must be included to convert the data
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QTimer>

#include "mainwindow.h"

SettingsCenter::SettingsCenter(MainWindow *mainPtr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsCenter)
    , mainApp(mainPtr)
{
    ui->setupUi(this);

    initDeveloperInterface();

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &SettingsCenter::refreshDebugInfo);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index)
    {
        if (index == 1)
            updateTimer->start(100);
        else
            updateTimer->stop();
    });
}

//===============================================================================================

//===============================================================================================

void SettingsCenter::showWindow()
{
    // ZH: 將視窗移至螢幕中央
    QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();   // ZH: 獲取當前主螢幕尺寸（排除工作列後）

    //int x = (screenGeometry.width() - this->width()) / 2 + screenGeometry.left();
    //int y = (screenGeometry.height() - this->height()) / 2 + screenGeometry.top();
    //this->move(x, y);
    this->move(screenGeometry.center() - this->rect().center());
    this->show();
    this->activateWindow();   // ZH: 使視窗保持在最上層 | EN: Keep the window on top

    if (ui->tabWidget->currentIndex() == 1)
    {
        updateTimer->start(100);
    }

}

void SettingsCenter::initDeveloperInterface()
{
    const QMetaObject *metaObj = mainApp->metaObject();

    for(int i=metaObj->propertyOffset(); i<metaObj->propertyCount(); i++)
    {
        QMetaProperty prop = metaObj->property(i);
        QString propName = prop.name();

        QListWidgetItem *item = new QListWidgetItem(prop.name(), ui->watchListWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
}

void SettingsCenter::refreshDebugInfo()
{
    if (!mainApp) return;

    QString output = "=== [Developer Monitor] ===\n";
    bool hasChecked = false;

    for (int i = 0; i < ui->watchListWidget->count(); ++i)
    {
        QListWidgetItem *item = ui->watchListWidget->item(i);
        if (item->checkState() == Qt::Checked)
        {
            hasChecked = true;
            QString key = item->text();

            QVariant value = mainApp->property(key.toStdString().c_str());

            QString valStr;
            if (value.typeId() == QMetaType::Double)
            {
                valStr = QString::number(value.toDouble(), 'f', 2);
            }
            else
            {
                valStr = value.toString();
            }

            output += QString("%1 : %2\n").arg(key, -16).arg(valStr);
        }
    }

    if (!hasChecked) {
        output += "(請勾選左側項目以開始監測)";
    }

    ui->debugDisplay->setPlainText(output);
}

//===============================================================================================

//===============================================================================================

SettingsCenter::~SettingsCenter()
{
    delete ui;
}
