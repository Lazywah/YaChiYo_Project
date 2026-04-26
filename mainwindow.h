#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>                   // ZH: 紀錄點擊座標 | EN: Record click coordinates
#include <QMouseEvent>              // ZH: 處理滑鼠事件 | EN: Handling mouse events
#include <QContextMenuEvent>        // ZH: 處理右鍵選單 | EN: Handling right-click menus
// ZH: AI 通訊所需 | EN: AI communication required
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>

#include "settingscenter.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    // ZH: 宣告屬性: Q_PROPERTY(型別 名稱 READ 讀取函數) | EN: Declare property: Q_PROPERTY(type name READ read function)
    Q_PROPERTY(State currentState READ getCurrentState)
    Q_PROPERTY(double currentVelocityX READ getVelX)
    Q_PROPERTY(double targetVelocityX READ getTargetVelX)
    Q_PROPERTY(int walkSteps READ getSteps)
    Q_PROPERTY(double decisionTimerRemaining READ getDecisionTimerRemaining)
    Q_PROPERTY(int actionRoll READ getActionRoll)
    Q_PROPERTY(double ImageSwitchTimerRemaining READ getImageSwitchTimerRemaining)
    Q_PROPERTY(int currentSetNumber READ getCurrentSetNumber)
    Q_PROPERTY(QString lastAIError READ getLastAIError)

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    // ZH: 桌寵狀態(事件)列表 | EN: Desktop pet status(event) list
    enum State { Standing,
                 Walking,
                 Flying,
                 Hovering,
                 Captured,
                 AI_Processing };
    Q_ENUM(State);  // ZH: 讓 Qt 記住名稱的字串形式 | EN: Let Qt remember the string form of the name
    // ZH: Q_PROPERTY 對應的 Getter Functions | EN: Getter Functions corresponding to Q_PROPERTY
    State getCurrentState() const;
    double getVelX() const;
    double getTargetVelX() const;
    int getSteps() const;
    double getDecisionTimerRemaining() const;
    int getActionRoll() const;
    double getImageSwitchTimerRemaining() const;
    int getCurrentSetNumber() const;
    QString getLastAIError() const;

