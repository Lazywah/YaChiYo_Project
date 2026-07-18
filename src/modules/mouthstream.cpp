#include "mouthstream.h"

#include <QUdpSocket>
#include <QHostAddress>
#include <QDebug>

//===============================================================================================

MouthStream::MouthStream(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &MouthStream::onReadyRead);
}

MouthStream::~MouthStream()
{
    stop();
}

bool MouthStream::start(quint16 port)
{
    stop();
    // ZH: 僅綁本機迴路位址，不對區網/外網開放 | EN: bind loopback only — not reachable from the network
    if (!m_socket->bind(QHostAddress::LocalHost, port))
    {
        qWarning("[MouthStream] bind failed on 127.0.0.1:%u — %s",
                 static_cast<unsigned>(port), qPrintable(m_socket->errorString()));
        return false;
    }
    m_port = port;
    qInfo("[MouthStream] listening on 127.0.0.1:%u (UDP)", static_cast<unsigned>(port));
    return true;
}

void MouthStream::stop()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
    m_port = 0;
}

bool MouthStream::isListening() const
{
    return m_socket->state() == QAbstractSocket::BoundState;
}

void MouthStream::onReadyRead()
{
    // ZH: 讀完所有待處理封包，只保留「最後一個」的振幅 — 嘴型只需最新值，積壓的舊幀直接丟棄，
    //     避免延遲累積 (振幅串流不需重播歷史)。封包協議：1 byte，amp = byte/255。
    // EN: drain all pending datagrams and keep only the latest amplitude — the mouth only needs the
    //     freshest value; stale frames are discarded to avoid lag build-up. Protocol: 1 byte, amp = byte/255.
    int latest = -1;
    while (m_socket->hasPendingDatagrams())
    {
        QByteArray buf;
        buf.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        const qint64 n = m_socket->readDatagram(buf.data(), buf.size());
        if (n > 0)
            latest = static_cast<unsigned char>(buf.at(static_cast<int>(n) - 1));   // ZH: 取封包最後一個 byte | EN: last byte of the datagram
    }
    if (latest >= 0)
        emit mouthLevel(static_cast<float>(latest) / 255.0f);
}
