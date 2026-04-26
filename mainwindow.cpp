#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QMetaEnum>            // ZH: enum 型態轉換 | EN: enum type conversion
#include <QMenu>                // ZH: 右鍵選單容器 | EN: Right-clock menu container
#include <QAction>              // ZH: 選單動作項目 | EN: Menu actions
#include <QContextMenuEvent>    // ZH: 右鍵點擊事件 | EN: Right-click event
#include <QPixmap>              // ZH: 處理靜態圖 | EN: Processing static images
#include <QMovie>               // ZH: 處理動態圖 | EN: Processing dynamic images
#include <QScreen>              // ZH: 獲取螢幕資訊 | EN: Get screen information
#include <QGuiApplication>      // ZH: 監聽螢幕變化 | EN: Listen to screen changes
#include <QTimer>               // ZH: 計時器 | EN: Timer
#include <QRandomGenerator>     // ZH: 隨機數 | EN: Random numbers
#include <QToolTip>             // ZH: 提示框 | EN: Tooltip
#include <QtMath>               // ZH: 數學函數 (qSin) | EN: Math functions (qSin)
#include <QFile>                // ZH: 檔案操作 | EN: File operations

//===============================================================================================

//===============================================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    settingsCenter = new SettingsCenter(this, this);  // ZH: 將設定中心設置為該視窗的子視窗 | EN: Set the settings center as a child window of this window

    // --> setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::FramelessWindowHint;   // ZH: 消除視窗標題與邊框 | EN: Remove window title and border
    flags |= Qt::WindowStaysOnTopHint;  // ZH: 使視窗永遠置於頂層 | EN: Make window always stay on top
    setWindowFlags(flags);

    setAttribute(Qt::WA_TranslucentBackground); // ZH: 將視窗背景設為透明 | EN: Set window background to transparent

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
    physicsTimer = new QTimer(this);                                                // ZH: 初始化物理引擎計時器 | EN: Initialize physics engine timer
    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::updatePhysics);      // ZH: 串接上發送者、訊號、接收者、需運行函數 | EN: Connect the sender, signal, receiver, and function to be executed
    physicsTimer->start(16);                                                        // ZH: 啟用物理引擎計時器，更新頻率 16ms(約 60FPS) | EN: Enable physics engine timer, updating at a frequency of 16ms(approximately 60 FPS)

    behaviorTimer = new QTimer(this);                                               // ZH: 初始化行動決策計時器 | EN: Initialize action decision timer
    connect(behaviorTimer, &QTimer::timeout, this, &MainWindow::decideNextAction);
    behaviorTimer->start(behaviorInterval);                                          // ZH: 啟用行動決策計時器 | EN: Enable action decision timer

    imageSwitchTimer = new QTimer(this);                                            // ZH: 初始化圖像集切換計時器 | EN: Initialize image set switching timer
    connect(imageSwitchTimer, &QTimer::timeout, this, &MainWindow::turnImageSet);

    networkManager = new QNetworkAccessManager(this);                               // ZH: 初始化 AI 通訊網管 | EN: Initialize AI communication network management
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onAIResultReceived);
}

