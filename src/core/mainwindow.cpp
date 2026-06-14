#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QMetaEnum>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QPixmap>
#include <QMovie>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QToolTip>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QProgressDialog>
#include <QDateTime>
#include <QCoreApplication>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef YACHIYO_HAS_LIVE2D
#include "live2dwidget.h"
#endif

//===============================================================================================

//===============================================================================================

MainWindow::MainWindow(const PetConfig &config, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , config(config)
{
    ui->setupUi(this);

    // ZH: 載入並套用持久化設定（須在計時器啟動前，確保 behaviorInterval 生效）
    // EN: Load & apply persisted settings before timers start (so behaviorInterval takes effect)
    PetSettingsData saved = PetSettings::load();
    applySettings(saved);

    // ZH: 載入存檔指定的皮膚（資料驅動，定義於 skin.json）| EN: Load the saved skin (data-driven via skin.json)
    loadSkinById(saved.currentSkin);

#ifdef YACHIYO_HAS_LIVE2D
    // ZH: Live2D 模式 — 用 Live2DWidget 取代 QLabel 渲染 (須在建立 SettingsCenter 前設好 m_live2dMode)
    // EN: Live2D mode — replace QLabel with a Live2DWidget (must set m_live2dMode before SettingsCenter is created)
    if (config.live2dEnabled && !config.live2dModelDir.isEmpty())
    {
        m_live2dMode = true;
        m_live2d = new Live2DWidget(this);
        setCentralWidget(m_live2d);
        const QString name = QFileInfo(config.live2dModelDir).fileName();
        m_live2d->setModel(config.live2dModelDir, name);
        applyLive2DSize();   // ZH: 依 petScale 設定視窗大小 | EN: size by petScale
    }
#endif

    // ZH: 設定中心須在 m_live2dMode 確定後建立 (其 UI 依模式不同) | EN: create after m_live2dMode is known (UI depends on it)
    settingsCenter = new SettingsCenter(this, this);

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::FramelessWindowHint;
    if (saved.alwaysOnTop)              // ZH: 依設定決定是否置頂 | EN: Honour saved always-on-top
        flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground);

    initAllConnect();
    initTrayIcon();
    setState(Standing);
}

//===============================================================================================

//===============================================================================================

void MainWindow::initAllConnect()
{
    physicsTimer = new QTimer(this);
    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::updatePhysics);
    if (config.physicsEnabled)
        physicsTimer->start(16);    // ZH: 約 60 FPS | EN: ~60 FPS

    behaviorTimer = new QTimer(this);
    connect(behaviorTimer, &QTimer::timeout, this, &MainWindow::decideNextAction);
    if (config.behaviorEnabled && m_movementEnabled)   // ZH: 移動開關關閉時不啟動決策 | EN: don't start decisions if movement is off
        behaviorTimer->start(behaviorInterval);

    imageSwitchTimer = new QTimer(this);
    connect(imageSwitchTimer, &QTimer::timeout, this, &MainWindow::turnImageSet);

    if (config.aiEnabled)
    {
        aiClient = new AIClient(this);
        connect(aiClient, &AIClient::resultReady,    this, &MainWindow::onAIResultReady);
        connect(aiClient, &AIClient::skinReady,      this, &MainWindow::onSkinReady);
        connect(aiClient, &AIClient::errorOccurred,  this, &MainWindow::onAIError);
    }

    // ZH: 音效模組（檔案缺失時靜默，不影響運作）| EN: Sound module (silent if files are missing)
    sound = new PetSound(this);
    sound->setEnabled(config.soundEnabled);
}

