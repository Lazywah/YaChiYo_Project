#include "settingscenter.h"
#include "ui_settingscenter.h"

#include <QScreen>              // ZH: 獲取螢幕資訊 | EN: Get screen information
#include <QGuiApplication>      // ZH: 監聽螢幕變化 | EN: Listen to screen changes
#include <QMetaProperty>        // ZH: 用於操作屬性的元資訊(如獲取屬性名稱) | EN: Meta-information used to manipulate properties(such as retrieving property names)
#include <QMetaObject>          // ZH: 用於遍歷屬性清單 | EN: Used for iterating over the attribute list
#include <QVariant>             // ZH: property() 回傳的是 QVariant，必須包含此檔才能轉換數據 | EN: The property() function returns a QVariant; this file must be included to convert the data
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QTimer>
#include <QSlider>              // ZH: 滑桿控制項 | EN: Slider control
#include <QCheckBox>            // ZH: 核取方塊控制項 | EN: Checkbox control
#include <QLabel>               // ZH: 顯示數值標籤 | EN: Value display label
#include <QFormLayout>          // ZH: 表單佈局 | EN: Form layout
#include <QVBoxLayout>          // ZH: 垂直佈局 | EN: Vertical layout
#include <QHBoxLayout>          // ZH: 水平佈局（重啟/退出按鈕列）| EN: Horizontal layout (restart/quit button row)
#include <QGroupBox>            // ZH: 群組框 | EN: Group box
#include <QCoreApplication>     // ZH: 退出程式 | EN: Quit the application
#include <QLineEdit>            // ZH: 單行輸入框 | EN: Line edit
#include <QComboBox>            // ZH: 下拉選單（皮膚選擇）| EN: Combo box (skin selection)
#include <QSpinBox>             // ZH: 數值輸入框（語音埠號）| EN: Spin box (voice port)
#include <QPushButton>          // ZH: 按鈕（開發者全選）| EN: Button (developer select-all)
#include <QSignalBlocker>       // ZH: 暫時阻擋訊號（還原勾選狀態用）| EN: Temporarily block signals
#include <QScrollArea>          // ZH: 設定分頁捲動區 | EN: Settings tab scroll area
#include <QScrollBar>           // ZH: 索引籤跳轉/同步用 | EN: index jump / sync
#include <QPropertyAnimation>   // ZH: 索引籤平滑捲動 | EN: smooth scroll for the index

#include "mainwindow.h"
#include "petsettings.h"
#include "petautostart.h"
#include "petskin.h"
#include "petzguard.h"

