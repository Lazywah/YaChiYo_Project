#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>

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
}

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
        // ZH: 紀錄滑鼠點擊位置與視窗左上角的偏差值 | EN: record offset mouse click position to window Top Left
        m_offset = event->globalPosition().toPoint() - this->pos();
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
    Q_UNUSED(event);
}

MainWindow::~MainWindow()
{
    delete ui;
}