// ZH: 宣告右鍵選單中的事件 | EN: Declare event in right-click menu
void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // ZH: 設定中心 | EN: Settings center
    QAction *settingsAction = menu.addAction("設定中心");
    connect(settingsAction, &QAction::triggered, this, [this]()
    {
        settingsCenter->showWindow();
    });

    menu.addSeparator();    // ZH: 分隔線 | EN: Separator

    // ZH: AI 變身 | EN: AI Transform
    QAction *aiAction = menu.addAction("AI 變身");
    connect(aiAction, &QAction::triggered, this, [this]()
    {
        requestAIProcessing(aiPrompt);
    });

    menu.addSeparator();    // ZH: 分隔線 | EN: Separator

    // ZH: 關閉/隱藏桌寵 | EN: Hide desktop pet
    QAction *closeAction = menu.addAction("關閉/隱藏桌寵");
    connect(closeAction, &QAction::triggered, this, &MainWindow::hide);

    menu.exec(event->globalPos());
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        setState(Captured);

        // ZH: 滑鼠拖曳功能(紀錄初始點) | EN: Mouse drag function (record initial point)
        m_offset = event->globalPosition().toPoint() - this->pos(); // ZH: 紀錄滑鼠點擊位置與視窗左上角的偏差值 | EN: record offset mouse click position to window Top Left
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        // ZH: 移動視窗到目前滑鼠位置-偏差值 (絕對位置) | EN: move window to current mouse position - offset (absolute position)
        move(event->globalPosition().toPoint() - m_offset);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // ZH: 釋放滑鼠時判斷是否在空中，若在空中則進入浮空狀態 | EN: Check if in air when mouse released, enter Hovering if airborne
        QRect screenRect = getCurrentScreenRect();
        int groundY = screenRect.bottom() - this->height();

        if (this->y() < groundY - 10)   // ZH: 距離地面超過 10px 則視為在空中 | EN: More than 10px above ground = airborne
        {
            setState(Hovering);
        }
        else
        {
            setState(Standing);
        }

        physicsTimer->start(16);    // ZH: 啟用物理引擎計時器，更新頻率 16ms(約 60FPS) | EN: Enable physics engine timer, updating at a frequency of 16ms(approximately 60 FPS)
    }
}

