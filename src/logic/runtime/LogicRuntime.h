#pragma once

#include <QObject>
#include <QMap>
#include <QStringList>

#include "communication/datasource/StateSample.h"
#include "contracts/ModuleInvoke.h"
#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"
#include "IModuleInvoker.h"
#include "logic/runtime/ILogicRuntimePort.h"

class SceneGraph;
class ActiveModuleState;
class ModuleLogicRegistry;
class ModuleLogicHandler;
class IPromptAudioService;

class LogicRuntime : public QObject, public IModuleInvoker, public ILogicRuntimePort
{
    Q_OBJECT

public:
    explicit LogicRuntime(QObject* parent = nullptr);

    SceneGraph* getSceneGraph() const;
    ActiveModuleState* getActiveModuleState() const;
    ModuleLogicRegistry* getModuleLogicRegistry() const;
    ModuleInvokeResult invokeModule(const ModuleInvokeRequest& request) override;
    void sendAction(const UiAction& action) override;
    void initializeActiveModule(const QString& moduleId);
    void setPromptAudioService(IPromptAudioService* promptAudioService);
    bool hasPromptAudioService() const;
    bool playPromptAudioPreset(const QString& presetId) override;
    bool playPromptAudioSource(const QString& source) override;
    bool registerPromptAudioPreset(const QString& presetId, const QString& source) override;
    void stopPromptAudio() override;

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

    // ---------------------------------------------------------------------------
    // Data dispatch from socket message center
    // ---------------------------------------------------------------------------
    // Called once per module per poll cycle. 'module' may be "global" to
    // broadcast the same aggregated values map to ALL registered module handlers.
    void onModulePollBatch(const QString& module, const QVariantMap& values);

    // Called when a pub/sub message arrives for a module.  'module' may be
    // "global" to broadcast to ALL registered module handlers.
    void onModuleSubscription(const QString& module, const QString& channel,
                               const QVariantMap& payload);

signals:
    void logicNotification(const LogicNotification& notification);

private:
    bool acceptIncomingSequence(const QString& streamKey, const QVariantMap& payload,
                                const QString& actionDescription);
    void switchToModule(const QString& targetModule, const QString& sourceActionId);
    void routeToModuleHandler(const UiAction& action);

    SceneGraph* m_sceneGraph;
    ActiveModuleState* m_activeModuleState;
    ModuleLogicRegistry* m_moduleLogicRegistry;
    IPromptAudioService* m_promptAudioService = nullptr;
    QMap<QString, qint64> m_lastInboundSeqByStream;
};
