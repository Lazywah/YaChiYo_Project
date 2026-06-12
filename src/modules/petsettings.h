#ifndef PETSETTINGS_H
#define PETSETTINGS_H

#include <QString>

// ZH: 所有需要持久化的使用者偏好集中在此結構，欄位自帶預設值
// EN: All persisted user preferences live in this struct, each with its default
//
// ZH: 新增一個可儲存設定的步驟（共三處，皆為一行）：
//     1. 在此 struct 加一個欄位（含預設值）
//     2. 在 petsettings.cpp 的 load() 加一行讀取
//     3. 在 petsettings.cpp 的 save() 加一行寫入
// EN: To add a new persisted setting (three one-liners):
//     1. add a field here (with default)
//     2. add one read line in load() (petsettings.cpp)
//     3. add one write line in save() (petsettings.cpp)
struct PetSettingsData
{
    double  walkSpeed        = 2.0;     // ZH: 行走速度 | EN: Walk speed
    int     behaviorInterval = 5000;    // ZH: 決策間隔 ms | EN: Decision interval (ms)
    double  petScale         = 1.0;     // ZH: 桌寵縮放 | EN: Pet scale
    double  gravity          = 0.8;     // ZH: 重力強度 | EN: Gravity
    bool    gifSkin          = true;    // ZH: GIF 皮膚模式 (對應 petSkinType 1/0) | EN: GIF skin mode
    bool    alwaysOnTop      = true;    // ZH: 視窗置頂 | EN: Always on top
    QString aiPrompt         = "transform this character into a new style"; // ZH: AI 提示詞 | EN: AI prompt
    QString currentSkin      = "default";   // ZH: 當前皮膚 id (skins/ 下的資料夾名) | EN: current skin id (folder name under skins/)
};

// ZH: 設定的讀取與儲存（後端使用 QSettings，Windows 上存於登錄檔）
// EN: Load / save settings (backed by QSettings; stored in the registry on Windows)
namespace PetSettings
{
    PetSettingsData load();
    void            save(const PetSettingsData &data);
}

#endif // PETSETTINGS_H