private:
    // ZH: 定義動畫配置結構 | EN: Define animation configuration structure
    struct AnimationConfig
    {
        int totalFrames;    // ZH: 總影格數 | EN: Total number of frames
        int intervalMs;     // ZH: 播放速度(ms) | EN: Playback speed(ms)
    };

    Ui::MainWindow *ui;
    SettingsCenter * settingsCenter;    // ZH: 宣告設定中心視窗指標 | EN: Announce settings center window pointer

    // ZH: 狀態設定流程 | EN: Status setting process
    void setState(MainWindow::State nextState);
    void initAnimationConfigs();                // ZH: 初始化狀態動畫相關參數 | EN: Initialization state animation related parameters
    void initAllConnect();                        // ZH: 初始化所有計時器 | EN: Initialize all timers

    // ZH: 圖片 & 動畫載入路徑 | EN: Image & animation load path
    int petSkinType = 0;    // ZH: 用於決定桌寵皮膚的呈現形式(0、png | 1、gif) | Used to determine the presentation format of the desktop pet skin(0. png | 1. gif)
    const QString imagePath = ":/res/images/characterAnimation/";
    const QString testImageSetPath = ":/res/images/testImageSet/";
    QMap<State, AnimationConfig> animConfigs;   // ZH: 狀態與配置對照表
    void updatePetSkin();
    // void loadImage(QString filename, int setNumber = 0);
    // void loadAnimation(QString filename);

    // ZH: 用於計算滑鼠與視窗左上角的偏差值 | EN: Used to calculate the offset between the mouse and the top left corner of the viewport
    QPoint m_offset;

    // ZH: 物理引擎變數與函數 | EN: Physics engine value and function
    QTimer *physicsTimer;
    double velocityY = 0;                   // ZH: 垂直速度 | EN: Vertical velocity
    const double gravity = 0.8;             // ZH: 重力加速度 | EN: Gravitational acceleration
    const double bounceFactor = -0.5;       // ZH: 垂直回彈係數(負號代表反彈，0.5 代表回彈一半能量) | EN: Vertical rebound coefficient (a negative sign indicates a rebound, 0.5 means that half of the energy is rebounded)
    bool isGrounded = false;                // ZH: 在地面上(狀態) | EN: on the ground(status)
    State currentState = Captured;          // ZH: 當前狀態 | EN: Current state
    void applyGravity();                    // ZH: 計算下落位移 | EN: Calculate the falling displacement
    void checkGroundCollision();            // ZH: 螢幕邊緣偵測(底部) | EN: Screen edge detection(bottom)
    const double wallBounceFactor = -1;     // ZH: 水平回彈係數(負號代表反彈) | EN: Horizontal rebound coefficient(negative sign indicates rebound, 0.8 represents 80% energy rebound)
    double currentVelocityX = 0;            // ZH: 當前實際水平速度 | EN: Current actual horizontal speed
    const double acceleration = 0.2;        // ZH: 加速度 | EN: acceleration
    const double friction = 0.15;           // ZH: 摩擦力 / 減速度 | EN: Friction / deceleration
    double targetVelocityX = 0;             // ZH: 目標速度(由 AI 決定) | EN: Target speed(determined by AI)
    void checkBoundaryCollision();          // ZH: 螢幕邊緣偵測(左右) | EN: Screen edge detection(left and right)
    void updatePhysics();                   // ZH: 物理引擎流程 | EN: Physics engine process

    // ZH: 浮空 & 飛行狀態變數 | EN: Hovering & Flying state variables
    double hoverPhase = 0.0;                // ZH: Sin Wave 相位 (弧度) | EN: Sin Wave phase (radians)
    int hoverBaseY = 0;                     // ZH: 浮空基準 Y 座標 | EN: Hovering base Y coordinate
    const double hoverAmplitude = 8.0;      // ZH: 浮動振幅 (像素) | EN: Hovering amplitude (pixels)
    const double hoverSpeed = 0.08;         // ZH: 浮動速度 (相位增量) | EN: Hovering speed (phase increment)
    int flyTargetX = 0;                     // ZH: 飛行目標 X | EN: Flying target X
    int flyTargetY = 0;                     // ZH: 飛行目標 Y | EN: Flying target Y
    const double flySpeed = 1.5;            // ZH: 飛行速度 | EN: Flying speed

    // ZH: 行動邏輯 | EN: Action Logic
    QTimer *behaviorTimer;      // ZH: 用於計時幾秒思考一次下一次行動 | EN: Used to time a few seconds to consider the next action
    QTimer *imageSwitchTimer;   // ZH: 用於計時圖片切換頻率 | EN: Used to time image switching frequency
    int currentSetNumber;       // ZH: 用於紀錄要輪到哪張圖片 | EN: Used to record which image is next
    void turnImageSet();        // ZH: 用於控制 currentSetNumber 並轉換圖片 | EN: Used to control currentSetNumber and transform images
    int walkSteps = 0;          // ZH: 行走步數(控制行走時間) | EN: Walking steps(controlling walking time)
    double walkSpeed = 2.0;     // ZH: 行走速度 | EN: Walking Speed
    int actionRoll;             // ZH: 用於機率決策(提供開發者模式觀察用) | EN:
    void decideNextAction();    // ZH: 行動決策邏輯 | EN: Action Decision Logic

    // ZH: AI 通訊 | EN: AI communication
    QNetworkAccessManager *networkManager;
    QString lastAIError;    // ZH: 最後一次 AI 通訊錯誤訊息 | EN: Last AI communication error message
    void requestAIProcessing(const QString &prompt);

private slots:
    // ZH: AI 結果回傳 | EN: AI Result Received
    void onAIResultReceived(QNetworkReply *reply);

protected:
    // ZH: 宣告右鍵選單事件 | EN: Declare right-click menu event
    void contextMenuEvent(QContextMenuEvent *event) override;

    // ZH: 宣告滑鼠拖曳相關事件 | EN: Declare mouse dragging event
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
};
#endif // MAINWINDOW_H
