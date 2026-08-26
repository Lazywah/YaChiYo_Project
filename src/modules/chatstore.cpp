#include "chatstore.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVariant>
#include <algorithm>

// ZH: 每個 ChatStore 用獨立連線名，避免與其他 QSQLITE 連線衝突 | EN: unique connection name per instance
static int s_connSeq = 0;

ChatStore::ChatStore(QObject *parent)
    : QObject(parent)
    , m_conn(QStringLiteral("chatstore_%1").arg(++s_connSeq))
{
}

ChatStore::~ChatStore()
{
    stopPolling();
    // ZH: 先確保無 QSqlQuery 存活再移除連線 (本類的 query 都在方法內作用域結束即銷毀)
    // EN: remove the connection after all QSqlQuery objects are gone (queries here are method-scoped)
    {
        QSqlDatabase db = QSqlDatabase::database(m_conn, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_conn);
}

bool ChatStore::open(const QString &dbPath)
{
    QString path = dbPath;
    if (path.isEmpty())
    {
        const QString local = qEnvironmentVariable("LOCALAPPDATA");
        if (local.isEmpty())
        {
            m_error = QStringLiteral("找不到 %LOCALAPPDATA%");
            return false;
        }
        path = local + QStringLiteral("/hermes/state.db");
    }
    path.replace('\\', '/');

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_conn);
    // ZH: 讀寫開啟 (才能看到 Hermes 即時寫進 -wal 的新訊息；純唯讀連線常只讀已 checkpoint 的主檔=看不到即時)。
    //     忙碌時等 2s。開啟後立刻 PRAGMA query_only=ON → 禁止任何資料寫入 (安全、等同唯讀語意，且不觸發 checkpoint)。
    // EN: open read-write so we see Hermes' live WAL commits (a pure read-only conn often only sees the checkpointed
    //     main file). Then PRAGMA query_only=ON forbids any data write (read-only semantics, no checkpoint).
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=2000"));
    db.setDatabaseName(path);

    if (!db.open())
    {
        m_error = QStringLiteral("無法開啟 %1：%2").arg(path, db.lastError().text());
        m_ok = false;
        return false;
    }

    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("PRAGMA query_only = ON"));   // ZH: 只讀不寫的保險 | EN: refuse writes
        // ZH: 以當下最大 id 當輪詢基準 (只推播開啟後的新訊息) | EN: seed poll cursor at current max id
        if (q.exec(QStringLiteral("SELECT COALESCE(MAX(id),0) FROM messages")) && q.next())
            m_lastSeenId = q.value(0).toLongLong();
    }
    m_ok = true;
    m_error.clear();
    return true;
}

QString ChatStore::currentSessionId()
{
    if (!m_ok)
        return QString();
    QSqlDatabase db = QSqlDatabase::database(m_conn, false);
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT id FROM sessions ORDER BY started_at DESC LIMIT 1")) && q.next())
        return q.value(0).toString();
    return QString();
}

// ZH: 共用查詢 — 只取 user/assistant 且 content 非空；ascending=true 時把 DESC 結果反轉為升冪。
// EN: shared query — only user/assistant with non-null content; reverse to ascending when requested.
QList<ChatMsg> ChatStore::runQuery(const QString &sql, const QVariantList &binds, bool ascending)
{
    QList<ChatMsg> out;
    if (!m_ok)
        return out;
    QSqlDatabase db = QSqlDatabase::database(m_conn, false);
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &b : binds)
        q.addBindValue(b);
    if (!q.exec())
    {
        m_error = q.lastError().text();
        return out;
    }
    while (q.next())
    {
        ChatMsg m;
        m.id      = q.value(0).toLongLong();
        m.role    = q.value(1).toString();
        m.content = q.value(2).toString();
        m.ts      = q.value(3).toDouble();
        out.append(m);
    }
    if (ascending)
        std::reverse(out.begin(), out.end());
    return out;
}

QList<ChatMsg> ChatStore::loadSession(const QString &sessionId, qint64 beforeId, int limit)
{
    // ZH: 取 id < beforeId 的最後 limit 則 (DESC 取後反轉為升冪) | EN: last `limit` msgs with id < beforeId, then ascending
    static const QString sql = QStringLiteral(
        "SELECT id, role, content, timestamp FROM messages "
        "WHERE session_id = ? AND id < ? AND role IN ('user','assistant') "
        "AND content IS NOT NULL AND trim(content) <> '' "
        "ORDER BY id DESC LIMIT ?");
    return runQuery(sql, {sessionId, static_cast<qlonglong>(beforeId), limit}, /*ascending=*/true);
}

void ChatStore::startPolling(int intervalMs)
{
    if (!m_ok)
        return;
    if (!m_timer)
    {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &ChatStore::poll);
    }
    m_timer->start(intervalMs);
}

void ChatStore::stopPolling()
{
    if (m_timer)
        m_timer->stop();
}

void ChatStore::poll()
{
    // ZH: 不限 session — 新訊息 id 必然更大，自然追隨當下對話 (含跨 session) | EN: any session; new ids are strictly larger
    static const QString sql = QStringLiteral(
        "SELECT id, role, content, timestamp FROM messages "
        "WHERE id > ? AND role IN ('user','assistant') "
        "AND content IS NOT NULL AND trim(content) <> '' "
        "ORDER BY id ASC");
    const QList<ChatMsg> msgs = runQuery(sql, {static_cast<qlonglong>(m_lastSeenId)}, /*ascending=*/false);
    if (msgs.isEmpty())
        return;
    m_lastSeenId = msgs.last().id;   // ZH: 升冪，最後一則 id 最大 | EN: ascending → last has max id
    emit newMessages(msgs);
}
