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
#include <QSlider>              // ZH: 滑桿控制項 | EN: Slider control
#include <QCheckBox>            // ZH: 核取方塊控制項 | EN: Checkbox control
#include <QLabel>               // ZH: 顯示數值標籤 | EN: Value display label
#include <QFormLayout>          // ZH: 表單佈局 | EN: Form layout
#include <QVBoxLayout>          // ZH: 垂直佈局 | EN: Vertical layout
#include <QGroupBox>            // ZH: 群組框 | EN: Group box
#include <QLineEdit>            // ZH: 單行輸入框 | EN: Line edit
#include <QComboBox>            // ZH: 下拉選單（皮膚選擇）| EN: Combo box (skin selection)
#include <QSignalBlocker>       // ZH: 暫時阻擋訊號（還原勾選狀態用）| EN: Temporarily block signals

#include "mainwindow.h"
#include "petsettings.h"
#include "petautostart.h"
#include "petskin.h"

SettingsCenter::SettingsCenter(MainWindow *mainPtr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsCenter)
    , mainApp(mainPtr)
{
    ui->setupUi(this);

    initSettingsInterface();
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
    // ZH: 將視窗移至當前螢幕中央 | EN: Move window to center of current screen
    QScreen *screen = QGuiApplication::screenAt(mainApp->geometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
        
    QRect screenGeometry = screen->availableGeometry();

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

void SettingsCenter::initSettingsInterface()
{
    // ZH: 取得設定分頁的佈局 | EN: Get the layout of the settings tab
    QLayout *existingLayout = ui->tab->layout();
    QVBoxLayout *mainLayout;

    if (existingLayout)
    {
        // ZH: 移除現有空佈局並替換 | EN: Remove existing empty layout and replace
        delete existingLayout;
    }
    mainLayout = new QVBoxLayout(ui->tab);

    // ZH: 載入已儲存的設定，作為各控制項的初始值 | EN: Load persisted settings to initialise controls
    PetSettingsData s = PetSettings::load();

    // ===== ZH: 行為設定群組 | EN: Behavior settings group =====
    QGroupBox *behaviorGroup = new QGroupBox("行為設定 (Behavior)", ui->tab);
    QFormLayout *behaviorLayout = new QFormLayout(behaviorGroup);

    // ZH: 行走速度 (0.5 ~ 5.0) | EN: Walk speed (0.5 ~ 5.0)
    QSlider *walkSpeedSlider = new QSlider(Qt::Horizontal);
    walkSpeedSlider->setRange(5, 50);       // ZH: 實際值 = slider / 10.0 | EN: Actual value = slider / 10.0
    walkSpeedSlider->setValue(static_cast<int>(s.walkSpeed * 10));  // ZH: 載入存檔值 | EN: From saved value
    QLabel *walkSpeedLabel = new QLabel(QString::number(s.walkSpeed, 'f', 1));
    QHBoxLayout *walkSpeedRow = new QHBoxLayout;
    walkSpeedRow->addWidget(walkSpeedSlider);
    walkSpeedRow->addWidget(walkSpeedLabel);
    behaviorLayout->addRow("行走速度 (Walk Speed):", walkSpeedRow);

    connect(walkSpeedSlider, &QSlider::valueChanged, this, [this, walkSpeedLabel](int val)
    {
        double speed = val / 10.0;
        walkSpeedLabel->setText(QString::number(speed, 'f', 1));
        mainApp->setWalkSpeed(speed);
    });

    // ZH: 決策間隔 (1s ~ 15s) | EN: Decision interval (1s ~ 15s)
    QSlider *intervalSlider = new QSlider(Qt::Horizontal);
    intervalSlider->setRange(1000, 15000);
    intervalSlider->setSingleStep(500);
    intervalSlider->setValue(s.behaviorInterval);   // ZH: 載入存檔值 | EN: From saved value
    QLabel *intervalLabel = new QLabel(QString::number(s.behaviorInterval / 1000.0, 'f', 1) + "s");
    QHBoxLayout *intervalRow = new QHBoxLayout;
    intervalRow->addWidget(intervalSlider);
    intervalRow->addWidget(intervalLabel);
    behaviorLayout->addRow("決策間隔 (Decision Interval):", intervalRow);

    connect(intervalSlider, &QSlider::valueChanged, this, [this, intervalLabel](int val)
    {
        intervalLabel->setText(QString::number(val / 1000.0, 'f', 1) + "s");
        mainApp->setBehaviorInterval(val);
    });

    mainLayout->addWidget(behaviorGroup);

    // ===== ZH: 外觀設定群組 | EN: Appearance settings group =====
    QGroupBox *appearanceGroup = new QGroupBox("外觀設定 (Appearance)", ui->tab);
    QFormLayout *appearanceLayout = new QFormLayout(appearanceGroup);

    // ZH: 皮膚選擇 (掃描內建 + 使用者 skins/ 目錄) | EN: Skin selection (scans built-in + user skins/)
    QComboBox *skinCombo = new QComboBox;
    const QList<PetSkin::SkinEntry> skins = PetSkin::available();
    for (const PetSkin::SkinEntry &e : skins)
    {
        skinCombo->addItem(e.name, e.id);           // ZH: 顯示名稱，資料存 id | EN: display name, store id
        if (e.id == s.currentSkin)
            skinCombo->setCurrentIndex(skinCombo->count() - 1);
    }
    appearanceLayout->addRow("皮膚 (Skin):", skinCombo);

    connect(skinCombo, &QComboBox::currentIndexChanged, this, [this, skinCombo](int)
    {
        mainApp->setSkin(skinCombo->currentData().toString());
    });

    // ZH: 桌寵大小 (50% ~ 200%) | EN: Pet scale (50% ~ 200%)
    QSlider *scaleSlider = new QSlider(Qt::Horizontal);
    scaleSlider->setRange(50, 200);
    scaleSlider->setValue(static_cast<int>(s.petScale * 100));  // ZH: 載入存檔值 | EN: From saved value
    QLabel *scaleLabel = new QLabel(QString::number(static_cast<int>(s.petScale * 100)) + "%");
    QHBoxLayout *scaleRow = new QHBoxLayout;
    scaleRow->addWidget(scaleSlider);
    scaleRow->addWidget(scaleLabel);
    appearanceLayout->addRow("桌寵大小 (Pet Size):", scaleRow);

    connect(scaleSlider, &QSlider::valueChanged, this, [this, scaleLabel](int val)
    {
        scaleLabel->setText(QString::number(val) + "%");
        mainApp->setPetScale(val / 100.0);
    });

    // ZH: GIF 動畫皮膚模式 (關閉時僅使用 PNG) | EN: GIF skin mode (PNG only when off)
    QCheckBox *gifModeCheck = new QCheckBox("啟用 (Enabled)");
    gifModeCheck->setChecked(s.gifSkin);    // ZH: 載入存檔值 | EN: From saved value
    appearanceLayout->addRow("GIF 動畫皮膚 (GIF Skin):", gifModeCheck);

    connect(gifModeCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        mainApp->setPetSkinType(checked ? 1 : 0);
    });

    mainLayout->addWidget(appearanceGroup);

    // ===== ZH: 物理設定群組 | EN: Physics settings group =====
    QGroupBox *physicsGroup = new QGroupBox("物理設定 (Physics)", ui->tab);
    QFormLayout *physicsLayout = new QFormLayout(physicsGroup);

    // ZH: 重力強度 (0.1 ~ 3.0) | EN: Gravity strength (0.1 ~ 3.0)
    QSlider *gravitySlider = new QSlider(Qt::Horizontal);
    gravitySlider->setRange(1, 30);         // ZH: 實際值 = slider / 10.0 | EN: Actual value = slider / 10.0
    gravitySlider->setValue(static_cast<int>(s.gravity * 10));  // ZH: 載入存檔值 | EN: From saved value
    QLabel *gravityLabel = new QLabel(QString::number(s.gravity, 'f', 1));
    QHBoxLayout *gravityRow = new QHBoxLayout;
    gravityRow->addWidget(gravitySlider);
    gravityRow->addWidget(gravityLabel);
    physicsLayout->addRow("重力強度 (Gravity):", gravityRow);

    connect(gravitySlider, &QSlider::valueChanged, this, [this, gravityLabel](int val)
    {
        double g = val / 10.0;
        gravityLabel->setText(QString::number(g, 'f', 1));
        mainApp->setGravity(g);
    });

    mainLayout->addWidget(physicsGroup);

    // ===== ZH: 視窗設定群組 | EN: Window settings group =====
    QGroupBox *windowGroup = new QGroupBox("視窗設定 (Window)", ui->tab);
    QFormLayout *windowLayout = new QFormLayout(windowGroup);

    // ZH: 是否置頂 | EN: Always on top
    QCheckBox *alwaysOnTopCheck = new QCheckBox("啟用 (Enabled)");
    alwaysOnTopCheck->setChecked(s.alwaysOnTop);    // ZH: 載入存檔值 | EN: From saved value
    windowLayout->addRow("視窗置頂 (Always on Top):", alwaysOnTopCheck);

    connect(alwaysOnTopCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        mainApp->setAlwaysOnTop(checked);
    });

    // ZH: 開機自啟動（以登錄檔為真實來源，故直接讀寫 PetAutostart）
    // EN: Run at startup (registry is the source of truth, so read/write PetAutostart directly)
    QCheckBox *autostartCheck = new QCheckBox("啟用 (Enabled)");
    autostartCheck->setChecked(PetAutostart::isEnabled());
    windowLayout->addRow("開機自啟動 (Run at Startup):", autostartCheck);

    connect(autostartCheck, &QCheckBox::toggled, this, [autostartCheck](bool checked)
    {
        // ZH: 設定失敗時還原勾選狀態，避免 UI 與實際不符 | EN: Revert checkbox if the change fails
        if (!PetAutostart::setEnabled(checked))
        {
            QSignalBlocker blocker(autostartCheck);
            autostartCheck->setChecked(PetAutostart::isEnabled());
        }
    });

    mainLayout->addWidget(windowGroup);

    // ===== ZH: AI 設定群組 | EN: AI settings group =====
    QGroupBox *aiGroup = new QGroupBox("AI 設定 (AI)", ui->tab);
    QFormLayout *aiLayout = new QFormLayout(aiGroup);

    // ZH: AI 提示詞 | EN: AI Prompt
    QLineEdit *promptEdit = new QLineEdit(s.aiPrompt);  // ZH: 載入存檔值 | EN: From saved value
    aiLayout->addRow("AI 提示詞 (Prompt):", promptEdit);

    connect(promptEdit, &QLineEdit::textChanged, this, [this](const QString &text)
    {
        mainApp->setAiPrompt(text);
    });

    mainLayout->addWidget(aiGroup);

    // ZH: 底部彈性留白 | EN: Bottom spacer
    mainLayout->addStretch();
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

            QVariant value = mainApp->property(qPrintable(key));

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
