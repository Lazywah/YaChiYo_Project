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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // ZH: 宣告右鍵選單事件 | EN: Declare right-click menu event
    void contextMenuEvent(QContextMenuEvent *event) override;

    // ZH: 宣告滑鼠拖曳相關事件 | EN: Declare mouse dragging event
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;

    // ZH: 圖片 & 動畫載入流程 | EN: Image & Animation Loading Process
    void loadImage(QString filename);
    void loadAnimation(QString filename);

private:
    Ui::MainWindow *ui;
    // ZH: 圖片 & 動畫載入路徑 | EN: Image & animation load path
    const QString imagePath = ":/res/images/characterAnimation/";
    // ZH: 用於計算滑鼠與視窗左上角的偏差值 | EN: Used to calculate the offset between the mouse and the top left corner of the viewport
    QPoint m_offset;
};
#endif // MAINWINDOW_H
