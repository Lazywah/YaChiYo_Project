#ifndef VOICEBRIDGE_H
#define VOICEBRIDGE_H

#include <QObject>
#include <QHash>

class QTcpServer;
class QTcpSocket;

// ZH: 語音助手事件橋接 — 內嵌極簡 HTTP server，被動接收 localhost 的 POST /pet/event
//     刻意與 AIClient 分開：AIClient 是「主動請求」，VoiceBridge 是「被動接收」，職責不同。
// EN: Voice-assistant event bridge — embedded minimal HTTP server; passively receives POST /pet/event
//     on loopback. Deliberately separate from AIClient (active requester vs. passive receiver).
class VoiceBridge : public QObject
{
    Q_OBJECT

public:
    explicit VoiceBridge(QObject *parent = nullptr);
    ~VoiceBridge() override;

    // ZH: 在 127.0.0.1:port 開始監聽 (僅本機迴路，不對外)；成功回 true
    // EN: start listening on 127.0.0.1:port (loopback only, never exposed); true on success
    bool    start(quint16 port);
    void    stop();
    bool    isListening() const;
    quint16 port() const { return m_port; }

signals:
    // ZH: 對應事件協議的五個事件 | EN: the five protocol events
    void listening();                    // {"event":"listening"}
    void thinking();                     // {"event":"thinking"}
    void speaking(double durationSec);   // {"event":"speaking","duration_sec":3.2}
    void idle();                         // {"event":"idle"}
    void dndChanged(bool enabled);       // {"event":"dnd","enabled":true}

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void handleRequest(QTcpSocket *sock, const QByteArray &raw, int headerEnd);
    void dispatchEvent(const QByteArray &body);
    void respond(QTcpSocket *sock, int code, const char *reason);

    QTcpServer *m_server = nullptr;
    quint16     m_port   = 0;
    QHash<QTcpSocket*, QByteArray> m_buffers;   // ZH: 每連線累積緩衝 (分批到達) | EN: per-connection buffer (chunked arrival)
};

#endif // VOICEBRIDGE_H
