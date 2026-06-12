#include "petskin.h"

#include <QFile>
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
