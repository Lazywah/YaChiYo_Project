#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QSystemTrayIcon>
#include <QImage>
#include <QList>
#include <QStringList>

#include "settingscenter.h"
#include "petphysics.h"
#include "petbehavior.h"
#include "aiclient.h"
#include "petsound.h"
#include "petsettings.h"
#include "petskin.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTimer;
class QMenu;
class QMovie;
class QProgressDialog;
class Live2DWidget;

// ZH: 程式啟動時的功能開關，由 main.cpp 傳入
// EN: Feature flags passed from main.cpp at startup
struct PetConfig
{
    bool physicsEnabled  = true;
    bool behaviorEnabled = true;
    bool aiEnabled       = true;
    bool soundEnabled    = true;

    // ZH: Live2D 模式 (改用 Live2D 角色渲染，取代幀皮膚) | EN: Live2D mode (render a Live2D character instead of frame skins)
    bool    live2dEnabled  = false;
    QString live2dModelDir;     // ZH: 模型資料夾完整路徑 | EN: full path to the model folder
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
    void setSkin(const QString &id);    // ZH: 切換皮膚 (依 id) | EN: Switch skin by id
    void setMovementEnabled(bool on);   // ZH: 自動移動開關 | EN: auto-movement toggle
    void setFlyingEnabled(bool on);     // ZH: 飛行開關 | EN: flying toggle
    void setHoveringEnabled(bool on);   // ZH: 懸浮開關 | EN: hovering toggle
    void setLive2dWidth(int w);         // ZH: Live2D 視窗寬度 | EN: Live2D window width
    void setLive2dModeSetting(bool on); // ZH: Live2D 模式設定 (重啟生效，僅存設定) | EN: Live2D mode setting (restart-applied; saves only)
    bool isLive2dMode() const { return m_live2dMode; }   // ZH: 目前是否 Live2D 模式 | EN: currently in Live2D mode

private:
    Ui::MainWindow *ui;
    SettingsCenter *settingsCenter;
    PetConfig       config;

    // ZH: 子模組 | EN: Sub-modules
    PetPhysics  physics;
    PetBehavior behavior;
    AIClient   *aiClient = nullptr;
    PetSound   *sound    = nullptr;
    PetSkin     skin;                       // ZH: 當前皮膚 (資料驅動) | EN: Current skin (data-driven)
    QString     currentSkinId = "default";  // ZH: 當前皮膚 id | EN: Current skin id

    // ZH: Live2D 模式 | EN: Live2D mode
    Live2DWidget *m_live2d   = nullptr;     // ZH: Live2D 渲染面 (取代 QLabel) | EN: Live2D surface (replaces QLabel)
    bool          m_live2dMode = false;
    int           m_live2dBaseW = 200;      // ZH: Live2D 視窗基準寬度 | EN: Live2D base window width
    void applyLive2DSize();                 // ZH: 依 petScale 設定 Live2D 視窗大小 | EN: size the Live2D window by petScale

    // ZH: 自動移動 / 飛行 / 懸浮 開關 | EN: auto-movement / flying / hovering toggles
    bool m_movementEnabled = true;
    bool m_flyingEnabled   = true;
    bool m_hoveringEnabled = true;
    // ZH: 可懸浮 = 飛行開 或 懸浮開 (飛行開連動懸浮開) | EN: can hover = flying on OR hovering on
    bool canHover() const { return m_flyingEnabled || m_hoveringEnabled; }

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
    QMovie *currentMovie = nullptr;

    // ZH: 計時器 | EN: Timers
    QTimer *physicsTimer;
    QTimer *behaviorTimer;
    QTimer *imageSwitchTimer;
    int behaviorInterval = 5000;

    // ZH: 滑鼠拖曳 | EN: Mouse drag
    QPoint m_offset;
    QPoint m_pressPos;     // ZH: 按下時的全域座標 (判斷點擊 vs 拖曳) | EN: press global pos (tap vs drag)

    // ZH: 系統托盤 | EN: System tray
    QSystemTrayIcon *trayIcon;
    QMenu           *trayMenu;

    // ZH: 初始化 | EN: Initialisation
    void initAllConnect();
    void initTrayIcon();

    // ZH: 設定持久化 | EN: Settings persistence
    void applySettings(const PetSettingsData &s);   // ZH: 套用載入的設定到各模組 | EN: Apply loaded settings to modules
    void saveSettings() const;                      // ZH: 將目前狀態寫回儲存 | EN: Persist current state

    // ZH: 依 id 載入皮膚，找不到時退回內建 default | EN: Load skin by id, fall back to built-in default
    void loadSkinById(const QString &id);

    // ZH: 核心邏輯 | EN: Core logic
    void setState(State nextState);
    void updatePhysics();
    void decideNextAction();
    void updatePetSkin();
    void turnImageSet();
    void requestAIProcessing(const QString &prompt);
    QRect getCurrentScreenRect() const;

    // ZH: AI 生成皮膚 | EN: AI skin generation
    void requestSkinGeneration();
    void collectCurrentSkinFrames(QStringList &relPaths, QList<QImage> &images) const;
    QStringList pendingSkinPaths;   // ZH: 送出的幀對應的相對路徑 (寫回時用) | EN: target rel paths for the sent frames

    // ZH: AI 處理中的提示視窗 | EN: Busy dialog shown during AI processing
    QProgressDialog *busyDialog = nullptr;
    void showBusy(const QString &text);
    void hideBusy();

private slots:
    void onAIResultReady(QPixmap result);
    void onAIError(QString errorMsg);
    void onSkinReady(QList<QImage> results);

protected:
    void contextMenuEvent(QContextMenuEvent *event)  override;
    void mousePressEvent(QMouseEvent *event)         override;
    void mouseMoveEvent(QMouseEvent *event)          override;
    void mouseReleaseEvent(QMouseEvent *event)       override;
};

#endif // MAINWINDOW_H
