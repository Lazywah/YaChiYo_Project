#ifndef AICLIENT_H
#define AICLIENT_H

#include <QObject>
#include <QPixmap>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// ZH: AI 通訊模組，封裝 HTTP POST 至 Python 後端的完整流程
// EN: AI client module — encapsulates HTTP POST to the Python backend
class AIClient : public QObject
{
    Q_OBJECT

public:
    explicit AIClient(QObject *parent = nullptr);

    bool isBusy() const;

    // ZH: 發送圖生圖請求 | EN: Send image-to-image request
    void sendRequest(const QPixmap &pixmap, const QString &prompt);

signals:
    void resultReady(QPixmap result);       // ZH: 成功回傳結果圖 | EN: Successfully received result image
    void errorOccurred(QString errorMsg);   // ZH: 網路或解碼錯誤 | EN: Network or decode error

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *networkManager;
    bool busy = false;
};

#endif // AICLIENT_H