void MainWindow::updatePetSkin()
{
    // ZH: 自動獲取當前狀態的字串 | EN: Auto retrieve the string of the current state
    QMetaEnum metaEnum = QMetaEnum::fromType<MainWindow::State>();
    QString stateName = metaEnum.valueToKey(currentState);
    
    // ZH: 停止並清除舊動畫 | EN: Stop and clear old animation
    if (currentMovie)
    {
        currentMovie->stop();
        currentMovie->deleteLater();
        currentMovie = nullptr;
    }

    int scaledSize = static_cast<int>(250 * petScale);

    // ZH: 檢查是否有對應的 GIF 動畫 (或當設定強制使用 GIF 皮膚) | EN: Check for GIF animation or forced GIF skin type
    QString gifPath = imagePath + stateName + ".gif";
    if (petSkinType == 1 || QFile::exists(gifPath))
    {
        if (QFile::exists(gifPath))
        {
            currentMovie = new QMovie(gifPath);
            
            // ZH: 讀取第一幀以取得原始比例，避免變形 | EN: Read first frame to get original ratio, avoid deformation
            QPixmap tempPix(gifPath);
            if (!tempPix.isNull()) {
                QSize newSize = tempPix.size().scaled(scaledSize, scaledSize, Qt::KeepAspectRatio);
                currentMovie->setScaledSize(newSize);
            } else {
                currentMovie->setScaledSize(QSize(scaledSize, scaledSize));
            }
            ui->label->setMovie(currentMovie);
            currentMovie->start();
            
            ui->label->adjustSize();
            this->adjustSize();
            return;
        }
    }

    QPixmap pix;

    // ZH: 判斷是否要讀取序列幀 | EN: Determine whether to read the sequence frame
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

    // ZH: 若找不到該狀態對應的圖片(如 Hovering)，預設退回 Standing 避免消失 | EN: Fallback to Standing if image not found (e.g. Hovering)
    if (pix.isNull())
    {
        pix.load(imagePath + "Standing.png");
    }

    // ZH: 處理水平翻轉 | EN: Handling horizontal flipping
    if (qAbs(currentVelocityX) > 0.1)
    {
        if (currentVelocityX < -0.1)
        {
            pix = QPixmap::fromImage(pix.toImage().flipped(Qt::Horizontal));    // ZH: 使用 Qt 6 規範寫法 | EN: Using Qt 6 standard syntax
        }
    }

    // ZH: 使用 petScale 動態計算圖片大小 | EN: Use petScale to dynamically calculate image size
    ui->label->setPixmap(pix.scaled(scaledSize, scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->label->adjustSize();
    this->adjustSize();
}

void MainWindow::setState(MainWindow::State nextState)
{
    if (currentState == nextState)
        return;

    currentState = nextState;

    // ZH: 重置動畫狀態 | EN: Reset animation state
    imageSwitchTimer->stop();
    currentSetNumber = 0;

    switch (currentState)
    {
    case Walking:
        // ZH: 若新狀態有動畫則啟動計時器 | EN: If the new state has an animation, start the timer.
        if (animConfigs.contains(currentState))
        {
            currentSetNumber = 1;
            imageSwitchTimer->start(animConfigs[currentState].intervalMs);
        }
        break;

    case Hovering:
        // ZH: 記錄進入浮空時的 Y 座標作為基準，重置相位 | EN: Record Y position as base when entering Hovering, reset phase
        hoverBaseY = this->y();
        hoverPhase = 0.0;
        velocityY = 0;
        currentVelocityX = 0;
        isGrounded = false;
        break;

    case Flying:
    {
        // ZH: 隨機選擇螢幕上的飛行目標位置 | EN: Randomly select a flying target position on screen
        QRect screenRect = getCurrentScreenRect();
        flyTargetX = QRandomGenerator::global()->bounded(screenRect.left(), screenRect.right() - this->width());
        flyTargetY = QRandomGenerator::global()->bounded(screenRect.top() + 50, screenRect.bottom() - this->height() - 100);
        velocityY = 0;
        isGrounded = false;
        break;
    }

    case Captured:
        physicsTimer->stop();
        break;

    default:
        break;
    }

    updatePetSkin();
}

void MainWindow::updatePhysics()
{
    switch (currentState)
    {
    case Standing:
        // ZH: 啟用重力使桌寵落地 | EN: Use gravity to make desktop pet land
        applyGravity();             // ZH: 計算重力加速度 | EN: Calculate gravitational acceleration
        checkGroundCollision();     // ZH: 落地偵測 | EN: Landing detection
        break;

    case Walking:
        // ZH: 水平移動邏輯 | EN: Horizontal movement logic
        if (isGrounded)
        {
            // ZH: 行動加速度計算 | EN: Action accelerometer
            if (walkSteps > 0)
            {
                // ZH: 朝目標速度加速 | EN: Accelerate towards the target
                if (currentVelocityX < targetVelocityX)
                    currentVelocityX = qMin(targetVelocityX, currentVelocityX + acceleration);
                else if (currentVelocityX > targetVelocityX)
                    currentVelocityX = qMax(targetVelocityX, currentVelocityX - acceleration);
            }
            else
            {
                // ZH: 受摩擦力影響減速 | EN: Deceleration due to friction
                if (currentVelocityX > 0)
                    currentVelocityX = qMax(0.0, currentVelocityX - friction);
                else if (currentVelocityX < 0)
                    currentVelocityX = qMin(0.0, currentVelocityX + friction);
            }

            // ZH: 動畫切換的頻率隨著移動速度改變(速度越快，動畫播越快) | EN: The animation switching frequency changes with movement speed(the faster the speed, the faster the animation plays)
            if (qAbs(currentVelocityX) > 0.1)
            {
                int dynamicInterval = 300 - (static_cast<int>(qAbs(currentVelocityX) * 100));
                dynamicInterval = qBound(80, dynamicInterval, 350);
                if (qAbs(imageSwitchTimer->interval() - dynamicInterval) > dynamicInterval)
                {
                    imageSwitchTimer->setInterval(dynamicInterval);
                }
            }

            this->move(this->x() + (int)currentVelocityX, this->y());
            checkBoundaryCollision();

            if (walkSteps > 0)
                walkSteps--;

            if (walkSteps == 0 && qAbs(currentVelocityX) < 0.1)
            {
                currentVelocityX = 0;
                setState(Standing);
            }
        }

        // ZH: 啟用重力使桌寵落地 | EN: Use gravity to make desktop pet land
        applyGravity();             // ZH: 計算重力加速度 | EN: Calculate gravitational acceleration
        checkGroundCollision();     // ZH: 落地偵測 | EN: Landing detection
        break;

    case Flying:
    {
        // ZH: 朝目標位置緩慢飛行 | EN: Fly slowly towards the target position
        double dx = flyTargetX - this->x();
        double dy = flyTargetY - this->y();
        double distance = qSqrt(dx * dx + dy * dy);

        if (distance < 5.0)
        {
            // ZH: 到達目標後轉為浮空 | EN: Switch to Hovering after reaching target
            setState(Hovering);
        }
        else
        {
            // ZH: 歸一化方向向量並以 flySpeed 移動 | EN: Normalize direction vector and move at flySpeed
            double moveX = (dx / distance) * flySpeed;
            double moveY = (dy / distance) * flySpeed;
            this->move(this->x() + static_cast<int>(moveX), this->y() + static_cast<int>(moveY));

            // ZH: 更新水平速度以驅動翻轉運算 | EN: Update horizontal velocity to drive flip calculation
            currentVelocityX = moveX;
            updatePetSkin();
        }
        break;
    }

    case Hovering:
    {
        // ZH: Sin Wave 上下浮動效果 | EN: Sin Wave up-down floating effect
        hoverPhase += hoverSpeed;
        if (hoverPhase > 2 * M_PI)  // ZH: 相位回歸避免溢出 | EN: Phase wrap-around to avoid overflow
            hoverPhase -= 2 * M_PI;

        int newY = hoverBaseY + static_cast<int>(qSin(hoverPhase) * hoverAmplitude);
        this->move(this->x(), newY);
        break;
    }

    case Captured:
        break;

    case AI_Processing:
        break;

    default:
        break;
    }
}

void MainWindow::applyGravity()
{
    // ZH: 未落地則加速 | EN: Accelerate before landing
    if (!isGrounded)
    {
        velocityY += gravity;
    }

    // ID: feat-3
    // ZH: 未來可新增 X 軸慣性 | EN: Future additions could include X-axis inertia.
    // ZH: 不移動 X 座標，僅變更 Y 座標 | EN: Without moving the X coordinate, only change the Y coordinate
    this->move(this->x(), this->y() + (int)velocityY);
}

void MainWindow::checkGroundCollision()
{
    // ZH: 獲取目前的螢幕可用區域 | EN: Get the current screen available area
    QRect screenRect = getCurrentScreenRect();
    // ZH: 計算地面的 Y 座標(螢幕底部-視窗高度) | EN: Calculate the Y coordinates of the ground (bottom of screen - viewport height)
    int groundY = screenRect.bottom() - this->height();

    // ZH: 碰撞檢測邏輯 | EN: Collision detection logic
    if (this->y() >= groundY)   // ZH: 已經到達或超過地面 | EN: Have reached or exceeded the ground
    {
        if (qAbs(velocityY) > 1.5)  // ZH: 回彈功能 | EN: Rebound function
        {
            velocityY *= bounceFactor;          // ZH: 速度反轉並減半 | EN: Speed ​​reversed and halved
            this->move(this->x(), groundY - 1); // ZH: 稍微往上移一些，避免卡在地板裡 | EN: Move it up slightly to avoid it getting stuck in the floor
        }
        else
        {
            // ZH: 速度太小時進入靜止狀態 | EN: When the speed is too low, it comes to a standstill
            this->move(this->x(), groundY); // ZH: 強制對齊地板 | EN: Forced Alignment Floor
            velocityY = 0;                  // ZH: 速度歸零 | EN: Speed ​​to zero
            isGrounded = true;              // ZH: 標記為落地狀態 | EN: Marked as landed
        }
    }
    else    // ZH: 未到達地面 | EN: Not reached the ground
    {
        isGrounded = false;
    }
}

void MainWindow::checkBoundaryCollision()
{
    // ZH: 獲取螢幕可用區域 | EN: Get screen available area
    QRect screenRect = getCurrentScreenRect();

    // ZH: 獲取桌寵目前的 X 座標邊界 | EN: Get the current X coordinate boundaries of the desktop pet
    int leftWall = screenRect.left();
    int rightWall = screenRect.right() - this->width();

    // ZH: 檢查左右邊界 | EN: Check left and right boundaries
    if (this->x() <= leftWall || this->x() >= rightWall)
    {
        this->move(qBound(leftWall, this->x(), rightWall), this->y());

        currentVelocityX *= wallBounceFactor;
        targetVelocityX *= wallBounceFactor;
    }
}

void MainWindow::turnImageSet()
{
    if (!animConfigs.contains(currentState))
        return;

    if (currentState == AI_Processing)
        return;

    int total = animConfigs[currentState].totalFrames;
    currentSetNumber = (currentSetNumber % total) + 1;

    updatePetSkin();
}

void MainWindow::decideNextAction()
{
    // ZH: 若被抓取中，強制停止所有行動與動畫 | EN: If captured, all actions and animations will be forcibly stopped.
    if (currentState == Captured)
    {
        imageSwitchTimer->stop();   // ZH: 暫停圖像集切換計時器 | EN: Disable image set switching timer
        currentSetNumber = 0;
        walkSteps = 0;              // ZH: 清空行動步數 | EN: Clear action steps
        targetVelocityX = 0;
        return;
    }

    // ZH: 若正在浮空或飛行中，使用不同的決策邏輯 | EN: Use different decision logic when hovering or flying
    if (currentState == Hovering || currentState == Flying)
    {
        actionRoll = QRandomGenerator::global()->bounded(100);

        if (actionRoll < 50)        // ZH: 50% 繼續浮空 | EN: 50% continue hovering
        {
            if (currentState != Hovering)
                setState(Hovering);
        }
        else if (actionRoll < 80)   // ZH: 30% 飛去新位置 | EN: 30% fly to new position
        {
            setState(Standing);     // ZH: 先重置狀態再進入 Flying | EN: Reset state first then enter Flying
            setState(Flying);
        }
        else                        // ZH: 20% 落地 | EN: 20% land
        {
            velocityY = 0;
            isGrounded = false;
            setState(Standing);     // ZH: 交由重力使其落地 | EN: Let gravity bring it down
        }
        return;
    }

    QRect screenRect = getCurrentScreenRect();
    int usableWidth = screenRect.width() - this->width();
    // ZH: 計算目前的 X 座標相對於螢幕可用區域的比例(0.0 為最左，1.0 為最右) | EN: Calculate the current X coordinate relative to the available screen area (0.0 is the leftmost, 1.0 is the rightmost)
    double positionRatio = static_cast<double>(this->x() - screenRect.left()) / usableWidth;

    actionRoll = QRandomGenerator::global()->bounded(100);    // ZH: 決定下一動作(0~99) | EN: Decide on the next action(0~99)

    if (actionRoll < 50)  // ZH: 50% 開始移動(散步) | EN: 50% started moving(walking)
    {
        double rightProb = 1.0 - positionRatio; // ZH: 往右的機率隨位置線性調整 | EN: The probability of going right is linearly adjusted with position
        double dirRoll = QRandomGenerator::global()->generateDouble(); // ZH: 生成 0.0 ~ 1.0 的隨機數 | EN: Generate random numbers between 0.0 and 1.0

        int direction;
        if (dirRoll < rightProb)
            direction = 1;
        else
            direction = -1;

        // ZH: 設定移動參數 | EN: Set movement parameters
        targetVelocityX = direction * walkSpeed;
        walkSteps = QRandomGenerator::global()->bounded(120) + 90; // ZH: 隨機步數 90~210 | EN: Random number of steps: 90~210

        setState(Walking);
    }
    else if (actionRoll < 60)   // ZH: 10% 起飛 | EN: 10% take off and fly
    {
        setState(Flying);
    }
    else    // ZH: 40% 原地站定 | EN: 40% standing
    {
        targetVelocityX = 0;
        setState(Standing);
    }
}

void MainWindow::requestAIProcessing(const QString &prompt)
{
    if (currentState == AI_Processing)
        return;

    setState(AI_Processing);

    // ZH: 擷取目前 Label 畫面 | EN: Capture the current Label screen
    QPixmap pix = ui->label->pixmap(Qt::ReturnByValue);
    QByteArray ba;
    QBuffer buffer(&ba);
    pix.save(&buffer, "PNG");
    QString base64Img = ba.toBase64();

    // ZH: 建立 JSON 內容 | EN: Create JSON content
    QJsonObject json;
    json["image"] = base64Img;
    json["prompt"] = prompt;

    // ZH: 發送 POST 請求至 Python 後端 | EN: Send a POST request to the Python backend
    QNetworkRequest request(QUrl("http://127.0.0.1:8000/transform"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkManager->post(request, QJsonDocument(json).toJson());
}

void MainWindow::onAIResultReceived(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError)
    {
        QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        QByteArray resBa = QByteArray::fromBase64(res["result"].toString().toUtf8());

        QPixmap newPix;
        if (newPix.loadFromData(resBa))
        {
            ui->label->setPixmap(newPix);
            lastAIError.clear();    // ZH: 成功時清除錯誤訊息 | EN: Clear error message on success
        }
        else
        {
            // ZH: 圖片解碼失敗 | EN: Image decoding failed
            lastAIError = "AI Error: Failed to decode response image";
            QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), lastAIError, this, QRect(), 3000);
        }
    }
    else
    {
        // ZH: 網路或後端錯誤 | EN: Network or backend error
        lastAIError = QString("AI Error: %1").arg(reply->errorString());
        QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), lastAIError, this, QRect(), 3000);
    }

    reply->deleteLater();
    setState(Standing);
}