//===============================================================================================
// ZH: 滑鼠事件 | EN: Mouse events
//===============================================================================================

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    QAction *settingsAction = menu.addAction("設定中心");
    connect(settingsAction, &QAction::triggered, this, [this]()
    {
        settingsCenter->showWindow();
    });

    menu.addSeparator();

    QAction *aiAction = menu.addAction("AI 變身");
    connect(aiAction, &QAction::triggered, this, [this]()
    {
        requestAIProcessing(aiPrompt);
    });

    // ZH: AI 生成整套皮膚 (上傳參考圖) | EN: AI generate a whole skin (upload reference)
    QAction *aiSkinAction = menu.addAction("AI 生成皮膚 (上傳圖片)…");
    connect(aiSkinAction, &QAction::triggered, this, [this]()
    {
        requestSkinGeneration();
    });

    menu.addSeparator();

    QAction *closeAction = menu.addAction("關閉/隱藏桌寵");
    connect(closeAction, &QAction::triggered, this, &MainWindow::hide);

    menu.exec(event->globalPos());
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        setState(Captured);
        if (sound) sound->playGrab();
        m_offset   = event->globalPosition().toPoint() - this->pos();
        m_pressPos = event->globalPosition().toPoint();   // ZH: 記錄按下位置 | EN: record press position
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        move(event->globalPosition().toPoint() - m_offset);

#ifdef YACHIYO_HAS_LIVE2D
        // ZH: 拖曳時讓 Hiyori 看向滑鼠 (相對視窗中心) | EN: while dragging, look toward the cursor (relative to window center)
        if (m_live2dMode && m_live2d)
        {
            QPointF c = event->globalPosition() - QPointF(this->x() + width() / 2.0, this->y() + height() / 2.0);
            float nx = qBound(-1.0, c.x() / (width()  / 2.0), 1.0);
            float ny = qBound(-1.0, c.y() / (height() / 2.0), 1.0);
            m_live2d->setLookDirection(static_cast<float>(nx), static_cast<float>(ny));
        }
#endif
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // ZH: 移動距離很小視為「點擊身體」→ 播 TapBody | EN: small movement = body tap → play TapBody
        const bool isTap = (event->globalPosition().toPoint() - m_pressPos).manhattanLength() < 6;

        QRect screenRect = getCurrentScreenRect();
        int groundY = screenRect.bottom() - this->height();

        if (this->y() < groundY - 10)
            setState(Hovering);
        else
            setState(Standing);

        if (sound) sound->playRelease();
        physicsTimer->start(16);

#ifdef YACHIYO_HAS_LIVE2D
        if (m_live2dMode && m_live2d)
        {
            m_live2d->setLookDirection(0.0f, 0.0f);   // ZH: 放開後回正面 | EN: reset to front on release
            if (isTap) m_live2d->playTapBody();        // ZH: 點擊身體互動動作 | EN: body-tap interaction motion
        }
#endif
    }
}

//===============================================================================================
// ZH: 外觀 & 動畫 | EN: Appearance & animation
//===============================================================================================

