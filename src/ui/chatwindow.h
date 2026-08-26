#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QList>

#include "chatstore.h"   // ZH: ChatMsg | EN: ChatMsg

class QScrollArea;
class QVBoxLayout;
class QLineEdit;
class QPushButton;

// ZH: 八千代聊天室 — 一般視窗。QScrollArea + 每則一個圓角泡泡 (QLabel)。顯示 Hermes 對話 (含語音)、
//     打字送出、捲頂載入更舊。只負責顯示+收集輸入；讀取交給 ChatStore、送字交給 MainWindow。
// EN: YaChiYo chat room — a normal window. QScrollArea + one rounded bubble (QLabel) per message.
//     Shows Hermes conversation (incl. voice), lets you type, loads older on scroll-to-top.
class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);

    void setMessages(const QList<ChatMsg> &msgs);      // ZH: 首次載入 (取代全部，捲到底) | EN: initial (replace, scroll bottom)
    void appendMessages(const QList<ChatMsg> &msgs);   // ZH: 尾端追加新訊息 | EN: append new at bottom
    void prependMessages(const QList<ChatMsg> &msgs);  // ZH: 頭端插入更舊 (空=沒有更舊了) | EN: prepend older (empty = no more)

signals:
    void sendRequested(const QString &text);
    void loadOlderRequested(qint64 beforeId);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onSend();
    void onScroll(int value);

private:
    QWidget *makeBubbleRow(const ChatMsg &m) const;    // ZH: 造一列 (泡泡+左右對齊) | EN: build a row (bubble + side align)
    int      bubbleMaxWidth() const;
    void     clearRows();
    void     scrollToBottomDeferred();

    QScrollArea *m_scroll    = nullptr;
    QWidget     *m_container = nullptr;
    QVBoxLayout *m_vbox      = nullptr;   // ZH: 訊息列 + 尾端 stretch | EN: message rows + trailing stretch
    QLineEdit   *m_input     = nullptr;
    QPushButton *m_send      = nullptr;

    QList<ChatMsg> m_msgs;                // ZH: 升冪 (依 id) | EN: ascending by id
    bool m_loadingOlder = false;
    bool m_noMoreOlder  = false;

    // ZH: 依系統主題選的泡泡配色 | EN: theme-aware bubble colors
    QString m_userBg, m_userFg, m_asstBg, m_asstFg;
};

#endif // CHATWINDOW_H
