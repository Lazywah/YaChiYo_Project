#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>               // ZH: 紀錄點擊座標 | EN: Record click coordinates
#include <QMouseEvent>          // ZH: 處理滑鼠事件 | EN: Handling mouse events
#include <QContextMenuEvent>    // ZH: 處理右鍵選單 | EN: Handling right-click menus

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

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    // ZH: 桌寵狀態(事件)列表 | EN: Desktop pet status(event) list
    enum State { Standing, Flying, Hovering, Captured };
    Q_ENUM(State);  // ZH: 讓 Qt 記住名稱的字串形式 | EN: Let Qt remember the string form of the name

private:
    Ui::MainWindow *ui;
    SettingsCenter * settingsCenter;    // ZH: 宣告設定中心視窗指標 | EN: Announce settings center window pointer
    // ZH: 圖片 & 動畫載入路徑 | EN: Image & animation load path
    int petSkinType = 0;    // ZH: 用於決定桌寵皮膚的呈現形式(0、png | 1、gif) | Used to determine the presentation format of the desktop pet skin(0. png | 1. gif)
    const QString imagePath = ":/res/images/characterAnimation/";
    // ZH: 用於計算滑鼠與視窗左上角的偏差值 | EN: Used to calculate the offset between the mouse and the top left corner of the viewport
    QPoint m_offset;
    // ZH: 物理引擎變數與函數 | EN: Physics engine value and function
    QTimer *physicsTimer;
    double velocityY = 0;                   // ZH: 垂直速度 | EN: Vertical velocity
    const double gravity = 0.8;             // ZH: 重力加速度 | EN: Gravitational acceleration
    const double bounceFactor = -0.5;       // ZH: 垂直回彈係數(負號代表反彈，0.5 代表回彈一半能量) | EN: Vertical rebound coefficient (a negative sign indicates a rebound, 0.5 means that half of the energy is rebounded)
    bool isGrounded = false;                // ZH: 在地面上(狀態) | EN: on the ground(status)
    State currentState = Standing;          // ZH: 當前狀態 | EN: Current state
    void applyGravity();                    // ZH: 計算下落位移 | EN: Calculate the falling displacement
    void checkGroundCollision();            // ZH: 螢幕邊緣偵測(底部) | EN: Screen edge detection (bottom)
    double velocityX = 5.0;                 // ZH: 水平速度(正值向右，負值向左) | EN: Horizontal velocity (positive values ​​to the right, negative values ​​to the left)
    const double wallBounceFactor = -1;   // ZH: 水平回彈係數(負號代表反彈) | EN: Horizontal rebound coefficient (negative sign indicates rebound, 0.8 represents 80% energy rebound)
    void checkBoundaryCollision();          // ZH: 螢幕邊緣偵測(左右) | EN: Screen edge detection (left and right)

protected:
    // ZH: 宣告右鍵選單事件 | EN: Declare right-click menu event
    void contextMenuEvent(QContextMenuEvent *event) override;

    // ZH: 宣告滑鼠拖曳相關事件 | EN: Declare mouse dragging event
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;

    // ZH: 圖片 & 動畫載入流程 | EN: Image & Animation loading process
    void updatePetSkin();
    void loadImage(QString filename);
    void loadAnimation(QString filename);

    // ZH: 狀態設定流程 | EN: Status setting process
    void setState(MainWindow::State nextState);
    // ZH: 物理引擎流程 | EN: Physics engine process
    void updatePhysics();
};
#endif // MAINWINDOW_H
