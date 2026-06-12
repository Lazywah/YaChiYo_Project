#ifndef PETSKIN_H
#define PETSKIN_H

#include <QString>
#include <QMap>
#include <QList>

// ZH: 皮膚模組 — 從一個皮膚目錄 (含 skin.json) 載入設定，提供各狀態的圖片路徑
//     皮膚目錄可為 qrc 資源 (":/res/skins/default") 或檔案系統路徑
// EN: Skin module — loads a skin directory (with skin.json) and resolves image
//     paths per state. The directory may be a qrc resource or a filesystem path.
//
// ZH: 一套皮膚的檔案慣例 | EN: File convention within a skin:
//     png   : <State>.png            (e.g. Standing.png)
//     gif   : <State>.gif            (e.g. Captured.gif)
//     frames: <State>/<State>-<n>.png (e.g. Walking/Walking-1.png, 1-based)
class PetSkin
{
public:
    // ZH: 單一狀態的呈現設定 | EN: Per-state presentation config
    struct StateInfo
    {
        enum Kind { Png, Frames, Gif };
        Kind    kind     = Png;
        int     frames   = 1;       // ZH: kind==Frames 時的總幀數 | EN: total frames when kind==Frames
        int     interval = 150;     // ZH: kind==Frames 時的播放間隔 ms | EN: frame interval (ms)
        QString fallback;           // ZH: 找不到圖時退回的狀態名 | EN: fallback state name when missing
    };

    // ZH: 可用皮膚的摘要 (供設定中心列表) | EN: Summary of an available skin (for the settings list)
    struct SkinEntry
    {
        QString id;     // ZH: 資料夾名稱 (唯一識別) | EN: folder name (unique id)
        QString dir;    // ZH: 可載入的完整路徑 | EN: full loadable path
        QString name;   // ZH: 顯示名稱 (來自 skin.json) | EN: display name (from skin.json)
    };

    // ZH: 掃描內建 (:/res/skins) 與使用者 (<執行檔>/skins) 目錄，列出所有含 skin.json 的皮膚
    //     同名時使用者皮膚覆蓋內建 | EN: Discover built-in and user skins; user skins override built-in by id
    static QList<SkinEntry> available();

    // ZH: 載入皮膚目錄 (解析 skin.json)；回傳是否成功 | EN: Load a skin dir (parse skin.json); returns success
    bool load(const QString &dir);

    bool    isLoaded() const { return loaded; }
    QString name()     const { return skinName; }
    QString dir()      const { return baseDir; }
    int     scale()    const { return baseScale; }   // ZH: 基準像素大小 (預設 250) | EN: base pixel size (default 250)

    bool      hasState(const QString &state) const;
    StateInfo state(const QString &state) const;     // ZH: 找不到時回傳預設 Png | EN: returns a default Png info if absent

    // ZH: 路徑解析 (檔案不存在時回傳空字串) | EN: Path resolution (empty string when the file is absent)
    QString pngPath(const QString &state) const;
    QString gifPath(const QString &state) const;
    QString framePath(const QString &state, int frame) const;   // ZH: frame 為 1-based | EN: frame is 1-based

    // ZH: 取得狀態的退回目標 (json 未指定時退回 "Standing") | EN: Fallback state ("Standing" if unspecified)
    QString fallbackState(const QString &state) const;

private:
    bool                     loaded   = false;
    QString                  baseDir;
    QString                  skinName;
    int                      baseScale = 250;
    QMap<QString, StateInfo> states;
};

#endif // PETSKIN_H
