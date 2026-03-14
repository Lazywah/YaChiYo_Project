#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QPixmap>              // ZH: 處理靜態圖 | EN: Processing static images
#include <QMovie>               // ZH: 處理動態圖 | EN: Processing dynamic images

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

    loadImage("normal.jpg");    // ZH: 載入圖片 | EN: load image

    //loadAnimation("normal.gif");  // ZH: 載入動畫 | EN: load animation
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

MainWindow::~MainWindow()
{
    delete ui;
}
