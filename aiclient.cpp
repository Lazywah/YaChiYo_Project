#include "aiclient.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

AIClient::AIClient(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
{
    connect(networkManager, &QNetworkAccessManager::finished, this, &AIClient::onReplyFinished);
}

bool AIClient::isBusy() const
{
    return busy;
}

void AIClient::sendRequest(const QPixmap &pixmap, const QString &prompt)
{
    if (busy) return;
    busy = true;

    // ZH: 將圖片轉為 Base64 | EN: Encode pixmap to Base64
    QByteArray ba;
    QBuffer buffer(&ba);
    pixmap.save(&buffer, "PNG");

    QJsonObject json;
    json["image"]  = QString::fromLatin1(ba.toBase64());
    json["prompt"] = prompt;

    QNetworkRequest request(QUrl("http://127.0.0.1:8000/transform"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkManager->post(request, QJsonDocument(json).toJson());
}

void AIClient::onReplyFinished(QNetworkReply *reply)
{
    busy = false;

    if (reply->error() == QNetworkReply::NoError)
    {
        QJsonObject res  = QJsonDocument::fromJson(reply->readAll()).object();
        QByteArray resBa = QByteArray::fromBase64(res["result"].toString().toUtf8());

        QPixmap result;
        if (result.loadFromData(resBa))
            emit resultReady(result);
        else
            emit errorOccurred("AI Error: Failed to decode response image");
    }
    else
    {
        emit errorOccurred(QString("AI Error: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
}
