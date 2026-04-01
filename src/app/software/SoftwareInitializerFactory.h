#pragma once

#include <QString>
#include <QMap>
#include <functional>

class BaseSoftwareInitializer;
enum class RunMode;

class SoftwareInitializerFactory
{
public:
    static BaseSoftwareInitializer* create(const QString& softwareType, RunMode mode,
                                           QObject* parent = nullptr);

    static void registerInitializer(const QString& softwareType,
        std::function<BaseSoftwareInitializer*(const QString&, RunMode, QObject*)> creator);

private:
    static QMap<QString, std::function<BaseSoftwareInitializer*(const QString&, RunMode, QObject*)>>& registry();
};
