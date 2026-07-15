#include "aiclient.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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

void AIClient::generateSkin(const QImage &reference, const QList<QImage> &frames, const QString &prompt)
{
    if (busy) return;
    busy = true;
    pendingKind = Kind::Skin;

    auto toB64 = [](const QImage &img) {
        QByteArray ba;
        QBuffer buffer(&ba);
        img.save(&buffer, "PNG");
        return QString::fromLatin1(ba.toBase64());
    };

    // ZH: 把整套姿勢幀打包成 Base64 陣列 | EN: Pack all pose frames into a Base64 array
    QJsonArray arr;
    for (const QImage &img : frames)
        arr.append(toB64(img));

    QJsonObject json;
    json["frames"] = arr;
    json["prompt"] = prompt;
    if (!reference.isNull())                 // ZH: 角色身份參考圖 | EN: character identity reference
        json["reference"] = toB64(reference);

    QNetworkRequest request(QUrl("http://127.0.0.1:8000/generate_skin"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkManager->post(request, QJsonDocument(json).toJson());
}

void AIClient::onReplyFinished(QNetworkReply *reply)
{
    busy = false;
    pendingKind = Kind::None;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit errorOccurred(QString("AI Error: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();

    // ZH: 目前僅剩 /generate_skin 一種回應 | EN: only /generate_skin remains
    const QJsonArray results = res["results"].toArray();
    QList<QImage> imgs;
    for (const QJsonValue &v : results)
    {
        QByteArray ba = QByteArray::fromBase64(v.toString().toUtf8());
        QImage img;
        if (img.loadFromData(ba))
            imgs.append(img);
    }

    if (!imgs.isEmpty() && imgs.size() == results.size())
        emit skinReady(imgs);
    else
        emit errorOccurred("AI Error: Failed to decode skin frames");

    reply->deleteLater();
}