void MainWindow::updatePetSkin()
{
    // ZH: Live2D 模式由 Live2DWidget 自行渲染，跳過幀皮膚邏輯 | EN: Live2D mode renders itself; skip frame-skin logic
    if (m_live2dMode)
        return;

    QMetaEnum metaEnum = QMetaEnum::fromType<MainWindow::State>();
    QString stateName  = metaEnum.valueToKey(currentState);

    if (currentMovie)
    {
        currentMovie->stop();
        currentMovie->deleteLater();
        currentMovie = nullptr;
    }

    int scaledSize = static_cast<int>(skin.scale() * petScale);
    PetSkin::StateInfo info = skin.state(stateName);

    // ZH: GIF 模式開啟且該狀態為 gif 型且檔案存在才播放動圖 | EN: Play GIF only when GIF mode is on, the state is a gif type, and the file exists
    QString gifPath = skin.gifPath(stateName);
    if (petSkinType == 1 && info.kind == PetSkin::StateInfo::Gif && !gifPath.isEmpty())
    {
        currentMovie = new QMovie(gifPath);

        QPixmap tempPix(gifPath);
        if (!tempPix.isNull())
        {
            QSize newSize = tempPix.size().scaled(scaledSize, scaledSize, Qt::KeepAspectRatio);
            currentMovie->setScaledSize(newSize);
        }
        else
        {
            currentMovie->setScaledSize(QSize(scaledSize, scaledSize));
        }

        ui->label->setMovie(currentMovie);
        currentMovie->start();
        ui->label->adjustSize();
        this->adjustSize();
        return;
    }

    // ZH: 依序解析圖片路徑：序列幀 → 該狀態 png → 退回狀態 png → 預設 Standing
    // EN: Resolve image path in order: frame → state png → fallback-state png → default Standing
    QString path;
    if (info.kind == PetSkin::StateInfo::Frames && currentSetNumber > 0)
        path = skin.framePath(stateName, currentSetNumber);
    else
        path = skin.pngPath(stateName);

    if (path.isEmpty())
        path = skin.pngPath(skin.fallbackState(stateName));
    if (path.isEmpty())
        path = skin.pngPath("Standing");

    QPixmap pix;
    pix.load(path);

    if (qAbs(physics.currentVelocityX) > 0.1 && physics.currentVelocityX < -0.1)
        pix = QPixmap::fromImage(pix.toImage().flipped(Qt::Horizontal));

    ui->label->setPixmap(pix.scaled(scaledSize, scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->label->adjustSize();
    this->adjustSize();
}

void MainWindow::turnImageSet()
{
    PetSkin::StateInfo info = skin.state(QMetaEnum::fromType<State>().valueToKey(currentState));
    if (info.kind != PetSkin::StateInfo::Frames || currentState == AI_Processing)
        return;

    currentSetNumber = (currentSetNumber % info.frames) + 1;
    updatePetSkin();
}

//===============================================================================================
// ZH: 狀態機 | EN: State machine
//===============================================================================================

void MainWindow::setState(MainWindow::State nextState)
{
    if (currentState == nextState)
        return;

    currentState = nextState;

    imageSwitchTimer->stop();
    currentSetNumber = 0;

    switch (currentState)
    {
    case Walking:
    {
        // ZH: 若皮膚為此狀態定義了序列幀，啟動切換計時器 | EN: Start frame timer if the skin defines frames for this state
        PetSkin::StateInfo info = skin.state("Walking");
        if (info.kind == PetSkin::StateInfo::Frames)
        {
            currentSetNumber = 1;
            imageSwitchTimer->start(info.interval);
        }
        break;
    }

    case Hovering:
        physics.initHover(this->y());
        break;

    case Flying:
        physics.initFly(getCurrentScreenRect(), this->width(), this->height());
        break;

    case Captured:
        physicsTimer->stop();
        break;

    default:
        break;
    }

    updatePetSkin();
}

//===============================================================================================
// ZH: 物理引擎（委派給 PetPhysics）| EN: Physics engine (delegates to PetPhysics)
//===============================================================================================

void MainWindow::updatePhysics()
{
    int posX = this->x();
    int posY = this->y();
    QRect screenRect = getCurrentScreenRect();

    switch (currentState)
    {
    case Standing:
        physics.applyGravity(posY);
        if (physics.resolveGroundCollision(posY, height(), screenRect) && sound)
            sound->playLand();
        this->move(posX, posY);
        break;

    case Walking:
        if (physics.isGrounded)
        {
            physics.updateWalkVelocity(walkSteps > 0);

            // ZH: 動畫播放速度隨移動速度動態調整 | EN: Animation speed tracks movement speed
            if (qAbs(physics.currentVelocityX) > 0.1)
            {
                int dynamicInterval = 300 - static_cast<int>(qAbs(physics.currentVelocityX) * 100);
                dynamicInterval = qBound(80, dynamicInterval, 350);
                if (qAbs(imageSwitchTimer->interval() - dynamicInterval) > 10)
                    imageSwitchTimer->setInterval(dynamicInterval);
            }

            posX += static_cast<int>(physics.currentVelocityX);
            if (physics.resolveBoundaryCollision(posX, width(), screenRect) && sound)
                sound->playWallHit();

            if (walkSteps > 0)
                walkSteps--;

            if (walkSteps == 0 && qAbs(physics.currentVelocityX) < 0.1)
            {
                physics.currentVelocityX = 0;
                setState(Standing);
            }
        }

        physics.applyGravity(posY);
        if (physics.resolveGroundCollision(posY, height(), screenRect) && sound)
            sound->playLand();
        this->move(posX, posY);
        break;

    case Flying:
    {
        auto step = physics.calcFlyStep(posX, posY);
        if (step.arrived)
        {
            setState(Hovering);
        }
        else
        {
            physics.currentVelocityX = step.newX - posX;   // ZH: 更新速度以驅動翻轉 | EN: Update velocity for flip
            this->move(step.newX, step.newY);
            updatePetSkin();
        }
        break;
    }

    case Hovering:
        this->move(posX, physics.calcHoverY());
        break;

    case Captured:
    case AI_Processing:
    default:
        break;
    }

#ifdef YACHIYO_HAS_LIVE2D
    // ZH: Live2D 模式：看向移動方向 (轉頭/眼神，靜止時回正面) | EN: Live2D: look toward movement direction (front when idle)
    if (m_live2dMode && m_live2d)
    {
        if (physics.currentVelocityX > 0.1)        m_live2d->setLookDirection(1.0f);
        else if (physics.currentVelocityX < -0.1)  m_live2d->setLookDirection(-1.0f);
        else                                       m_live2d->setLookDirection(0.0f);
    }
#endif
}

//===============================================================================================
// ZH: 行為決策（委派給 PetBehavior）| EN: Behavior decisions (delegates to PetBehavior)
//===============================================================================================

void MainWindow::decideNextAction()
{
    // ZH: AI 處理中不做任何行動決策，避免打斷生成/變身 | EN: No decisions during AI processing — don't interrupt generation/transform
    if (currentState == AI_Processing)
        return;

    if (currentState == Captured)
    {
        imageSwitchTimer->stop();
        currentSetNumber = 0;
        walkSteps = 0;
        physics.targetVelocityX = 0;
        return;
    }

    if (currentState == Hovering || currentState == Flying)
    {
        auto d = behavior.decideInAir();
        actionRoll = behavior.lastActionRoll;

        switch (d.action)
        {
        case PetBehavior::Decision::HoverStay:
            if (currentState != Hovering)
                setState(Hovering);
            break;
        case PetBehavior::Decision::HoverFly:
            setState(Standing);     // ZH: 先重置再進入 Flying | EN: Reset state before entering Flying
            setState(Flying);
            break;
        case PetBehavior::Decision::HoverLand:
            physics.velocityY  = 0;
            physics.isGrounded = false;
            setState(Standing);
            break;
        default:
            break;
        }
        return;
    }

    QRect screenRect  = getCurrentScreenRect();
    int usableWidth   = screenRect.width() - this->width();
    double posRatio   = static_cast<double>(this->x() - screenRect.left()) / usableWidth;

    auto d = behavior.decideOnGround(posRatio);
    actionRoll = behavior.lastActionRoll;

    switch (d.action)
    {
    case PetBehavior::Decision::Walk:
        physics.targetVelocityX = d.targetVelocityX;
        walkSteps = d.walkSteps;
        setState(Walking);
        break;
    case PetBehavior::Decision::Fly:
        setState(Flying);
        break;
    case PetBehavior::Decision::Stand:
        physics.targetVelocityX = 0;
        setState(Standing);
        break;
    default:
        break;
    }
}

//===============================================================================================
// ZH: AI 通訊（委派給 AIClient）| EN: AI communication (delegates to AIClient)
//===============================================================================================

void MainWindow::requestAIProcessing(const QString &prompt)
{
    if (currentState == AI_Processing || !aiClient || aiClient->isBusy())
        return;

    setState(AI_Processing);
    aiClient->sendRequest(ui->label->pixmap(Qt::ReturnByValue), prompt);
}

void MainWindow::onAIResultReady(QPixmap result)
{
    ui->label->setPixmap(result);
    lastAIError.clear();
    setState(Standing);
}

void MainWindow::onAIError(QString errorMsg)
{
    hideBusy();
    lastAIError = errorMsg;
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), lastAIError, this, QRect(), 3000);
    if (trayIcon)
        trayIcon->showMessage("YaChiYo", errorMsg, QSystemTrayIcon::Warning, 4000);
    pendingSkinPaths.clear();
    setState(Standing);
}

