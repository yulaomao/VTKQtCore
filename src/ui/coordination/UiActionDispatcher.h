#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "contracts/UiAction.h"

class ILogicGateway;

class UiActionDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit UiActionDispatcher(const QString& sourceModule,
                                ILogicGateway* gateway,
                                QObject* parent = nullptr);

    QString getSourceModule() const;

    void sendAction(const UiAction& action);
    void sendCommand(const QString& command,
                     const QVariantMap& payload = {});
    void sendTargetedCommand(const QString& targetModule,
                             const QString& command,
                             const QVariantMap& payload = {});
    void sendModuleUiEvent(const QString& targetModule,
                           const QString& eventName,
                           const QVariantMap& payload = {});
    void requestModuleSwitch(const QString& targetModule);
    void requestResync(const QString& reason) const;

signals:
    void actionDispatched(const UiAction& action);

private:
    QString m_sourceModule;
    ILogicGateway* m_gateway = nullptr;
};