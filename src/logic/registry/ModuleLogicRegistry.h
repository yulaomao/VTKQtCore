#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class ModuleLogicHandler;

struct ModuleRuntimeDescriptor
{
    QString moduleId;
    QString displayName;
    QStringList aliases;
    QStringList capabilities;
    QStringList acceptedMessages;
    QStringList emittedMessages;
    QVariantMap uiMetadata;

    bool isValid() const { return !moduleId.trimmed().isEmpty(); }
};

class ModuleLogicRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ModuleLogicRegistry(QObject* parent = nullptr);

    void registerHandler(ModuleLogicHandler* handler);
    void unregisterHandler(const QString& moduleId);
    ModuleLogicHandler* getHandler(const QString& moduleId) const;
    QStringList getRegisteredModules() const;
    void registerDescriptor(const ModuleRuntimeDescriptor& descriptor);
    ModuleRuntimeDescriptor descriptorFor(const QString& moduleIdOrAlias) const;
    QString resolveModuleId(const QString& moduleIdOrAlias) const;
    bool contains(const QString& moduleIdOrAlias) const;

private:
    QMap<QString, ModuleLogicHandler*> m_handlers;
    QMap<QString, ModuleRuntimeDescriptor> m_descriptors;
    QMap<QString, QString> m_aliasToModuleId;
};