//===============================================================================================
// ZH: AI 生成皮膚 | EN: AI skin generation
//===============================================================================================

// ZH: 收集當前皮膚每個狀態的圖檔 (相對路徑 + 影像，兩串列平行對應)
// EN: Collect every image of the current skin (parallel lists of relative path + image)
void MainWindow::collectCurrentSkinFrames(QStringList &relPaths, QList<QImage> &images) const
{
    const QStringList states = {"Standing", "Walking", "Flying", "Hovering", "Captured", "AI_Processing"};
    for (const QString &st : states)
    {
        PetSkin::StateInfo info = skin.state(st);
        if (info.kind == PetSkin::StateInfo::Frames)
        {
            for (int i = 1; i <= info.frames; ++i)
            {
                QString path = skin.framePath(st, i);
                if (path.isEmpty()) continue;
                QImage img(path);
                if (img.isNull()) continue;
                relPaths << QString("%1/%1-%2.png").arg(st).arg(i);
                images   << img;
            }
        }
        else
        {
            QString path = skin.pngPath(st);     // ZH: gif 型也用其 png 靜圖 | EN: gif states use their png still
            if (path.isEmpty()) continue;
            QImage img(path);
            if (img.isNull()) continue;
            relPaths << (st + ".png");
            images   << img;
        }
    }
}

