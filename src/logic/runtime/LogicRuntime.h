#pragma once

#include <QObject>
#include <QMap>

#include "communication/datasource/StateSample.h"
#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class SceneGraph;
class WorkflowStateMachine;
class ModuleLogicRegistry;
class ModuleLogicHandler;

class LogicRuntime : public QObject
{
    Q_OBJECT

public:
    explicit LogicRuntime(QObject* parent = nullptr);

    SceneGraph* getSceneGraph() const;
    WorkflowStateMachine* getWorkflowStateMachine() const;
    ModuleLogicRegistry* getModuleLogicRegistry() const;

    void registerModuleHandler(ModuleLogicHandler* handler);

public slots:
    void onActionReceived(const UiAction& action);
    void onControlMessageReceived(const QString& module, const QVariantMap& payload);
    void onServerCommandReceived(const QString& commandType, const QVariantMap& payload);
    void onStateSampleReceived(const StateSample& sample);
    void onCommunicationError(const QString& source, const QString& errorMessage);
    void onCommunicationIssue(const QString& source, const QString& severity,
                              const QString& errorCode, const QString& errorMessage,
                              const QVariantMap& context);
    void onCommunicationHealthChanged(const QVariantMap& healthSnapshot);
    void onConnectionStateChanged(const QString& state);
    void requestResync(const QString& reason);

signals:
    void logicNotification(const LogicNotification& notification);

private:
    bool acceptIncomingSequence(const QString& streamKey, const QVariantMap& payload,
                                const QString& actionDescription);
    void switchToModule(const QString& targetModule, const QString& sourceActionId);
    void routeToModuleHandler(const UiAction& action);

    SceneGraph* m_sceneGraph;
    WorkflowStateMachine* m_workflowStateMachine;
    ModuleLogicRegistry* m_moduleLogicRegistry;
    QMap<QString, qint64> m_lastInboundSeqByStream;
};
