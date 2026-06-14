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
    d.currentSkin      = settings.value("currentSkin",      d.currentSkin).toString();
    d.movementEnabled  = settings.value("movementEnabled",  d.movementEnabled).toBool();
    d.flyingEnabled    = settings.value("flyingEnabled",    d.flyingEnabled).toBool();
    d.hoveringEnabled  = settings.value("hoveringEnabled",  d.hoveringEnabled).toBool();
    d.live2dMode       = settings.value("live2dMode",       d.live2dMode).toBool();
    d.live2dModelDir   = settings.value("live2dModelDir",   d.live2dModelDir).toString();
    d.live2dWidth      = settings.value("live2dWidth",      d.live2dWidth).toInt();

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
    settings.setValue("currentSkin",      d.currentSkin);
    settings.setValue("movementEnabled",  d.movementEnabled);
    settings.setValue("flyingEnabled",    d.flyingEnabled);
    settings.setValue("hoveringEnabled",  d.hoveringEnabled);
    settings.setValue("live2dMode",       d.live2dMode);
    settings.setValue("live2dModelDir",   d.live2dModelDir);
    settings.setValue("live2dWidth",      d.live2dWidth);
}
