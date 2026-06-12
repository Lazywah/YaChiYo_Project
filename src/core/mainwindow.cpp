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
#include <QFile>

//===============================================================================================

//===============================================================================================

MainWindow::MainWindow(const PetConfig &config, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , config(config)
{
    ui->setupUi(this);
    settingsCenter = new SettingsCenter(this, this);

    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::FramelessWindowHint;
    flags |= Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground);

    initAnimationConfigs();
    initAllConnect();
    initTrayIcon();
    setState(Standing);
}

//===============================================================================================

//===============================================================================================

void MainWindow::initAnimationConfigs()
{
    animConfigs[Walking] = {6, 150};
}

void MainWindow::initAllConnect()
{
    physicsTimer = new QTimer(this);
    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::updatePhysics);
    if (config.physicsEnabled)
        physicsTimer->start(16);    // ZH: 約 60 FPS | EN: ~60 FPS

    behaviorTimer = new QTimer(this);
    connect(behaviorTimer, &QTimer::timeout, this, &MainWindow::decideNextAction);
    if (config.behaviorEnabled)
        behaviorTimer->start(behaviorInterval);

    imageSwitchTimer = new QTimer(this);
    connect(imageSwitchTimer, &QTimer::timeout, this, &MainWindow::turnImageSet);

    if (config.aiEnabled)
    {
        aiClient = new AIClient(this);
        connect(aiClient, &AIClient::resultReady,    this, &MainWindow::onAIResultReady);
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
        m_offset = event->globalPosition().toPoint() - this->pos();
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        move(event->globalPosition().toPoint() - m_offset);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        QRect screenRect = getCurrentScreenRect();
        int groundY = screenRect.bottom() - this->height();

        if (this->y() < groundY - 10)
            setState(Hovering);
        else
            setState(Standing);

        if (sound) sound->playRelease();
        physicsTimer->start(16);
    }
}

//===============================================================================================
// ZH: 外觀 & 動畫 | EN: Appearance & animation
//===============================================================================================

void MainWindow::updatePetSkin()
{
    QMetaEnum metaEnum = QMetaEnum::fromType<MainWindow::State>();
    QString stateName  = metaEnum.valueToKey(currentState);

    if (currentMovie)
    {
        currentMovie->stop();
        currentMovie->deleteLater();
        currentMovie = nullptr;
    }

    int scaledSize = static_cast<int>(250 * petScale);

    // ZH: GIF 模式開啟且該狀態有對應 GIF 才播放動圖，否則退回 PNG | EN: Play GIF only when GIF mode is on and the file exists, otherwise fall back to PNG
    QString gifPath = imagePath + stateName + ".gif";
    if (petSkinType == 1 && QFile::exists(gifPath))
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

    QPixmap pix;

    if (animConfigs.contains(currentState) && currentSetNumber > 0)
    {
        QString path = QString("%1%2/%2-%3.png")
                           .arg(testImageSetPath)
                           .arg(stateName)
                           .arg(currentSetNumber);
        pix.load(path);
    }
    else
    {
        pix.load(imagePath + stateName + ".png");
    }

    if (pix.isNull())
        pix.load(imagePath + "Standing.png");

    if (qAbs(physics.currentVelocityX) > 0.1 && physics.currentVelocityX < -0.1)
        pix = QPixmap::fromImage(pix.toImage().flipped(Qt::Horizontal));

    ui->label->setPixmap(pix.scaled(scaledSize, scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->label->adjustSize();
    this->adjustSize();
}

void MainWindow::turnImageSet()
{
    if (!animConfigs.contains(currentState) || currentState == AI_Processing)
        return;

    int total = animConfigs[currentState].totalFrames;
    currentSetNumber = (currentSetNumber % total) + 1;
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
        if (animConfigs.contains(currentState))
        {
            currentSetNumber = 1;
            imageSwitchTimer->start(animConfigs[currentState].intervalMs);
        }
        break;

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
}

//===============================================================================================
// ZH: 行為決策（委派給 PetBehavior）| EN: Behavior decisions (delegates to PetBehavior)
//===============================================================================================

void MainWindow::decideNextAction()
{
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
    lastAIError = errorMsg;
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), lastAIError, this, QRect(), 3000);
    setState(Standing);
}

//===============================================================================================
// ZH: 系統托盤 | EN: System tray
//===============================================================================================

void MainWindow::initTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayMenu = new QMenu(this);

    trayIcon->setIcon(QIcon(imagePath + "Standing.png"));
    trayIcon->setToolTip("YaChiYo Desktop Pet");

    QAction *showAction = trayMenu->addAction("顯示桌寵 (Show Pet)");
    connect(showAction, &QAction::triggered, this, &MainWindow::show);

    QAction *settingsAction = trayMenu->addAction("設定中心 (Settings)");
    connect(settingsAction, &QAction::triggered, this, [this]()
    {
        settingsCenter->showWindow();
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
// ZH: 設定中心 Setter 實作 | EN: Settings Center setter implementations
//===============================================================================================

void MainWindow::setWalkSpeed(double speed)   { behavior.walkSpeed = speed; }
void MainWindow::setGravity(double g)         { physics.gravity = g; }
void MainWindow::setAiPrompt(const QString &prompt) { aiPrompt = prompt; }

void MainWindow::setBehaviorInterval(int ms)
{
    behaviorInterval = ms;
    if (behaviorTimer && behaviorTimer->isActive())
        behaviorTimer->start(ms);
}

void MainWindow::setPetScale(double scale)
{
    petScale = scale;
    updatePetSkin();
}

void MainWindow::setPetSkinType(int type)
{
    petSkinType = type;
    updatePetSkin();
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
}

MainWindow::~MainWindow()
{
    delete ui;
}
