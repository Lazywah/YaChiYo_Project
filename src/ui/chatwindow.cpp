#include "chatwindow.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)   // ZH: 獨立頂層視窗 | EN: standalone top-level window
{
    setWindowTitle(QStringLiteral("跟八千代聊天"));
    resize(430, 580);

    // ZH: 捲動區 + 內容容器 + 垂直排列 (尾端 stretch 讓訊息少時靠上) | EN: scroll area + container + vbox (trailing stretch)
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);

    m_container = new QWidget;
    m_vbox = new QVBoxLayout(m_container);
    m_vbox->setContentsMargins(8, 8, 8, 8);
    m_vbox->setSpacing(2);
    m_vbox->addStretch(1);          // ZH: 尾端彈簧 (訊息插在它之前) | EN: trailing spring (rows inserted before it)
    m_scroll->setWidget(m_container);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("輸入訊息，Enter 送出…"));
    m_send  = new QPushButton(QStringLiteral("送出"), this);

    auto *row = new QHBoxLayout;
    row->addWidget(m_input);
    row->addWidget(m_send);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->addWidget(m_scroll, 1);
    col->addLayout(row);

    connect(m_send,  &QPushButton::clicked,     this, &ChatWindow::onSend);
    connect(m_input, &QLineEdit::returnPressed, this, &ChatWindow::onSend);
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this, &ChatWindow::onScroll);

    // ZH: 依系統主題選泡泡配色 | EN: pick bubble colors from the theme
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    if (dark)
    {
        m_userBg = QStringLiteral("#2b5278"); m_userFg = QStringLiteral("#f2f2f2");
        m_asstBg = QStringLiteral("#3a3b3c"); m_asstFg = QStringLiteral("#ededed");
    }
    else
    {
        m_userBg = QStringLiteral("#cfe6ff"); m_userFg = QStringLiteral("#101010");
        m_asstBg = QStringLiteral("#ececec"); m_asstFg = QStringLiteral("#101010");
    }

    m_input->setFocus();
}

void ChatWindow::closeEvent(QCloseEvent *e)
{
    // ZH: 關閉=隱藏 (MainWindow 持有單例) | EN: close = hide (MainWindow keeps the singleton)
    hide();
    e->ignore();
}

int ChatWindow::bubbleMaxWidth() const
{
    const int vw = m_scroll->viewport()->width();
    const int base = vw > 60 ? vw : width();   // ZH: 尚未佈局時退回視窗寬 | EN: fall back to window width pre-layout
    return qMax(200, static_cast<int>(base * 0.74));
}

QWidget *ChatWindow::makeBubbleRow(const ChatMsg &m) const
{
    const bool user = (m.role == QLatin1String("user"));
    const QString bg = user ? m_userBg : m_asstBg;
    const QString fg = user ? m_userFg : m_asstFg;

    auto *bubble = new QLabel(m.content.trimmed());
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubble->setMaximumWidth(bubbleMaxWidth());
    bubble->setStyleSheet(QStringLiteral(
        "QLabel { background:%1; color:%2; border-radius:12px; padding:8px 11px; }").arg(bg, fg));

    auto *rowW = new QWidget;
    auto *h = new QHBoxLayout(rowW);
    h->setContentsMargins(0, 2, 0, 2);
    if (user) { h->addStretch(1); h->addWidget(bubble); }   // ZH: 使用者靠右 | EN: user right
    else      { h->addWidget(bubble); h->addStretch(1); }   // ZH: 八千代靠左 | EN: assistant left
    return rowW;
}

void ChatWindow::clearRows()
{
    // ZH: 移除所有訊息列，保留尾端 stretch (在最後) | EN: remove all rows, keep the trailing stretch (last)
    while (m_vbox->count() > 1)
    {
        QLayoutItem *it = m_vbox->takeAt(0);
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }
}

void ChatWindow::scrollToBottomDeferred()
{
    // ZH: 佈局更新後才捲到底 (剛加入的列尺寸尚未算完) | EN: scroll after layout settles
    QTimer::singleShot(0, this, [this]() {
        QScrollBar *sb = m_scroll->verticalScrollBar();
        sb->setValue(sb->maximum());
    });
}

void ChatWindow::setMessages(const QList<ChatMsg> &msgs)
{
    m_msgs = msgs;
    m_noMoreOlder = false;
    m_loadingOlder = false;
    clearRows();
    for (const ChatMsg &m : m_msgs)
        m_vbox->insertWidget(m_vbox->count() - 1, makeBubbleRow(m));   // ZH: 插在 stretch 前 | EN: before stretch
    scrollToBottomDeferred();
}

void ChatWindow::appendMessages(const QList<ChatMsg> &msgs)
{
    if (msgs.isEmpty())
        return;
    QScrollBar *sb = m_scroll->verticalScrollBar();
    const bool wasAtBottom = sb->value() >= sb->maximum() - 8;

    const qint64 lastId = m_msgs.isEmpty() ? 0 : m_msgs.last().id;
    for (const ChatMsg &m : msgs)
    {
        if (m.id <= lastId)
            continue;
        m_msgs.append(m);
        m_vbox->insertWidget(m_vbox->count() - 1, makeBubbleRow(m));
    }
    if (wasAtBottom)
        scrollToBottomDeferred();   // ZH: 原本在底部才自動跟隨 | EN: auto-follow only if already at bottom
}

void ChatWindow::prependMessages(const QList<ChatMsg> &msgs)
{
    m_loadingOlder = false;
    // ZH: 濾掉已存在的 | EN: drop overlaps
    const qint64 firstId = m_msgs.isEmpty() ? 0 : m_msgs.first().id;
    QList<ChatMsg> older;
    for (const ChatMsg &m : msgs)
        if (firstId == 0 || m.id < firstId)
            older.append(m);
    if (older.isEmpty())
    {
        m_noMoreOlder = true;       // ZH: 沒有更舊了 | EN: reached the oldest
        return;
    }

    QScrollBar *sb = m_scroll->verticalScrollBar();
    const int prevMax = sb->maximum();
    const int prevVal = sb->value();

    // ZH: 插到最前 (由新到舊逐一 insert(0) → 最終升冪) | EN: insert at top so final order stays ascending
    for (int i = older.size() - 1; i >= 0; --i)
        m_vbox->insertWidget(0, makeBubbleRow(older[i]));
    m_msgs = older + m_msgs;

    // ZH: 內容在上方變長，補高度差讓視野停在原本那則 | EN: keep the viewport on the same message
    QTimer::singleShot(0, this, [this, prevMax, prevVal]() {
        QScrollBar *s = m_scroll->verticalScrollBar();
        s->setValue(s->maximum() - prevMax + prevVal);
    });
}

void ChatWindow::onSend()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;
    m_input->clear();
    // ZH: 不自行塞畫面——等 ChatStore 從 DB 讀回 (單一真相來源) | EN: no local echo; the DB poll is the source of truth
    emit sendRequested(text);
}

void ChatWindow::onScroll(int value)
{
    // ZH: 捲到頂 → 要求更舊 | EN: at top → request older
    if (value <= 0 && !m_loadingOlder && !m_noMoreOlder && !m_msgs.isEmpty())
    {
        m_loadingOlder = true;
        emit loadOlderRequested(m_msgs.first().id);
    }
}
