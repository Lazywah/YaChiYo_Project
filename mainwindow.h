#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>               // ZH: 紀錄點擊座標 | EN: Record click coordinates
#include <QMouseEvent>          // ZH: 處理滑鼠事件 | EN: Handling mouse events
#include <QContextMenuEvent>    // ZH: 處理右鍵選單 | EN: Handling right-click menus

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

private:
    Ui::MainWindow *ui;
    // ZH: 桌寵狀態(事件)列表 | EN: Desktop pet status(event) list
    enum State { Standing, Flying, Hovering, Captured };
    // ZH: 圖片 & 動畫載入路徑 | EN: Image & animation load path
    const QString imagePath = ":/res/images/characterAnimation/";
    // ZH: 用於計算滑鼠與視窗左上角的偏差值 | EN: Used to calculate the offset between the mouse and the top left corner of the viewport
    QPoint m_offset;
    // ZH: 物理引擎變數與函數 | EN: Physics engine value and function
    QTimer *physicsTimer;
    double velocityY = 0;           // ZH: 垂直速度 | EN: Vertical velocity
    const double gravity = 0.8;     // ZH: 重力加速度 | EN: Gravitational acceleration
    bool isGrounded = false;        // ZH: 在地面上(狀態) | EN: on the ground(status)
    State currentState = Standing;  // ZH: 當前狀態 | EN: Current state
    void applyGravity();            // ZH: 計算下落位移 | EN: Calculate the falling displacement
    void checkGroundCollision();    // ZH: 螢幕底部偵測 | EN: Bottom Screen Detection

protected:
    // ZH: 宣告右鍵選單事件 | EN: Declare right-click menu event
    void contextMenuEvent(QContextMenuEvent *event) override;

    // ZH: 宣告滑鼠拖曳相關事件 | EN: Declare mouse dragging event
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;

    // ZH: 圖片 & 動畫載入流程 | EN: Image & Animation loading process
    void loadImage(QString filename);
    void loadAnimation(QString filename);

    // ZH: 時間刻內流程 | EN: Time-bound process
    void timerProcess();
    // ZH: 狀態設定流程 | EN: Status setting process
    void setState();
    // ZH: 物理引擎流程 | EN: Physics engine process
    void updatePhysics();
};
#endif // MAINWINDOW_H
