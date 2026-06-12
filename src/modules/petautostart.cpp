#include "petautostart.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN

#include <QSettings>
#include <QDir>

namespace {
    // ZH: Windows 開機啟動項所在的登錄路徑 | EN: Windows startup registry path
    constexpr const char *kRunKey =
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    // ZH: 登錄值名稱（即此應用的識別）| EN: Registry value name (this app's identifier)
    constexpr const char *kAppName = "YaChiYo";
}

bool PetAutostart::isEnabled()
{
    QSettings run(kRunKey, QSettings::NativeFormat);
    return run.contains(kAppName);
}

bool PetAutostart::setEnabled(bool enable)
{
    QSettings run(kRunKey, QSettings::NativeFormat);

    if (enable)
    {
        // ZH: 指向目前執行檔（路徑加引號以容許空白）| EN: Point to current exe (quoted for spaces)
        QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(kAppName, '"' + exePath + '"');
    }
    else
    {
        run.remove(kAppName);
    }

    run.sync();
    return run.status() == QSettings::NoError;
}

#else   // ZH: 非 Windows 平台暫不支援 | EN: Not supported on non-Windows platforms yet

bool PetAutostart::isEnabled() { return false; }
bool PetAutostart::setEnabled(bool) { return false; }

#endif