//===============================================================================================

//===============================================================================================

MainWindow::State MainWindow::getCurrentState() const
{
    return currentState;
}

double MainWindow::getVelX() const
{
    return currentVelocityX;
}

double MainWindow::getTargetVelX() const
{
    return targetVelocityX;
}

int MainWindow::getSteps() const
{
    return walkSteps;
}

double MainWindow::getDecisionTimerRemaining() const
{
    if (behaviorTimer && behaviorTimer->isActive())
    {
        return behaviorTimer->remainingTime() / 1000.0;
    }
    return 0.0;
}

int MainWindow::getActionRoll() const
{
    return actionRoll;
}

double MainWindow::getImageSwitchTimerRemaining() const
{
    if (imageSwitchTimer && imageSwitchTimer->isActive())
    {
        return imageSwitchTimer->remainingTime() / 1000.0;
    }
    return 0.0;
}

int MainWindow::getCurrentSetNumber() const
{
    return currentSetNumber;
}

QRect MainWindow::getCurrentScreenRect() const
{
    QScreen *screen = QGuiApplication::screenAt(this->geometry().center());
    if (!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }
    return screen->availableGeometry();
}

QString MainWindow::getLastAIError() const
{
    return lastAIError;
}

void MainWindow::initTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayMenu = new QMenu(this);

    // ZH: 設定托盤圖示 (使用 Standing 作為預設) | EN: Set tray icon (using Standing as default)
    trayIcon->setIcon(QIcon(imagePath + "Standing.png"));
    trayIcon->setToolTip("YaChiYo Desktop Pet");

    // ZH: 顯示桌寵 | EN: Show pet
    QAction *showAction = trayMenu->addAction("顯示桌寵 (Show Pet)");
    connect(showAction, &QAction::triggered, this, &MainWindow::show);

    // ZH: 開啟設定中心 | EN: Open settings
    QAction *settingsAction = trayMenu->addAction("設定中心 (Settings)");
    connect(settingsAction, &QAction::triggered, this, [this]()
    {
        settingsCenter->showWindow();
    });

    trayMenu->addSeparator();

    // ZH: 徹底退出程式 | EN: Quit application completely
    QAction *quitAction = trayMenu->addAction("退出程式 (Quit)");
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    // ZH: 雙擊托盤圖示顯示桌寵 | EN: Double click tray icon to show pet
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::DoubleClick)
        {
            this->show();
        }
    });
}

//===============================================================================================

//===============================================================================================

void MainWindow::setWalkSpeed(double speed)
{
    walkSpeed = speed;
}

void MainWindow::setGravity(double g)
{
    gravity = g;
}

void MainWindow::setBehaviorInterval(int ms)
{
    behaviorInterval = ms;
    if (behaviorTimer && behaviorTimer->isActive())
        behaviorTimer->start(ms);   // ZH: 立即套用新間隔 | EN: Apply new interval immediately
}

void MainWindow::setPetScale(double scale)
{
    petScale = scale;
    updatePetSkin();    // ZH: 立即更新顯示大小 | EN: Update display size immediately
}

void MainWindow::setAlwaysOnTop(bool onTop)
{
    Qt::WindowFlags flags = windowFlags();
    if (onTop)
        flags |= Qt::WindowStaysOnTopHint;
    else
        flags &= ~Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    show();     // ZH: setWindowFlags 會隱藏視窗，需重新顯示 | EN: setWindowFlags hides the window, need to show again
}

void MainWindow::setAiPrompt(const QString &prompt)
{
    aiPrompt = prompt;
}

MainWindow::~MainWindow()
{
    delete ui;
}
