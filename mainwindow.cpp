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

//===============================================================================================

//===============================================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    settingsCenter = new SettingsCenter(this);  // ZH: 將設定中心設置為該視窗的子視窗 | EN: Set the settings center as a child window of this window

    // --> setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::FramelessWindowHint;   // ZH: 消除視窗標題與邊框 | EN: Remove window title and border
    flags |= Qt::WindowStaysOnTopHint;  // ZH: 使視窗永遠置於頂層 | EN: Make window always stay on top
    setWindowFlags(flags);

    setAttribute(Qt::WA_TranslucentBackground); // ZH: 將視窗背景設為透明 | EN: Set window background to transparent

    initAnimationConfigs();

    initAllConnect();

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
    behaviorTimer->start(5000);                                                     // ZH: 啟用行動決策計時器(每 3s 執行一次) | EN: Enable action decision timer (executes every 3 seconds)

    imageSwitchTimer = new QTimer;                                                  // ZH: 初始化圖像集切換計時器 | EN: Initialize image set switching timer
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

    // ZH: 關閉桌寵 | EN: Close desktop pet
    QAction *closeAction = menu.addAction("關閉桌寵");
    connect(closeAction, &QAction::triggered, this, &MainWindow::close);

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
        setState(Standing);
        physicsTimer->start(16);    // ZH: 啟用物理引擎計時器，更新頻率 16ms(約 60FPS) | EN: Enable physics engine timer, updating at a frequency of 16ms(approximately 60 FPS)
    }

    //Q_UNUSED(event);    // ZH: 僅用於消除因為撰寫函數邏輯的警告 | EN: Used only to eliminate warnings caused by writing function logic
}

void MainWindow::updatePetSkin()
{
    // ZH: 自動獲取當前狀態的字串 | EN: Auto retrieve the string of the current state
    QMetaEnum metaEnum = QMetaEnum::fromType<MainWindow::State>();
    QString stateName = metaEnum.valueToKey(currentState);
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

    // ZH: 處理水平翻轉 | EN: Handling horizontal flipping
    if (qAbs(currentVelocityX) > 0.1)
    {
        if (currentVelocityX < -0.1)
        {
            // pix = QPixmap::fromImage(pix.toImage().mirrored(true, false));
            pix = QPixmap::fromImage(pix.toImage().flipped(Qt::Horizontal));    // ZH: 使用 Qt 6 規範寫法 | EN: Using Qt 6 standard syntax
        }
    }

    ui->label->setPixmap(pix.scaled(250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->label->adjustSize();
    this->adjustSize();
}

// ZH: 設置靜態圖 | EN: Set static images
// void MainWindow::loadImage(QString filename, int setNumber)
// {
//     QPixmap pix = QPixmap(imagePath + filename + ".png")
//     pix = pix.scaled(320, 640, Qt::KeepAspectRatio, Qt::SmoothTransformation);  // ZH: 手動調整 pix 大小 | EN: Manually adjust the pix size

//     ui->label->setPixmap(pix);
//     ui->label->adjustSize();    // ZH: 自動調整 label 大小以包覆 pix | EN: Auto adjust the label size to cover the pix
//     this->adjustSize();         // ZH: 自動調整視窗大小以包覆 label | EN: Auto adjust the window size to cover the label
// }

// ZH: 設置動態圖 | EN: Set dynamic images
// void MainWindow::loadAnimation(QString filename)
// {
//     ui->label->setMovie(movie);

//     ui->label->setScaledContents(true); // ZH: 使動畫自適應 label 大小 | EN: Make the animation adapt to the label size

//     // ZH: 取得動畫第一幀的尺寸以調整視窗大小 | EN: Get the size of the first frame of the animation to adjust the viewport size.
//     movie->jumpToFrame(0);
//     QSize movieSize = movie->currentImage().size();
//     ui->label->setFixedSize(movieSize);
//     this->setFixedSize(movieSize);

//     // ZH: 手動調整動畫大小 | EN: Manually adjust the animation size
//     movie->setScaledSize(QSize(256, 256));
//     ui->label->setFixedSize(256, 256);
//     this->setFixedSize(256, 256);

//     movie->start();
// }

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
        // ID: feat-1
        // ZH: 未來新增功能 | EN: Future new features
        // ZH: 關閉重力，執行位移邏輯，使桌寵前往滑鼠位置 | EN: Turn off gravity, execute displacement logic, and move the desktop pet to the mouse position
        //moveToTarget();
        break;

    case Hovering:
        // ID: feat-2
        // ZH: 未來新增功能 | EN: Future new features
        // ZH: 垂直速度設為 0，只做微小的上下浮動 (Sin Wave) | EN: The vertical velocity is set to 0, resulting in only a slight up-and-down fluctuation (Sin Wave)
        //applyBovverEffect();
        break;

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
    // ZH: 獲取當前視窗所在螢幕的可用區域(避開工作列) | EN: Get the available area of ​​the screen where the current window is located (excluding the task bar)
    QRect screenRect = QGuiApplication::primaryScreen()->availableGeometry();
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
    QRect screenRect = QGuiApplication::primaryScreen()->availableGeometry();

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

    QRect screenRect = QGuiApplication::primaryScreen()->availableGeometry();
    int usableWidth = screenRect.width() - this->width();
    // ZH: 計算目前的 X 座標相對於螢幕可用區域的比例(0.0 為最左，1.0 為最右) | EN: Calculate the current X coordinate relative to the available screen area (0.0 is the leftmost, 1.0 is the rightmost)
    double positionRatio = static_cast<double>(this->x() - screenRect.left()) / usableWidth;

    actionRoll = QRandomGenerator::global()->bounded(100);    // ZH: 決定下一動作(0~99) | EN: Decide on the next action(0~99)

    if (actionRoll < 60)  // ZH: 60% 開始移動(散步) | EN: 60% started moving(walking)
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
            ui->label->setPixmap(newPix);
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

//===============================================================================================

//===============================================================================================

MainWindow::~MainWindow()
{
    delete ui;
}
