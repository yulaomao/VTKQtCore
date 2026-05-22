#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "contracts/UiAction.h"

class ILogicRuntimePort;

class UiActionDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit UiActionDispatcher(const QString& sourceModule,
                                ILogicRuntimePort* runtimePort,
                                QObject* parent = nullptr);

    QString getSourceModule() const;

    void sendAction(const UiAction& action);
    void sendCommand(const QString& command,
                     const QVariantMap& payload = {});
    void sendTargetedCommand(const QString& targetModule,
                             const QString& command,
                             const QVariantMap& payload = {});
    void sendToTarget(const QString& targetName,
                      const QString& command,
                      const QVariantMap& payload = {});
    void sendModuleUiEvent(const QString& targetModule,
                           const QString& eventName,
                           const QVariantMap& payload = {});
    void requestModuleSwitch(const QString& targetModule);
    void requestResync(const QString& reason);

signals:
    void actionDispatched(const UiAction& action);
    void resyncRequested(const QString& reason);

private:
    QString m_sourceModule;
    ILogicRuntimePort* m_runtimePort = nullptr;
};