SettingsCenter::SettingsCenter(MainWindow *mainPtr, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsCenter)
    , mainApp(mainPtr)
{
    ui->setupUi(this);
    setFixedSize(580, 560);     // ZH: 固定大小，設定變多時改由捲動區承載 | EN: fixed size; extra settings scroll instead of growing the window

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

void SettingsCenter::populateRunningApps()
{
    if (!yieldAppCombo) return;

    const QString typed = yieldAppCombo->currentText();
    yieldAppCombo->clear();
    yieldAppCombo->addItems(PetZGuard::runningWindowApps());
    yieldAppCombo->setEditText(typed);      // ZH: 保留使用者已輸入的文字 | EN: keep what the user typed
}

void SettingsCenter::showWindow()
{
    // ZH: 每次開啟重新掃描皮膚 (含剛生成的 AI 皮膚) | EN: rescan skins on open (incl. freshly generated AI skins)
    populateSkinList();
    populateRunningApps();

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
    if (existingLayout)
    {
        // ZH: 移除現有空佈局並替換 | EN: Remove existing empty layout and replace
        delete existingLayout;
    }
    QVBoxLayout *tabLayout = new QVBoxLayout(ui->tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);

    // ZH: 設定群組放進捲動區 (視窗固定大小，新增設定只會變長、可捲動) | EN: setting groups live in a scroll area (fixed window; new settings just scroll)
    QScrollArea *scrollArea = new QScrollArea(ui->tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // ZH: 只直向捲動，內容須自行縮進寬度 | EN: vertical scrolling only; content must fit the width
    QWidget *scrollContent = new QWidget;
    scrollArea->setWidget(scrollContent);

    // ZH: 左側索引籤 (點擊跳到對應群組，捲動時自動標示所在群組) | EN: left index tabs (click to jump; follows scrolling)
    QListWidget *indexList = new QListWidget(ui->tab);
    indexList->setFixedWidth(72);
    indexList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    indexList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QHBoxLayout *bodyLayout = new QHBoxLayout;
    bodyLayout->addWidget(indexList);
    bodyLayout->addWidget(scrollArea);
    tabLayout->addLayout(bodyLayout);
    QVBoxLayout *mainLayout = new QVBoxLayout(scrollContent);

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

    // ZH: 懸浮浮動幅度 (0 ~ 20 px，0 = 不上下浮動) | EN: hover sine-wave amplitude (0 ~ 20 px, 0 = no bobbing)
    QSlider *hoverAmpSlider = new QSlider(Qt::Horizontal);
    hoverAmpSlider->setRange(0, 200);       // ZH: 實際值 = slider / 10.0 | EN: Actual value = slider / 10.0
    hoverAmpSlider->setValue(static_cast<int>(s.hoverAmplitude * 10));
    QLabel *hoverAmpLabel = new QLabel(QString::number(s.hoverAmplitude, 'f', 1) + "px");
    QHBoxLayout *hoverAmpRow = new QHBoxLayout;
    hoverAmpRow->addWidget(hoverAmpSlider);
    hoverAmpRow->addWidget(hoverAmpLabel);
    physicsLayout->addRow("懸浮幅度 (Hover Amplitude):", hoverAmpRow);

    connect(hoverAmpSlider, &QSlider::valueChanged, this, [this, hoverAmpLabel](int val)
    {
        double a = val / 10.0;
        hoverAmpLabel->setText(QString::number(a, 'f', 1) + "px");
        mainApp->setHoverAmplitude(a);
    });

    mainLayout->addWidget(physicsGroup);

    // ===== ZH: 視窗設定群組 | EN: Window settings group =====
    QGroupBox *windowGroup = new QGroupBox("視窗設定 (Window)", ui->tab);
    QFormLayout *windowLayout = new QFormLayout(windowGroup);

    // ZH: 是否置頂 | EN: Always on top
    QCheckBox *alwaysOnTopCheck = new QCheckBox("啟用 (Enabled)");
    alwaysOnTopCheck->setChecked(s.alwaysOnTop);    // ZH: 載入存檔值 | EN: From saved value
    windowLayout->addRow("視窗置頂 (Always on Top):", alwaysOnTopCheck);

    // ZH: 全螢幕讓位 — 前景為全螢幕程式 (遊戲/影片) 時退到後面 | EN: yield to fullscreen apps (games/video)
    QCheckBox *yieldFullscreenCheck = new QCheckBox("啟用 (Enabled)");
    yieldFullscreenCheck->setChecked(s.yieldFullscreen);
    yieldFullscreenCheck->setToolTip("前景是全螢幕程式時，桌寵暫時退到它後面");
    windowLayout->addRow("全螢幕時讓開 (Yield to Fullscreen):", yieldFullscreenCheck);
    connect(yieldFullscreenCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        mainApp->setYieldFullscreen(checked);
    });

    // ZH: 讓位清單 (To-do 9) — 這些程式在前景時桌寵不覆蓋它 | EN: yield list (To-do 9) — pet won't cover these apps
    QListWidget *yieldList = new QListWidget;
    yieldList->addItems(s.yieldApps);
    yieldList->setMaximumHeight(90);
    yieldList->setToolTip("這些程式在前景時，桌寵暫時退到它後面");

    yieldAppCombo = new QComboBox;
    yieldAppCombo->setEditable(true);       // ZH: 下拉挑執行中程式，或手動輸入 exe 名 | EN: pick a running app or type an exe name
    yieldAppCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // ZH: 寬度不隨最長的程式名撐開 (否則整頁被推寬而裁切) | EN: don't widen to the longest app name (it pushed the page wider and clipped it)
    yieldAppCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    yieldAppCombo->setMinimumContentsLength(8);
    QPushButton *yieldAddBtn    = new QPushButton("加入 (Add)");
    QPushButton *yieldRemoveBtn = new QPushButton("移除 (Remove)");
    QHBoxLayout *yieldEditRow = new QHBoxLayout;
    yieldEditRow->addWidget(yieldAppCombo);
    yieldEditRow->addWidget(yieldAddBtn);
    yieldEditRow->addWidget(yieldRemoveBtn);

    QVBoxLayout *yieldBox = new QVBoxLayout;
    yieldBox->addWidget(yieldList);
    yieldBox->addLayout(yieldEditRow);
    windowLayout->addRow("不覆蓋的程式 (Yield List):", yieldBox);

    // ZH: 清單有變動 → 整份寫回 | EN: list changed → write the whole list back
    auto commitYieldList = [this, yieldList]()
    {
        QStringList apps;
        for (int i = 0; i < yieldList->count(); ++i)
            apps << yieldList->item(i)->text();
        mainApp->setYieldApps(apps);
    };

    connect(yieldAddBtn, &QPushButton::clicked, this, [this, yieldList, commitYieldList]()
    {
        QString exe = yieldAppCombo->currentText().trimmed().toLower();
        if (exe.isEmpty())
            return;
        if (!exe.endsWith(".exe"))
            exe += ".exe";
        if (!yieldList->findItems(exe, Qt::MatchFixedString).isEmpty())
            return;             // ZH: 已在清單中 | EN: already listed
        yieldList->addItem(exe);
        yieldAppCombo->setEditText(QString());
        commitYieldList();
    });

    connect(yieldRemoveBtn, &QPushButton::clicked, this, [yieldList, commitYieldList]()
    {
        delete yieldList->currentItem();
        commitYieldList();
    });

    // ZH: 讓位只在置頂時有意義，未置頂時停用相關控制項 | EN: yielding only matters while on top; disable otherwise
    auto setYieldEnabled = [=](bool on)
    {
        yieldFullscreenCheck->setEnabled(on);
        yieldList->setEnabled(on);
        yieldAppCombo->setEnabled(on);
        yieldAddBtn->setEnabled(on);
        yieldRemoveBtn->setEnabled(on);
    };
    setYieldEnabled(s.alwaysOnTop);

    connect(alwaysOnTopCheck, &QCheckBox::toggled, this, [this, setYieldEnabled](bool checked)
    {
        mainApp->setAlwaysOnTop(checked);
        setYieldEnabled(checked);
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

    // ===== ZH: 語音助手設定群組 (重啟生效) | EN: Voice assistant group (restart-applied) =====
    QGroupBox *voiceGroup = new QGroupBox("語音助手 (Voice Assistant)", ui->tab);
    QFormLayout *voiceLayout = new QFormLayout(voiceGroup);

    // ZH: 語音連動開關 (啟用後桌寵會對語音助手事件做狀態反應) | EN: voice-bridge toggle
    QCheckBox *voiceCheck = new QCheckBox("啟用 (重啟生效 / restart to apply)");
    voiceCheck->setChecked(s.voiceEnabled);
    voiceLayout->addRow("語音連動 (Voice Bridge):", voiceCheck);
    connect(voiceCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        mainApp->setVoiceModeSetting(checked);
    });

    // ZH: 事件接收埠 (localhost)；語音助手需往此埠發事件 | EN: event listen port (localhost)
    QSpinBox *portSpin = new QSpinBox;
    portSpin->setRange(1024, 65535);
    portSpin->setValue(s.voicePort);
    portSpin->setToolTip("語音助手往 127.0.0.1:<埠> 發 POST /pet/event");
    voiceLayout->addRow("接收埠 (Port, 重啟生效):", portSpin);
    connect(portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val)
    {
        mainApp->setVoicePortSetting(val);
    });

    mainLayout->addWidget(voiceGroup);

    // ===== ZH: 應用程式群組 (重啟 / 退出，免去每次找托盤) | EN: Application group (restart/quit without the tray) =====
    QGroupBox *appGroup = new QGroupBox("應用程式 (Application)");
    QHBoxLayout *appLayout = new QHBoxLayout(appGroup);

    QPushButton *restartButton = new QPushButton("重啟桌寵 (Restart)");
    restartButton->setToolTip("以相同參數重新啟動，套用「重啟生效」的設定（如 Live2D / 語音）");
    connect(restartButton, &QPushButton::clicked, this, [this]()
    {
        mainApp->restartApp();
    });

    QPushButton *quitButton = new QPushButton("退出程式 (Quit)");
    connect(quitButton, &QPushButton::clicked, this, []()
    {
        QCoreApplication::quit();
    });

    appLayout->addWidget(restartButton);
    appLayout->addWidget(quitButton);

    // ZH: 底部彈性留白 | EN: Bottom spacer
    mainLayout->addStretch();

    // ZH: 標籤 + 欄位放不下時，欄位換到標籤下一行 (避免撐寬捲動區) | EN: wrap a field below its label when the row doesn't fit
    for (QFormLayout *form : scrollContent->findChildren<QFormLayout *>())
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    // ZH: 索引籤 — 新增群組時在此加一行即可 | EN: index tabs — add one line here per new group
    const QList<QPair<QString, QGroupBox *>> sections = {
        { "行為", behaviorGroup },
        { "外觀", appearanceGroup },
        { "物理", physicsGroup },
        { "視窗", windowGroup },
        { "語音", voiceGroup },
    };
    for (const auto &sec : sections)
        indexList->addItem(sec.first);
    indexList->setCurrentRow(0);

    QScrollBar *vbar = scrollArea->verticalScrollBar();
    QPropertyAnimation *scrollAnim = new QPropertyAnimation(vbar, "value", this);
    scrollAnim->setDuration(220);
    scrollAnim->setEasingCurve(QEasingCurve::OutCubic);

    // ZH: 點擊索引 → 平滑捲到該群組頂端 | EN: click an index → smooth-scroll to that group's top
    connect(indexList, &QListWidget::itemClicked, this, [=](QListWidgetItem *item)
    {
        const QGroupBox *group = sections.at(indexList->row(item)).second;
        scrollAnim->stop();
        scrollAnim->setStartValue(vbar->value());
        scrollAnim->setEndValue(qMin(group->y() - mainLayout->contentsMargins().top(), vbar->maximum()));
        scrollAnim->start();
    });

    // ZH: 捲動時標示目前所在群組 (捲到底時標最後一個) | EN: highlight the group in view (last one when scrolled to the bottom)
    connect(vbar, &QScrollBar::valueChanged, this, [=](int value)
    {
        int row = 0;
        if (value >= vbar->maximum() && vbar->maximum() > 0)
            row = sections.size() - 1;
        else
            for (int i = 0; i < sections.size(); ++i)
                if (sections.at(i).second->y() <= value + mainLayout->contentsMargins().top() + 10)
                    row = i;
        QSignalBlocker blocker(indexList);
        indexList->setCurrentRow(row);
    });

    // ZH: 重啟/退出固定在捲動區下方，不必捲到底才找得到 | EN: restart/quit pinned below the scroll area, always reachable
    tabLayout->addWidget(appGroup);
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