void MainWindow::requestSkinGeneration()
{
    if (currentState == AI_Processing || !aiClient || aiClient->isBusy())
        return;

    // ZH: 1. 讓使用者上傳一張角色參考圖 | EN: 1. let the user pick a character reference image
    const QString file = QFileDialog::getOpenFileName(
        this, "選擇角色參考圖 (AI 生成皮膚)", QString(),
        "圖片 Images (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (file.isEmpty())
        return;     // ZH: 使用者取消 | EN: cancelled

    QImage reference(file);
    if (reference.isNull())
    {
        onAIError("AI Error: 無法讀取參考圖 (Cannot load reference image)");
        return;
    }

    // ZH: 2. 收集當前皮膚的姿勢幀 (作為 ControlNet 骨架) | EN: 2. collect current skin frames as pose sources
    QStringList relPaths;
    QList<QImage> images;
    collectCurrentSkinFrames(relPaths, images);
    if (images.isEmpty())
    {
        onAIError("AI Error: no frames to generate from");
        return;
    }

    // ZH: 3. 送出 (參考圖 + 姿勢幀) | EN: 3. send (reference + pose frames)
    pendingSkinPaths = relPaths;
    setState(AI_Processing);
    showBusy(QString("AI 生成皮膚中…（%1 張，約需 30–60 秒）\n請保持 AI 後端運行").arg(images.size()));
    aiClient->generateSkin(reference, images, aiPrompt);
}

void MainWindow::showBusy(const QString &text)
{
    if (!busyDialog)
    {
        busyDialog = new QProgressDialog(this);
        busyDialog->setWindowTitle("YaChiYo");
        busyDialog->setCancelButton(nullptr);        // ZH: 不提供取消 (請求無法中途中斷) | EN: no cancel (request can't be aborted mid-way)
        busyDialog->setRange(0, 0);                  // ZH: 不確定進度 (忙碌動畫) | EN: indeterminate busy bar
        busyDialog->setMinimumDuration(0);
        busyDialog->setWindowModality(Qt::NonModal);
        busyDialog->setWindowFlags((busyDialog->windowFlags() | Qt::WindowStaysOnTopHint)
                                   & ~Qt::WindowCloseButtonHint);
    }
    busyDialog->setLabelText(text);
    busyDialog->show();
}

void MainWindow::hideBusy()
{
    if (busyDialog)
        busyDialog->hide();
}

void MainWindow::onSkinReady(QList<QImage> results)
{
    if (results.size() != pendingSkinPaths.size() || results.isEmpty())
    {
        onAIError("AI Error: skin frame count mismatch");
        return;
    }

    // ZH: 新皮膚寫入執行檔同層的 skins/ (PetSkin::available 會掃到)
    // EN: Write the new skin under <exe>/skins/ (picked up by PetSkin::available)
    const QString id   = "ai_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    const QString base = QCoreApplication::applicationDirPath() + "/skins/" + id;

    QDir().mkpath(base);
    for (int i = 0; i < results.size(); ++i)
    {
        const QString dest = base + "/" + pendingSkinPaths[i];
        QDir().mkpath(QFileInfo(dest).absolutePath());   // ZH: 確保子目錄存在 (如 Walking/) | EN: ensure subdir exists
        results[i].save(dest, "PNG");
    }

    // ZH: 沿用當前皮膚的 skin.json，只改名稱寫入新資料夾
    // EN: Reuse the current skin.json, only rename, write into the new folder
    QJsonObject root;
    QFile srcJson(skin.dir() + "/skin.json");
    if (srcJson.open(QIODevice::ReadOnly))
    {
        root = QJsonDocument::fromJson(srcJson.readAll()).object();
        srcJson.close();
    }
    root["name"] = "AI: " + id;
    QFile outJson(base + "/skin.json");
    if (outJson.open(QIODevice::WriteOnly))
    {
        outJson.write(QJsonDocument(root).toJson());
        outJson.close();
    }

    pendingSkinPaths.clear();
    lastAIError.clear();
    setState(Standing);
    setSkin(id);            // ZH: 立即套用新皮膚並持久化 | EN: apply the new skin immediately and persist

    hideBusy();
    if (trayIcon)
        trayIcon->showMessage("YaChiYo", "皮膚生成完成，已套用！", QSystemTrayIcon::Information, 4000);
}

//===============================================================================================
// ZH: 系統托盤 | EN: System tray
//===============================================================================================

void MainWindow::initTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayMenu = new QMenu(this);

    trayIcon->setIcon(QIcon(":/res/icons/app.png"));
    trayIcon->setToolTip("YaChiYo Desktop Pet");

    QAction *showAction = trayMenu->addAction("顯示桌寵 (Show Pet)");
    connect(showAction, &QAction::triggered, this, &MainWindow::show);

    QAction *settingsAction = trayMenu->addAction("設定中心 (Settings)");
    connect(settingsAction, &QAction::triggered, this, [this]()
    {
        settingsCenter->showWindow();
    });

    // ZH: 重啟桌寵 (套用「重啟生效」的設定，如 Live2D 模式) | EN: restart pet (apply restart-only settings like Live2D mode)
    QAction *restartAction = trayMenu->addAction("重啟桌寵 (Restart)");
    connect(restartAction, &QAction::triggered, this, []()
    {
        QStringList args = QCoreApplication::arguments();
        if (!args.isEmpty()) args.removeFirst();   // ZH: 去掉程式路徑 | EN: drop argv[0]
        QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
        QCoreApplication::quit();
    });

    trayMenu->addSeparator();

    QAction *quitAction = trayMenu->addAction("退出程式 (Quit)");
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::DoubleClick)
            this->show();
    });
}

