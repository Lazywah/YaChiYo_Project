#ifndef PETZGUARD_H
#define PETZGUARD_H

#include <QObject>
#include <QStringList>

class QWidget;
class QTimer;

// ZH: 疊層讓位 (To-do 9) — 前景是「讓位清單」中的程式或全螢幕程式時，桌寵暫時退到該視窗後面；
//     切走後恢復置頂。直接以 Win32 SetWindowPos 調整 Z 順序，不動 Qt 視窗旗標 (不重建視窗、不閃爍)。
//     僅在「視窗置頂」開啟時運作；其他平台為無動作。
// EN: Z-order yield (To-do 9) — when the foreground window belongs to an app on the yield list (or is fullscreen),
//     the pet steps behind it; topmost is restored once focus moves elsewhere. Uses Win32 SetWindowPos directly
//     instead of Qt window flags (no window re-creation, no flicker). Active only while always-on-top is on.
class PetZGuard : public QObject
{
    Q_OBJECT

public:
    explicit PetZGuard(QWidget *pet, QObject *parent = nullptr);

    void setActive(bool on);                        // ZH: 置頂開關連動 | EN: follows the always-on-top toggle
    void setApps(const QStringList &exeNames);      // ZH: 讓位清單 (exe 檔名，不分大小寫) | EN: yield list (exe names, case-insensitive)
    void setYieldFullscreen(bool on);               // ZH: 全螢幕程式自動讓位 | EN: auto-yield to fullscreen apps

    // ZH: 目前有可見視窗的程式 exe 檔名 (設定中心挑選用，已排序去重) | EN: exe names of apps with visible windows (for the picker)
    static QStringList runningWindowApps();

private:
    void poll();
    void restoreTopmost();

    QWidget    *m_pet;
    QTimer     *m_timer;
    QStringList m_apps;                 // ZH: 皆為小寫 | EN: all lower-case
    bool        m_active          = false;
    bool        m_yieldFullscreen = true;
    bool        m_yielding        = false;   // ZH: 目前是否已退到後面 | EN: currently stepped behind
    quintptr    m_lastFg          = 0;       // ZH: 上次前景視窗 (換視窗才重查 exe) | EN: last foreground HWND (re-resolve exe only on change)
    QString     m_lastFgExe;
};

#endif // PETZGUARD_H
