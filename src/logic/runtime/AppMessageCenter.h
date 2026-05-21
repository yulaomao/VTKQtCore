#pragma once

#include <QObject>

#include "communication/datasource/StateSample.h"
#include "contracts/AppMessage.h"
#include "contracts/LogicNotification.h"
#include "contracts/UiAction.h"

class ActiveModuleState;
class ModuleLogicRegistry;

class AppMessageCenter : public QObject
{
    Q_OBJECT

public:
    explicit AppMessageCenter(QObject* parent = nullptr);

    void setModuleRegistry(ModuleLogicRegistry* registry);
    void setActiveModuleState(ActiveModuleState* activeModuleState);

    bool publish(const AppMessage& message);
    bool dispatchUiIntent(const UiAction& action);
    bool dispatchStateSample(const StateSample& sample);
    bool dispatchModuleEvent(const QString& targetModule,
                             const QString& eventName,
                             const QVariantMap& payload,
                             const QString& sourceModule = QString(),
                             const QString& correlationId = QString());

signals:
    void messageAccepted(const AppMessage& message);
    void messageRejected(const AppMessage& message, const LogicNotification& notification);
    void logicNotification(const LogicNotification& notification);

private:
    bool routeUiAction(const UiAction& action, const AppMessage& sourceMessage);
    bool routeStateSample(const StateSample& sample, const AppMessage& sourceMessage);
    bool routeShellMessage(const AppMessage& message);
    bool reject(const AppMessage& message,
                const QString& errorCode,
                const QString& text,
                const QVariantMap& context = {});
    QString resolveTargetModule(const QString& targetName) const;

    ModuleLogicRegistry* m_registry = nullptr;
    ActiveModuleState* m_activeModuleState = nullptr;
};
