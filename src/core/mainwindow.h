#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QSystemTrayIcon>

#include "settingscenter.h"
#include "petphysics.h"
#include "petbehavior.h"
#include "aiclient.h"
#include "petsound.h"
#include "petsettings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTimer;
class QMenu;
class QMovie;

// ZH: 程式啟動時的功能開關，由 main.cpp 傳入
// EN: Feature flags passed from main.cpp at startup
struct PetConfig
{
    bool physicsEnabled  = true;
    bool behaviorEnabled = true;
    bool aiEnabled       = true;
    bool soundEnabled    = true;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_PROPERTY(State   currentState              READ getCurrentState)
    Q_PROPERTY(double  currentVelocityX          READ getVelX)
    Q_PROPERTY(double  targetVelocityX           READ getTargetVelX)
    Q_PROPERTY(int     walkSteps                 READ getSteps)
    Q_PROPERTY(double  decisionTimerRemaining    READ getDecisionTimerRemaining)
    Q_PROPERTY(int     actionRoll                READ getActionRoll)
    Q_PROPERTY(double  ImageSwitchTimerRemaining READ getImageSwitchTimerRemaining)
    Q_PROPERTY(int     currentSetNumber          READ getCurrentSetNumber)
    Q_PROPERTY(QString lastAIError               READ getLastAIError)

public:
    explicit MainWindow(const PetConfig &config = PetConfig(), QWidget *parent = nullptr);
    ~MainWindow();

    // ZH: 桌寵狀態列表 | EN: Pet state list
    enum State { Standing, Walking, Flying, Hovering, Captured, AI_Processing };
    Q_ENUM(State)

    // ZH: Q_PROPERTY Getter Functions
    State   getCurrentState()               const;
    double  getVelX()                       const;
    double  getTargetVelX()                 const;
    int     getSteps()                      const;
    double  getDecisionTimerRemaining()     const;
    int     getActionRoll()                 const;
    double  getImageSwitchTimerRemaining()  const;
    int     getCurrentSetNumber()           const;
    QString getLastAIError()                const;

    // ZH: 設定中心可調整的 Setter | EN: Setters adjustable from Settings Center
    void setWalkSpeed(double speed);
    void setGravity(double g);
    void setBehaviorInterval(int ms);
    void setPetScale(double scale);
    void setAlwaysOnTop(bool onTop);
    void setAiPrompt(const QString &prompt);
    void setPetSkinType(int type);

private:
    struct AnimationConfig { int totalFrames; int intervalMs; };

    Ui::MainWindow *ui;
    SettingsCenter *settingsCenter;
    PetConfig       config;

    // ZH: 子模組 | EN: Sub-modules
    PetPhysics  physics;
    PetBehavior behavior;
    AIClient   *aiClient = nullptr;
    PetSound   *sound    = nullptr;

    // ZH: 狀態機 | EN: State machine
    State currentState = Captured;

    // ZH: 行走步數（跨模組橋接變數）| EN: Walk steps (bridge between behavior and physics)
    int walkSteps = 0;

    // ZH: 最後一次決策值（開發者監控用）| EN: Last action roll (developer monitor)
    int actionRoll = 0;

    // ZH: AI 相關 | EN: AI related
    QString lastAIError;
    QString aiPrompt = "transform this character into a new style";

    // ZH: 動畫 & 外觀 | EN: Animation & appearance
    int    petSkinType = 1;    // ZH: 皮膚模式 (0: 僅 PNG | 1: 優先使用 GIF) | EN: Skin mode (0: PNG only | 1: prefer GIF)
    double petScale    = 1.0;
    int    currentSetNumber = 0;
    const QString imagePath        = ":/res/images/characterAnimation/";
    const QString testImageSetPath = ":/res/images/testImageSet/";
    QMap<State, AnimationConfig> animConfigs;
    QMovie *currentMovie = nullptr;

    // ZH: 計時器 | EN: Timers
    QTimer *physicsTimer;
    QTimer *behaviorTimer;
    QTimer *imageSwitchTimer;
    int behaviorInterval = 5000;

    // ZH: 滑鼠拖曳 | EN: Mouse drag
    QPoint m_offset;

    // ZH: 系統托盤 | EN: System tray
    QSystemTrayIcon *trayIcon;
    QMenu           *trayMenu;

    // ZH: 初始化 | EN: Initialisation
    void initAnimationConfigs();
    void initAllConnect();
    void initTrayIcon();

    // ZH: 設定持久化 | EN: Settings persistence
    void applySettings(const PetSettingsData &s);   // ZH: 套用載入的設定到各模組 | EN: Apply loaded settings to modules
    void saveSettings() const;                      // ZH: 將目前狀態寫回儲存 | EN: Persist current state

    // ZH: 核心邏輯 | EN: Core logic
    void setState(State nextState);
    void updatePhysics();
    void decideNextAction();
    void updatePetSkin();
    void turnImageSet();
    void requestAIProcessing(const QString &prompt);
    QRect getCurrentScreenRect() const;

private slots:
    void onAIResultReady(QPixmap result);
    void onAIError(QString errorMsg);

protected:
    void contextMenuEvent(QContextMenuEvent *event)  override;
    void mousePressEvent(QMouseEvent *event)         override;
    void mouseMoveEvent(QMouseEvent *event)          override;
    void mouseReleaseEvent(QMouseEvent *event)       override;
};

#endif // MAINWINDOW_H
