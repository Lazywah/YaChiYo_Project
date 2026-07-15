#include "voicebridge.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>

//===============================================================================================

VoiceBridge::VoiceBridge(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &VoiceBridge::onNewConnection);
}

VoiceBridge::~VoiceBridge()
{
    stop();
}

bool VoiceBridge::start(quint16 port)
{
    stop();
    // ZH: 僅綁本機迴路位址，不對區網/外網開放 | EN: bind loopback only — not reachable from the network
    if (!m_server->listen(QHostAddress::LocalHost, port))
    {
        qWarning("[VoiceBridge] listen failed on 127.0.0.1:%u — %s",
                 static_cast<unsigned>(port), qPrintable(m_server->errorString()));
        return false;
    }
    m_port = port;
    qInfo("[VoiceBridge] listening on 127.0.0.1:%u", static_cast<unsigned>(port));
    return true;
}

void VoiceBridge::stop()
{
    if (m_server->isListening())
        m_server->close();
    m_buffers.clear();
    m_port = 0;
}

bool VoiceBridge::isListening() const
{
    return m_server->isListening();
}

//===============================================================================================
// ZH: 連線與極簡 HTTP 解析 | EN: connection handling & minimal HTTP parsing
//===============================================================================================

void VoiceBridge::onNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket *sock = m_server->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead,    this, &VoiceBridge::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &VoiceBridge::onDisconnected);
    }
}

void VoiceBridge::onReadyRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock)
        return;

    QByteArray &buf = m_buffers[sock];
    buf += sock->readAll();

    // ZH: 等 header 收齊 (以空行分隔) | EN: wait for the header block (blank-line terminated)
    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0)
    {
        // ZH: 防異常超大請求塞爆記憶體 | EN: guard against oversized junk
        if (buf.size() > 64 * 1024)
        {
            respond(sock, 400, "Bad Request");
            sock->disconnectFromHost();
        }
        return;
    }

    // ZH: 解析 Content-Length，確認 body 收齊才處理 | EN: parse Content-Length; process only once the body is complete
    int contentLength = 0;
    const QByteArray header = buf.left(headerEnd);
    const QList<QByteArray> lines = header.split('\n');
    for (const QByteArray &line : lines)
    {
        const QByteArray l = line.trimmed();
        if (l.toLower().startsWith("content-length:"))
            contentLength = l.mid(l.indexOf(':') + 1).trimmed().toInt();
    }

    const int bodyStart = headerEnd + 4;
    if (buf.size() - bodyStart < contentLength)
        return;   // ZH: body 尚未收齊，等下一批 readyRead | EN: body incomplete — wait for more

    // ZH: 先把資料複製出來、移除本連線緩衝，再處理。
    //     handleRequest() 內的 disconnectFromHost() 可能同步觸發 onDisconnected() 去 remove(sock)，
    //     若仍持有 buf 參考將懸空 → 先 copy + remove 可避免 use-after-free。
    // EN: copy the bytes out and drop this connection's buffer BEFORE handling.
    //     handleRequest()'s disconnectFromHost() may synchronously fire onDisconnected() → remove(sock),
    //     which would dangle a held reference. Copy + remove first avoids the use-after-free.
    const QByteArray raw = buf;
    m_buffers.remove(sock);
    handleRequest(sock, raw, headerEnd);
}

void VoiceBridge::handleRequest(QTcpSocket *sock, const QByteArray &raw, int headerEnd)
{
    const int crlf = raw.indexOf("\r\n");
    const QByteArray requestLine = (crlf > 0) ? raw.left(crlf) : raw;
    const QByteArray body        = raw.mid(headerEnd + 4);

    // ZH: 只接受 POST /pet/event，其餘一律 404 | EN: accept POST /pet/event only; everything else 404s
    if (requestLine.startsWith("POST ") && requestLine.contains("/pet/event"))
    {
        dispatchEvent(body);
        respond(sock, 204, "No Content");
    }
    else
    {
        respond(sock, 404, "Not Found");
    }
    sock->disconnectFromHost();
}

void VoiceBridge::dispatchEvent(const QByteArray &body)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning("[VoiceBridge] invalid JSON body: %s", qPrintable(err.errorString()));
        return;
    }

    const QJsonObject o     = doc.object();
    const QString     event = o.value("event").toString();

    if      (event == "listening") emit listening();
    else if (event == "thinking")  emit thinking();
    else if (event == "speaking")  emit speaking(o.value("duration_sec").toDouble(0.0));
    else if (event == "idle")      emit idle();
    else if (event == "dnd")       emit dndChanged(o.value("enabled").toBool(false));
    else qWarning("[VoiceBridge] unknown event: %s", qPrintable(event));
}

void VoiceBridge::respond(QTcpSocket *sock, int code, const char *reason)
{
    QByteArray resp = "HTTP/1.1 " + QByteArray::number(code) + ' ' + reason + "\r\n"
                      "Content-Length: 0\r\n"
                      "Connection: close\r\n\r\n";
    sock->write(resp);
    sock->flush();
}

void VoiceBridge::onDisconnected()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock)
        return;
    m_buffers.remove(sock);
    sock->deleteLater();
}
