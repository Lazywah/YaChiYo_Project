#ifndef PETAUTOSTART_H
#define PETAUTOSTART_H

// ZH: 開機自啟動工具，封裝平台相關的登錄方式
//     Windows: 寫入 HKCU\Software\Microsoft\Windows\CurrentVersion\Run
//     其他平台: 目前為無動作 (待實作)
// EN: Auto-start helper, wraps platform-specific registration.
//     Windows: writes HKCU\...\Run; other platforms are no-ops for now.
namespace PetAutostart
{
    // ZH: 目前是否已設定開機自啟 | EN: Whether auto-start is currently enabled
    bool isEnabled();

    // ZH: 設定 / 取消開機自啟（指向目前執行檔）；回傳是否成功
    // EN: Enable / disable auto-start (points to the current executable); returns success
    bool setEnabled(bool enable);
}

#endif // PETAUTOSTART_H
