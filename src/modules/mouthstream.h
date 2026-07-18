#ifndef MOUTHSTREAM_H
#define MOUTHSTREAM_H

#include <QObject>

class QUdpSocket;

// ZH: 嘴型振幅串流接收 (V2) — UDP 被動接收外部語音來源送來的音量包絡，驅動 Live2D 真嘴型。
//     刻意與 VoiceBridge 分開：VoiceBridge 收「離散狀態事件」(TCP/HTTP)，
//     MouthStream 收「連續振幅」(UDP，掉幀無感、最新值有效)。
// EN: Mouth-amplitude stream receiver (V2) — passively receives a volume envelope over UDP from an
//     external voice source to drive the Live2D real mouth. Deliberately separate from VoiceBridge
//     (discrete state events over TCP/HTTP vs. a continuous amplitude stream over UDP, drop-tolerant).
class MouthStream : public QObject
{
    Q_OBJECT

public:
    explicit MouthStream(QObject *parent = nullptr);
    ~MouthStream() override;

    // ZH: 綁 127.0.0.1:port (僅本機迴路)，成功回 true | EN: bind 127.0.0.1:port (loopback only); true on success
    bool    start(quint16 port);
    void    stop();
    bool    isListening() const;
    quint16 port() const { return m_port; }

signals:
    // ZH: 振幅 0~1 (每收到一批封包發一次，取最新值) | EN: amplitude 0~1 (emitted per batch, freshest value)
    void mouthLevel(float level);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket = nullptr;
    quint16     m_port   = 0;
};

#endif // MOUTHSTREAM_H
