#include "petsettings.h"

#include <QSettings>

// ZH: 統一的組織 / 應用名稱，決定設定的儲存位置 | EN: Organisation / application name → storage location
namespace {
    constexpr const char *kOrg = "YaChiYo";
    constexpr const char *kApp = "YaChiYo_Project";
}

PetSettingsData PetSettings::load()
{
    QSettings settings(kOrg, kApp);
    PetSettingsData d;  // ZH: 先帶入預設值，找不到 key 時自動沿用 | EN: Start from defaults; missing keys fall back

    d.walkSpeed        = settings.value("walkSpeed",        d.walkSpeed).toDouble();
    d.behaviorInterval = settings.value("behaviorInterval", d.behaviorInterval).toInt();
    d.petScale         = settings.value("petScale",         d.petScale).toDouble();
    d.gravity          = settings.value("gravity",          d.gravity).toDouble();
    d.gifSkin          = settings.value("gifSkin",          d.gifSkin).toBool();
    d.alwaysOnTop      = settings.value("alwaysOnTop",      d.alwaysOnTop).toBool();
    d.aiPrompt         = settings.value("aiPrompt",         d.aiPrompt).toString();

    return d;
}

void PetSettings::save(const PetSettingsData &d)
{
    QSettings settings(kOrg, kApp);

    settings.setValue("walkSpeed",        d.walkSpeed);
    settings.setValue("behaviorInterval", d.behaviorInterval);
    settings.setValue("petScale",         d.petScale);
    settings.setValue("gravity",          d.gravity);
    settings.setValue("gifSkin",          d.gifSkin);
    settings.setValue("alwaysOnTop",      d.alwaysOnTop);
    settings.setValue("aiPrompt",         d.aiPrompt);
}
