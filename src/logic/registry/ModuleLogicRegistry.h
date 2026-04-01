#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class ModuleLogicHandler;

class ModuleLogicRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ModuleLogicRegistry(QObject* parent = nullptr);

    void registerHandler(ModuleLogicHandler* handler);
    void unregisterHandler(const QString& moduleId);
    ModuleLogicHandler* getHandler(const QString& moduleId) const;
    QStringList getRegisteredModules() const;

private:
    QMap<QString, ModuleLogicHandler*> m_handlers;
};
