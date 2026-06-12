#include "petskin.h"

#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace {
    // ZH: 去除結尾斜線，統一路徑格式 | EN: Strip trailing slash to normalise the path
    QString normalize(QString dir)
    {
        while (dir.endsWith('/'))
            dir.chop(1);
        return dir;
    }
}

QList<PetSkin::SkinEntry> PetSkin::available()
{
    // ZH: 內建在前、使用者在後；同 id 時後者覆蓋前者 | EN: built-in first, user second; same id → user overrides
    const QStringList roots = {
        QStringLiteral(":/res/skins"),
        QCoreApplication::applicationDirPath() + "/skins"
    };

    QMap<QString, SkinEntry> found;     // ZH: 以 id 為鍵，自動依 id 排序 | EN: keyed by id, sorted by id
    for (const QString &root : roots)
    {
        QDir dir(root);
        if (!dir.exists())
            continue;

        const QStringList subs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &id : subs)
        {
            const QString sdir = root + "/" + id;
            if (!QFile::exists(sdir + "/skin.json"))
                continue;   // ZH: 無 skin.json 不算有效皮膚 | EN: not a valid skin without skin.json

            PetSkin probe;
            const QString name = probe.load(sdir) ? probe.name() : id;
            found.insert(id, { id, sdir, name });
        }
    }

    return found.values();
}

bool PetSkin::load(const QString &dir)
{
    loaded    = false;
    baseDir   = normalize(dir);
    skinName  = baseDir;
    baseScale = 250;
    states.clear();

    QFile file(baseDir + "/skin.json");
    if (!file.open(QIODevice::ReadOnly))
        return false;   // ZH: 無 skin.json 仍保留 baseDir，可作降級的純 png 載入 | EN: keep baseDir for degraded png loading

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    QJsonObject root = doc.object();
    skinName  = root.value("name").toString(baseDir);
    baseScale = root.value("scale").toInt(250);

    QJsonObject statesObj = root.value("states").toObject();
    for (auto it = statesObj.begin(); it != statesObj.end(); ++it)
    {
        QJsonObject so = it.value().toObject();
        StateInfo info;

        const QString type = so.value("type").toString("png");
        if (type == "frames")      info.kind = StateInfo::Frames;
        else if (type == "gif")    info.kind = StateInfo::Gif;
        else                       info.kind = StateInfo::Png;

        info.frames   = so.value("frames").toInt(1);
        info.interval = so.value("interval").toInt(150);
        info.fallback = so.value("fallback").toString();

        states.insert(it.key(), info);
    }

    loaded = true;
    return true;
}

bool PetSkin::hasState(const QString &state) const
{
    return states.contains(state);
}

PetSkin::StateInfo PetSkin::state(const QString &state) const
{
    return states.value(state, StateInfo{});   // ZH: 未定義時回傳預設 Png | EN: default Png when undefined
}

QString PetSkin::pngPath(const QString &state) const
{
    QString path = baseDir + "/" + state + ".png";
    return QFile::exists(path) ? path : QString();
}

QString PetSkin::gifPath(const QString &state) const
{
    QString path = baseDir + "/" + state + ".gif";
    return QFile::exists(path) ? path : QString();
}

QString PetSkin::framePath(const QString &state, int frame) const
{
    QString path = QString("%1/%2/%2-%3.png").arg(baseDir, state).arg(frame);
    return QFile::exists(path) ? path : QString();
}

QString PetSkin::fallbackState(const QString &state) const
{
    const QString fb = states.value(state).fallback;
    return fb.isEmpty() ? QStringLiteral("Standing") : fb;
}
