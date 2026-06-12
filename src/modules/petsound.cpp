#include "petsound.h"

#ifdef YACHIYO_HAS_MULTIMEDIA

#include <QSoundEffect>
#include <QCoreApplication>
#include <QFile>
#include <QUrl>
#include <QtGlobal>

PetSound::PetSound(QObject *parent)
    : QObject(parent)
{
    sfxLand    = makeEffect("land.wav");
    sfxWallHit = makeEffect("wall.wav");
    sfxGrab    = makeEffect("grab.wav");
    sfxRelease = makeEffect("release.wav");
}

void PetSound::setVolume(double v)
{
    volume = qBound(0.0, v, 1.0);
    for (QSoundEffect *e : { sfxLand, sfxWallHit, sfxGrab, sfxRelease })
        if (e) e->setVolume(volume);
}

QSoundEffect *PetSound::makeEffect(const QString &fileName)
{
    QSoundEffect *effect = new QSoundEffect(this);

    // ZH: 從執行檔同層的 sounds/ 目錄載入 | EN: Load from sounds/ next to the executable
    QString path = QCoreApplication::applicationDirPath() + "/sounds/" + fileName;
    if (QFile::exists(path))
    {
        effect->setSource(QUrl::fromLocalFile(path));
        effect->setVolume(volume);
    }
    // ZH: 檔案不存在時保留未載入的物件，playSafe 會跳過 | EN: Keep unloaded object if file is absent; playSafe skips it
    return effect;
}

void PetSound::playSafe(QSoundEffect *effect)
{
    if (!enabled || !effect)
        return;
    if (effect->status() != QSoundEffect::Ready)   // ZH: 未成功載入則跳過 | EN: Skip if not loaded
        return;
    effect->play();
}

#else   // ZH: 無 Qt Multimedia — 全部降級為無聲 | EN: No Qt Multimedia — everything is a no-op

PetSound::PetSound(QObject *parent) : QObject(parent) {}
void PetSound::setVolume(double v) { volume = v; }
QSoundEffect *PetSound::makeEffect(const QString &) { return nullptr; }
void PetSound::playSafe(QSoundEffect *) {}

#endif

//===============================================================================================
// ZH: 與是否有 Multimedia 無關的共用實作 | EN: Shared implementation regardless of Multimedia
//===============================================================================================

void PetSound::setEnabled(bool on) { enabled = on; }
bool PetSound::isEnabled() const   { return enabled; }

void PetSound::playLand()    { playSafe(sfxLand); }
void PetSound::playWallHit() { playSafe(sfxWallHit); }
void PetSound::playGrab()    { playSafe(sfxGrab); }
void PetSound::playRelease() { playSafe(sfxRelease); }
