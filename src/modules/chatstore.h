#ifndef CHATSTORE_H
#define CHATSTORE_H

#include <QObject>
#include <QList>
#include <QString>

class QTimer;

// ZH: 一則對話 (從 Hermes 的 state.db 讀出) | EN: one conversation turn read from Hermes' state.db
struct ChatMsg
{
    qint64  id = 0;
    QString role;      // ZH: "user" | "assistant" | EN: same
    QString content;
    double  ts = 0.0;  // ZH: unix 秒 | EN: unix seconds
};

// ZH: Hermes 對話儲存的「唯讀尾隨」——直接讀 %LOCALAPPDATA%/hermes/state.db 的 messages 表。
//     語音與打字都由同一個 Hermes 寫進同一張表，故此處自動含語音對話。Path B：只讀、絕不寫。
// EN: Read-only tail of Hermes' conversation store — reads the messages table in state.db directly.
//     Voice and typed turns are written by the same Hermes into the same table, so voice convos are included.
//     Path B: read-only, never writes.
class ChatStore : public QObject
{
    Q_OBJECT

public:
    explicit ChatStore(QObject *parent = nullptr);
    ~ChatStore() override;

    // ZH: 開啟 DB (唯讀, WAL 不擋 Hermes 寫入)。dbPath 空=預設 %LOCALAPPDATA%/hermes/state.db。成功回 true。
    // EN: open the DB read-only (WAL lets Hermes keep writing). Empty dbPath = default path. true on success.
    bool    open(const QString &dbPath = QString());
    bool    isOpen() const { return m_ok; }
    QString error() const { return m_error; }

    // ZH: 最新的 session id (依 started_at) | EN: latest session id (by started_at)
    QString currentSessionId();

    // ZH: 載入某 session 中 id < beforeId 的最後 limit 則 (升冪回傳，供捲動載入更舊)。beforeId 給大值=最新一批。
    // EN: load the last `limit` msgs of a session with id < beforeId (ascending; for scroll-back). Large beforeId = latest batch.
    QList<ChatMsg> loadSession(const QString &sessionId, qint64 beforeId, int limit);

    // ZH: 開始輪詢新訊息 (id > 已見過的最大 id，不限 session；追隨 Hermes 當下對話) | EN: poll for new msgs (any session)
    void    startPolling(int intervalMs = 500);
    void    stopPolling();

signals:
    // ZH: 有新訊息 (升冪) | EN: new messages arrived (ascending)
    void    newMessages(const QList<ChatMsg> &msgs);

private:
    QList<ChatMsg> runQuery(const QString &sql, const QVariantList &binds, bool ascending);
    void    poll();

    QString  m_conn;                // ZH: 唯一連線名 | EN: unique connection name
    bool     m_ok = false;
    QString  m_error;
    qint64   m_lastSeenId = 0;      // ZH: 輪詢基準 (開啟時=當下最大 id) | EN: poll cursor (max id at open)
    QTimer  *m_timer = nullptr;
};

#endif // CHATSTORE_H