//===============================================================================================
// ZH: Q_PROPERTY Getter 實作 | EN: Q_PROPERTY getter implementations
//===============================================================================================

MainWindow::State MainWindow::getCurrentState() const { return currentState; }
double  MainWindow::getVelX()          const { return physics.currentVelocityX; }
double  MainWindow::getTargetVelX()    const { return physics.targetVelocityX; }
int     MainWindow::getSteps()         const { return walkSteps; }
int     MainWindow::getActionRoll()    const { return actionRoll; }
int     MainWindow::getCurrentSetNumber() const { return currentSetNumber; }
QString MainWindow::getLastAIError()   const { return lastAIError; }

double MainWindow::getDecisionTimerRemaining() const
{
    if (behaviorTimer && behaviorTimer->isActive())
        return behaviorTimer->remainingTime() / 1000.0;
    return 0.0;
}

double MainWindow::getImageSwitchTimerRemaining() const
{
    if (imageSwitchTimer && imageSwitchTimer->isActive())
        return imageSwitchTimer->remainingTime() / 1000.0;
    return 0.0;
}

QRect MainWindow::getCurrentScreenRect() const
{
    QScreen *screen = QGuiApplication::screenAt(this->geometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    return screen->availableGeometry();
}

//===============================================================================================
// ZH: 設定持久化 | EN: Settings persistence
//===============================================================================================

void MainWindow::applySettings(const PetSettingsData &s)
{
    behavior.walkSpeed = s.walkSpeed;
    behaviorInterval   = s.behaviorInterval;
    petScale           = s.petScale;
    physics.gravity    = s.gravity;
    petSkinType        = s.gifSkin ? 1 : 0;
    aiPrompt           = s.aiPrompt;
    m_movementEnabled  = s.movementEnabled;
    m_live2dBaseW      = s.live2dWidth;
    // ZH: alwaysOnTop 由建構子直接套用至視窗旗標 | EN: alwaysOnTop is applied to window flags by the constructor
}

void MainWindow::saveSettings() const
{
    // ZH: 先載入既有設定，只覆寫「執行期可變」欄位 → 保留重啟生效欄位 (live2dMode/live2dModelDir)
    // EN: load existing first, override only runtime-mutable fields → preserves restart-applied fields
    PetSettingsData s = PetSettings::load();
    s.walkSpeed        = behavior.walkSpeed;
    s.behaviorInterval = behaviorInterval;
    s.petScale         = petScale;
    s.gravity          = physics.gravity;
    s.gifSkin          = (petSkinType == 1);
    s.alwaysOnTop      = windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    s.aiPrompt         = aiPrompt;
    s.currentSkin      = currentSkinId;
    s.movementEnabled  = m_movementEnabled;
    s.live2dWidth      = m_live2dBaseW;
    PetSettings::save(s);
}

//===============================================================================================
// ZH: 設定中心 Setter 實作（每次變更後即時存檔）| EN: Settings Center setters (persist on change)
//===============================================================================================

void MainWindow::setWalkSpeed(double speed)   { behavior.walkSpeed = speed; saveSettings(); }
void MainWindow::setGravity(double g)         { physics.gravity = g;        saveSettings(); }
void MainWindow::setAiPrompt(const QString &prompt) { aiPrompt = prompt;    saveSettings(); }

void MainWindow::setBehaviorInterval(int ms)
{
    behaviorInterval = ms;
    if (behaviorTimer && behaviorTimer->isActive())
        behaviorTimer->start(ms);
    saveSettings();
}

void MainWindow::setPetScale(double scale)
{
    petScale = scale;
    updatePetSkin();
    applyLive2DSize();   // ZH: Live2D 模式同步縮放視窗 | EN: also rescale the Live2D window
    saveSettings();
}

void MainWindow::applyLive2DSize()
{
    if (!m_live2dMode)
        return;
    // ZH: 基準寬度(可調) × petScale；高度依固定比例 | EN: base width (adjustable) × petScale; height keeps ratio
    const int baseH = 440;
    resize(static_cast<int>(m_live2dBaseW * petScale), static_cast<int>(baseH * petScale));
}

void MainWindow::setMovementEnabled(bool on)
{
    m_movementEnabled = on;
    if (on)
    {
        if (behaviorTimer) behaviorTimer->start(behaviorInterval);
    }
    else
    {
        // ZH: 關閉移動：停止決策、回到站立(仍受重力/可拖曳) | EN: off: stop decisions, return to standing (gravity/drag still work)
        if (behaviorTimer) behaviorTimer->stop();
        walkSteps = 0;
        physics.targetVelocityX = 0;
        if (currentState == Walking || currentState == Flying || currentState == Hovering)
            setState(Standing);
    }
    saveSettings();
}

void MainWindow::setLive2dWidth(int w)
{
    m_live2dBaseW = w;
    applyLive2DSize();
    saveSettings();
}

void MainWindow::setLive2dModeSetting(bool on)
{
    // ZH: 重啟生效，僅寫入設定 (不改變目前執行期模式) | EN: restart-applied; only writes the setting
    PetSettingsData s = PetSettings::load();
    s.live2dMode = on;
    PetSettings::save(s);
}

void MainWindow::setPetSkinType(int type)
{
    petSkinType = type;
    updatePetSkin();
    saveSettings();
}

void MainWindow::loadSkinById(const QString &id)
{
    // ZH: 從可用清單找出對應路徑，找不到則退回內建 default | EN: Resolve path from the list, fall back to built-in default
    QString dir = ":/res/skins/default";
    QString resolvedId = "default";
    for (const PetSkin::SkinEntry &e : PetSkin::available())
    {
        if (e.id == id)
        {
            dir = e.dir;
            resolvedId = e.id;
            break;
        }
    }
    skin.load(dir);
    currentSkinId = resolvedId;
}

void MainWindow::setSkin(const QString &id)
{
    loadSkinById(id);
    currentSetNumber = 0;       // ZH: 重置動畫幀，避免沿用舊皮膚的幀號 | EN: Reset frame to avoid stale index from old skin
    updatePetSkin();
    saveSettings();
}

void MainWindow::setAlwaysOnTop(bool onTop)
{
    Qt::WindowFlags flags = windowFlags();
    if (onTop)
        flags |= Qt::WindowStaysOnTopHint;
    else
        flags &= ~Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    show();
    saveSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}
