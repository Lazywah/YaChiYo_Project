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
#include <QPushButton>          // ZH: 按鈕（開發者全選）| EN: Button (developer select-all)
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

void SettingsCenter::populateSkinList()
{
    if (!skinCombo) return;

    // ZH: 阻擋訊號避免重填時誤觸發 setSkin | EN: block signals so refilling doesn't trigger setSkin
    QSignalBlocker blocker(skinCombo);

    const QString current = PetSettings::load().currentSkin;
    skinCombo->clear();
    for (const PetSkin::SkinEntry &e : PetSkin::available())
    {
        skinCombo->addItem(e.name, e.id);       // ZH: 顯示名稱，資料存 id | EN: display name, store id
        if (e.id == current)
            skinCombo->setCurrentIndex(skinCombo->count() - 1);
    }
}

void SettingsCenter::showWindow()
{
    // ZH: 每次開啟重新掃描皮膚 (含剛生成的 AI 皮膚) | EN: rescan skins on open (incl. freshly generated AI skins)
    populateSkinList();

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

    // ZH: 移動開關 (關閉時不主動散步，但仍受重力/可拖曳) | EN: movement toggle (no wandering; gravity/drag still work)
    QCheckBox *movementCheck = new QCheckBox("啟用 (Enabled)");
    movementCheck->setChecked(s.movementEnabled);
    behaviorLayout->addRow("自動移動 (Movement):", movementCheck);
    connect(movementCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        mainApp->setMovementEnabled(checked);
    });

    // ZH: 飛行開關 (關閉時不會起飛；空中時會落地或改懸浮) | EN: flying toggle
    QCheckBox *flyingCheck = new QCheckBox("啟用 (Enabled)");
    flyingCheck->setChecked(s.flyingEnabled);
    behaviorLayout->addRow("飛行 (Flying):", flyingCheck);

    // ZH: 懸浮開關 (飛行開時連動開且不可改；飛行關時為獨立漂浮事件) | EN: hovering toggle (coupled+locked when flying on; independent when flying off)
    QCheckBox *hoveringCheck = new QCheckBox("啟用 (Enabled)");
    hoveringCheck->setChecked(s.flyingEnabled || s.hoveringEnabled);
    hoveringCheck->setEnabled(!s.flyingEnabled);
    behaviorLayout->addRow("懸浮 (Hovering):", hoveringCheck);

    connect(flyingCheck, &QCheckBox::toggled, this, [this, hoveringCheck](bool checked)
    {
        mainApp->setFlyingEnabled(checked);
        // ZH: 飛行開→懸浮連動開且鎖定；飛行關→懸浮恢復自身設定值可編輯 | EN: flying on → hovering forced on+locked; off → restore editable
        hoveringCheck->setEnabled(!checked);
        QSignalBlocker blocker(hoveringCheck);
        hoveringCheck->setChecked(checked ? true : PetSettings::load().hoveringEnabled);
    });

    connect(hoveringCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        mainApp->setHoveringEnabled(checked);
    });

    mainLayout->addWidget(behaviorGroup);

    // ===== ZH: 外觀設定群組 | EN: Appearance settings group =====
    const bool live2d = mainApp->isLive2dMode();   // ZH: 目前是否 Live2D 模式 | EN: currently Live2D mode

    QGroupBox *appearanceGroup = new QGroupBox("外觀設定 (Appearance)", ui->tab);
    QFormLayout *appearanceLayout = new QFormLayout(appearanceGroup);

    // ZH: 皮膚 / GIF 僅在「幀皮膚」模式有效，Live2D 模式隱藏 | EN: skin/GIF only in frame mode; hidden in Live2D
    if (!live2d)
    {
        skinCombo = new QComboBox;
        populateSkinList();
        appearanceLayout->addRow("皮膚 (Skin):", skinCombo);
        connect(skinCombo, &QComboBox::currentIndexChanged, this, [this](int)
        {
            mainApp->setSkin(skinCombo->currentData().toString());
        });
    }

    // ZH: 桌寵大小 (50% ~ 200%) — 兩種模式皆有效 | EN: Pet scale — works in both modes
    QSlider *scaleSlider = new QSlider(Qt::Horizontal);
    scaleSlider->setRange(50, 200);
    scaleSlider->setValue(static_cast<int>(s.petScale * 100));
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

    if (!live2d)
    {
        // ZH: GIF 動畫皮膚模式 (幀皮膚專用) | EN: GIF skin mode (frame-skin only)
        QCheckBox *gifModeCheck = new QCheckBox("啟用 (Enabled)");
        gifModeCheck->setChecked(s.gifSkin);
        appearanceLayout->addRow("GIF 動畫皮膚 (GIF Skin):", gifModeCheck);
        connect(gifModeCheck, &QCheckBox::toggled, this, [this](bool checked)
        {
            mainApp->setPetSkinType(checked ? 1 : 0);
        });
    }
    else
    {
        // ZH: Live2D 視窗寬度 (Live2D 專用) | EN: Live2D window width (Live2D only)
        QSlider *widthSlider = new QSlider(Qt::Horizontal);
        widthSlider->setRange(120, 400);
        widthSlider->setValue(s.live2dWidth);
        QLabel *widthLabel = new QLabel(QString::number(s.live2dWidth) + "px");
        QHBoxLayout *widthRow = new QHBoxLayout;
        widthRow->addWidget(widthSlider);
        widthRow->addWidget(widthLabel);
        appearanceLayout->addRow("視窗寬度 (Window Width):", widthRow);
        connect(widthSlider, &QSlider::valueChanged, this, [this, widthLabel](int val)
        {
            widthLabel->setText(QString::number(val) + "px");
            mainApp->setLive2dWidth(val);
        });
    }

    // ZH: Live2D 模式開關 (重啟生效) | EN: Live2D mode toggle (applied on restart)
    QCheckBox *live2dCheck = new QCheckBox("啟用 (重啟生效 / restart to apply)");
    live2dCheck->setChecked(s.live2dMode);
    appearanceLayout->addRow("Live2D 模式 (Live2D Mode):", live2dCheck);
    connect(live2dCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        mainApp->setLive2dModeSetting(checked);
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

    // ZH: 在監測清單下方加「全選/全不選」按鈕 | EN: add a select-all/none button below the watch list
    QPushButton *selectAllBtn = new QPushButton("全選 / 全不選 (Select All)");
    QVBoxLayout *leftCol = new QVBoxLayout;
    ui->horizontalLayout_2->removeWidget(ui->watchListWidget);
    leftCol->addWidget(ui->watchListWidget);
    leftCol->addWidget(selectAllBtn);
    ui->horizontalLayout_2->insertLayout(0, leftCol);

    connect(selectAllBtn, &QPushButton::clicked, this, [this]()
    {
        // ZH: 若全部已勾 → 全部取消，否則 → 全選 | EN: all checked → uncheck all, else check all
        bool allChecked = true;
        for (int i = 0; i < ui->watchListWidget->count(); ++i)
            if (ui->watchListWidget->item(i)->checkState() != Qt::Checked) { allChecked = false; break; }
        const Qt::CheckState target = allChecked ? Qt::Unchecked : Qt::Checked;
        for (int i = 0; i < ui->watchListWidget->count(); ++i)
            ui->watchListWidget->item(i)->setCheckState(target);
    });
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
