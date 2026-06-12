#ifndef AICLIENT_H
#define AICLIENT_H

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <QList>
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

    // ZH: 單張變身 (POST /transform) | EN: Single-image restyle (POST /transform)
    void sendRequest(const QPixmap &pixmap, const QString &prompt);

    // ZH: 批次生成整套皮膚幀 (POST /generate_skin)；reference 為角色身份參考圖 (可為空)
    // EN: Batch-generate a whole skin; 'reference' is the character identity image (may be null)
    void generateSkin(const QImage &reference, const QList<QImage> &frames, const QString &prompt);

signals:
    void resultReady(QPixmap result);         // ZH: /transform 成功 | EN: /transform success
    void skinReady(QList<QImage> results);    // ZH: /generate_skin 成功 (順序對應送出的幀) | EN: /generate_skin success (same order)
    void errorOccurred(QString errorMsg);     // ZH: 網路或解碼錯誤 | EN: Network or decode error

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    // ZH: 區分目前等待的回應屬於哪個端點 (busy 保證同時只有一個請求)
    // EN: Which endpoint the pending reply belongs to (busy ensures one at a time)
    enum class Kind { None, Transform, Skin };

    QNetworkAccessManager *networkManager;
    bool busy = false;
    Kind pendingKind = Kind::None;
};

#endif // AICLIENT_H
