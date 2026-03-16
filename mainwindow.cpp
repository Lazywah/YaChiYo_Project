#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QMenu>                // ZH: 右鍵選單容器 | EN: Right-clock menu container
#include <QAction>              // ZH: 選單動作項目 | EN: Menu actions
#include <QContextMenuEvent>    // ZH: 右鍵點擊事件 | EN: Right-click event
#include <QPixmap>              // ZH: 處理靜態圖 | EN: Processing static images
#include <QMovie>               // ZH: 處理動態圖 | EN: Processing dynamic images
#include <QScreen>              // ZH: 獲取螢幕資訊 | EN: Get screen information
#include <QGuiApplication>      // ZH: 監聽螢幕變化 | EN: Listen to screen changes
#include <QTimer>

//===============================================================================================

//===============================================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // --> setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::FramelessWindowHint;   // ZH: 消除視窗標題與邊框 | EN: Remove window title and border
    flags |= Qt::WindowStaysOnTopHint;  // ZH: 使視窗永遠置於頂層 | EN: Make window always stay on top
    setWindowFlags(flags);

    setAttribute(Qt::WA_TranslucentBackground); // ZH: 將視窗背景設為透明 | EN: Set window background to transparent

    physicsTimer = new QTimer(this);                                            // ZH: 初始化物理引擎計時器 | EN: Initialize physics engine timer
    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::timerProcess);   // ZH: 串接上發送者、訊號、接收者、需運行函數 | EN: Connect the sender, signal, receiver, and function to be executed.
    physicsTimer->start(16);                                                    // ZH: 啟用物理引擎計時器，更新頻率 16ms(約 60FPS) | EN: Enable physics engine timer, updating at a frequency of 16ms(approximately 60 FPS)
    currentState = Standing;
}

//===============================================================================================

//===============================================================================================

// ZH: 宣告右鍵選單中的"關閉桌寵"事件 | EN: Declare event "Close Desktop Pet" in right-click menu
void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *closeAction = menu.addAction("關閉桌寵");

    connect(closeAction, &QAction::triggered, this, &MainWindow::close);

    menu.exec(event->globalPos());
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        currentState = Captured;

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
        currentState = Standing;
        physicsTimer->start(16);    // ZH: 啟用物理引擎計時器，更新頻率 16ms(約 60FPS) | EN: Enable physics engine timer, updating at a frequency of 16ms(approximately 60 FPS)
    }

    //Q_UNUSED(event);    // ZH: 僅用於消除因為撰寫函數邏輯的警告 | EN: Used only to eliminate warnings caused by writing function logic
}

// ZH: 設置靜態圖 | EN: Set static images
void MainWindow::loadImage(QString filename)
{
    QPixmap pix(imagePath + filename);
    pix = pix.scaled(320, 640, Qt::KeepAspectRatio, Qt::SmoothTransformation);  // ZH: 手動調整 pix 大小 | EN: Manually adjust the pix size

    ui->label->setPixmap(pix);
    ui->label->adjustSize();    // ZH: 自動調整 label 大小以包覆 pix | EN: Auto adjust the label size to cover the pix
    this->adjustSize();         // ZH: 自動調整視窗大小以包覆 label | EN: Auto adjust the window size to cover the label
}

// ZH: 設置動態圖 | EN: Set dynamic images
void MainWindow::loadAnimation(QString filename)
{
    QMovie *movie = new QMovie(imagePath + filename);
    ui->label->setMovie(movie);

    ui->label->setScaledContents(true); // ZH: 使動畫自適應 label 大小 | EN: Make the animation adapt to the label size

    // ZH: 取得動畫第一幀的尺寸以調整視窗大小 | EN: Get the size of the first frame of the animation to adjust the viewport size.
    movie->jumpToFrame(0);
    QSize movieSize = movie->currentImage().size();
    ui->label->setFixedSize(movieSize);
    this->setFixedSize(movieSize);

    // ZH: 手動調整動畫大小 | EN: Manually adjust the animation size
    movie->setScaledSize(QSize(256, 256));
    ui->label->setFixedSize(256, 256);
    this->setFixedSize(256, 256);

    movie->start();
}

void MainWindow::timerProcess()
{
    updatePhysics();
    setState();
}

void MainWindow::setState()
{
    if (currentState == Standing)
    {
        loadImage("normal.jpg");
        //loadAnimation("normal.gif");
    }
    else if (currentState == Flying)
    {
        // ID: feat-1
        // ZH: 未來新增功能 | EN: Future new features
    }
    else if (currentState == Hovering)
    {
        // ID: feat-2
        // ZH: 未來新增功能 | EN: Future new features
    }
    else if (currentState == Captured)
    {
        physicsTimer->stop();   // ZH: 停用物理引擎計時器 | EN: Disable physics engine timer

        loadImage("catch.png");
        //loadAnimation("catch.png");
    }
}

void MainWindow::updatePhysics()
{
    switch (currentState)
    {
        case Standing:
            // ZH: 當桌寵落地時，啟用 X 軸速度是桌寵行走並撞牆回彈 | EN: When the desktop pet lands, activating the X-axis speed will cause the desktop pet to walk and bounce back after hitting a wall
            if (isGrounded)
            {
                this->move(this->x() + (int)velocityX, this->y());
                checkBoundaryCollision();
            }

            // ZH: 啟用重力使桌寵落地 | EN: Use gravity to make desktop pet land
            applyGravity();
            checkGroundCollision();
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
    if (this->x() <= leftWall)
    {
        // ZH: 撞到左邊界 | EN: Hit the left boundary
        this->move(leftWall, this->y());    // ZH: 修正位置防止出界
        velocityX *= wallBounceFactor;      // ZH: 反彈
    }
    else if (this->x() >= rightWall)
    {
        // ZH: 撞到右邊界 | EN: Hit the right boundary
        this->move(rightWall, this->y());   // ZH: 修正位置防止出界
        velocityX *= wallBounceFactor;      // ZH: 反彈
    }
}

//===============================================================================================

//===============================================================================================

MainWindow::~MainWindow()
{
    delete ui;
}
