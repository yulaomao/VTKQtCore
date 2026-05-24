#pragma once

#include <functional>

#include <QString>
#include <QStringList>

struct ProductDefinition
{
    QString productId;
    QString displayName;
    QString softwareType;
    QString styleTheme;
    QString organizationName;
    QString versionString;
    QString defaultProfileResourcePath;
    QString windowIconResourcePath;
    QStringList defaultEnabledModules;
    QStringList moduleDisplayOrder;
    QString initialModule;
    std::function<void()> registerModulePacks;
};