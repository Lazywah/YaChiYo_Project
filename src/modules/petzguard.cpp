#include "petzguard.h"

#include <QWidget>
#include <QTimer>

#ifdef Q_OS_WIN

#include <QFileInfo>
#include <QSet>
#include <windows.h>

namespace {
    constexpr int kPollMs = 300;    // ZH: 前景輪詢間隔 | EN: foreground poll interval

    // ZH: 由行程 id 取 exe 檔名 (小寫)；權限不足時回空字串 | EN: exe file name (lower-case) for a pid; empty if access is denied
    QString exeOfPid(DWORD pid)
    {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h)
            return {};
        wchar_t buf[MAX_PATH];
        DWORD   len = MAX_PATH;
        QString exe;
        if (QueryFullProcessImageNameW(h, 0, buf, &len))
            exe = QFileInfo(QString::fromWCharArray(buf, static_cast<int>(len))).fileName().toLower();
        CloseHandle(h);
        return exe;
    }

    // ZH: 是否為全螢幕視窗 — 覆蓋整個螢幕且無標題列 (排除桌面；最大化的一般視窗仍有標題列)
    // EN: fullscreen = covers the whole monitor and has no caption (desktop excluded; maximized windows keep their caption)
    bool isFullscreen(HWND hwnd)
    {
        if (hwnd == GetDesktopWindow() || hwnd == GetShellWindow())
            return false;
        wchar_t cls[32] = {};
        GetClassNameW(hwnd, cls, 32);
        if (wcscmp(cls, L"WorkerW") == 0 || wcscmp(cls, L"Progman") == 0)
            return false;
        if ((GetWindowLongW(hwnd, GWL_STYLE) & WS_CAPTION) == WS_CAPTION)
            return false;

        RECT r;
        if (!GetWindowRect(hwnd, &r))
            return false;
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (!mon || !GetMonitorInfoW(mon, &mi))
            return false;
        const RECT &m = mi.rcMonitor;
        return r.left <= m.left && r.top <= m.top && r.right >= m.right && r.bottom >= m.bottom;
    }

    BOOL CALLBACK collectApp(HWND hwnd, LPARAM lParam)
    {
        if (!IsWindowVisible(hwnd) || GetWindowTextLengthW(hwnd) == 0)
            return TRUE;
        if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
            return TRUE;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId())
            return TRUE;
        const QString exe = exeOfPid(pid);
        if (!exe.isEmpty())
            reinterpret_cast<QSet<QString> *>(lParam)->insert(exe);
        return TRUE;
    }
}

PetZGuard::PetZGuard(QWidget *pet, QObject *parent)
    : QObject(parent)
    , m_pet(pet)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kPollMs);
    connect(m_timer, &QTimer::timeout, this, &PetZGuard::poll);
}

void PetZGuard::setActive(bool on)
{
    m_active   = on;
    m_yielding = false;     // ZH: 置頂切換會重建視窗旗標，狀態歸零重新判斷 | EN: flag change rebuilds the window → re-evaluate
    m_lastFg   = 0;
    if (on)
        m_timer->start();
    else
        m_timer->stop();
}

void PetZGuard::setApps(const QStringList &exeNames)
{
    m_apps.clear();
    for (const QString &n : exeNames)
        m_apps << n.trimmed().toLower();
    m_lastFg = 0;           // ZH: 清單變了，下次輪詢重新判斷 | EN: list changed → re-evaluate next poll
}

void PetZGuard::setYieldFullscreen(bool on)
{
    m_yieldFullscreen = on;
    m_lastFg = 0;
}

void PetZGuard::poll()
{
    if (!m_active)
        return;
    if (!m_pet->isVisible())
    {
        // ZH: 隱藏後再顯示時 Qt 會重套置頂，屆時重新判斷 | EN: Qt re-applies topmost on re-show → re-evaluate then
        m_yielding = false;
        m_lastFg   = 0;
        return;
    }

    HWND fg = GetForegroundWindow();
    if (!fg)
        return;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == GetCurrentProcessId())
        return;             // ZH: 自己的視窗 (桌寵/設定中心/聊天室) — 維持現狀 | EN: own windows — keep current state

    const bool fgChanged = reinterpret_cast<quintptr>(fg) != m_lastFg;
    if (fgChanged)
    {
        m_lastFg    = reinterpret_cast<quintptr>(fg);
        m_lastFgExe = exeOfPid(pid);
    }

    // ZH: 全螢幕狀態可能在同一視窗內切換 (例如影片)，故每次都檢查 | EN: fullscreen can toggle within the same window, so check every poll
    const bool yield = m_apps.contains(m_lastFgExe) || (m_yieldFullscreen && isFullscreen(fg));

    if (yield)
    {
        if (!m_yielding || fgChanged)
        {
            // ZH: 插到前景視窗正後方；對方非置頂時，系統會一併拿掉桌寵的置頂狀態
            // EN: insert right behind the foreground window; if it isn't topmost, Windows drops our topmost status too
            SetWindowPos(reinterpret_cast<HWND>(m_pet->winId()), fg, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            m_yielding = true;
        }
    }
    else if (m_yielding)
    {
        restoreTopmost();
    }
}

void PetZGuard::restoreTopmost()
{
    SetWindowPos(reinterpret_cast<HWND>(m_pet->winId()), HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    m_yielding = false;
}

QStringList PetZGuard::runningWindowApps()
{
    QSet<QString> apps;
    EnumWindows(collectApp, reinterpret_cast<LPARAM>(&apps));
    QStringList list(apps.begin(), apps.end());
    list.sort();
    return list;
}

#else   // ZH: 其他平台：無動作 | EN: other platforms: no-op

PetZGuard::PetZGuard(QWidget *pet, QObject *parent)
    : QObject(parent), m_pet(pet), m_timer(new QTimer(this)) {}
void PetZGuard::setActive(bool on)                     { m_active = on; }
void PetZGuard::setApps(const QStringList &exeNames)   { m_apps = exeNames; }
void PetZGuard::setYieldFullscreen(bool on)            { m_yieldFullscreen = on; }
void PetZGuard::poll() {}
void PetZGuard::restoreTopmost() {}
QStringList PetZGuard::runningWindowApps()             { return {}; }

#endif
