#ifndef PETSOUND_H
#define PETSOUND_H

#include <QObject>
#include <QString>

class QSoundEffect;

// ZH: 音效模組，封裝各互動事件的音效播放
// EN: Sound module — encapsulates sound playback for interaction events
//
// ZH: 音效檔放在執行檔同層的 sounds/ 目錄，檔名固定：
//     land.wav / wall.wav / grab.wav / release.wav
//     檔案不存在時對應的 play 呼叫會靜默跳過，不會崩潰
// EN: .wav files live in a sounds/ folder next to the executable with fixed
//     names. Missing files are silently skipped (no crash).
class PetSound : public QObject
{
    Q_OBJECT

public:
    explicit PetSound(QObject *parent = nullptr);

    // ZH: 全域開關（由 PetConfig 控制）| EN: Master switch (controlled by PetConfig)
    void setEnabled(bool on);
    bool isEnabled() const;

    // ZH: 0.0 ~ 1.0 音量 | EN: Volume 0.0 ~ 1.0
    void setVolume(double volume);

    // ZH: 互動事件音效 | EN: Interaction event sounds
    void playLand();        // ZH: 落地 | EN: Landing
    void playWallHit();     // ZH: 碰牆 | EN: Wall hit
    void playGrab();        // ZH: 抓取 | EN: Grabbed
    void playRelease();     // ZH: 釋放 | EN: Released

private:
    // ZH: 建立並載入單一音效（檔案不存在時回傳已建立但未載入的物件）
    // EN: Create and load one effect (returns object even if file is absent)
    QSoundEffect *makeEffect(const QString &fileName);

    // ZH: 安全播放（未啟用或未載入則跳過）| EN: Safe play (skip if disabled or not loaded)
    void playSafe(QSoundEffect *effect);

    bool   enabled = true;
    double volume  = 0.8;

    QSoundEffect *sfxLand    = nullptr;
    QSoundEffect *sfxWallHit = nullptr;
    QSoundEffect *sfxGrab    = nullptr;
    QSoundEffect *sfxRelease = nullptr;
};

#endif // PETSOUND_H
