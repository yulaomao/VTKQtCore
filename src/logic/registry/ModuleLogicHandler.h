#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include "contracts/ModuleInvoke.h"
#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"
#include "communication/datasource/StateSample.h"

class IModuleInvoker;
class SceneGraph;

class ModuleLogicHandler : public QObject
{
    Q_OBJECT

public:
    explicit ModuleLogicHandler(const QString& moduleId, QObject* parent = nullptr);

    QString getModuleId() const;
    void setSceneGraph(SceneGraph* scene);
    SceneGraph* getSceneGraph() const;
    void setModuleInvoker(IModuleInvoker* moduleInvoker);

    // Optional logical connection label for transports that route module data.
    void    setDefaultConnectionId(const QString& connectionId);
    QString getDefaultConnectionId() const;

    virtual void handleAction(const UiAction& action) = 0;
    virtual ModuleInvokeResult handleModuleInvoke(const ModuleInvokeRequest& request)
    {
        Q_UNUSED(request);
        return ModuleInvokeResult::failure(
            QStringLiteral("invoke_not_supported"),
            QStringLiteral("Module '%1' does not support internal invocation").arg(m_moduleId));
    }

    // ---------------------------------------------------------------------------
    // Data dispatch — called by the socket message center via LogicRuntime.
    //
    // Polling data is delivered as one aggregated StateSample per module per poll
    // round via handleStateSample(). The sample data always carries a
    // QVariantMap under "values".
    //
    // handleSubscription(): called when a pub/sub message arrives on 'channel'.
    // Default implementation wraps the payload into a StateSample and forwards
    // to handleStateSample() so that existing subclasses continue to work
    // without any changes.
    // ---------------------------------------------------------------------------
    virtual void handleSubscription(const QString& channel, const QVariantMap& payload);

    // Polling data and subscription data ultimately converge here.
    virtual void handleStateSample(const StateSample& sample)
    {
        Q_UNUSED(sample);
    }

    virtual void onModuleActivated() {}
    virtual void onModuleDeactivated() {}
    virtual void onResync() {}

signals:
    void logicNotification(const LogicNotification& notification);

protected:
    bool playPromptAudioPreset(const QString& presetId);
    bool playPromptAudioSource(const QString& source);
    bool registerPromptAudioPreset(const QString& presetId, const QString& source);
    void stopPromptAudio();
    ModuleInvokeResult invokeModule(const QString& targetModule,
                                    const QString& method,
                                    const QVariantMap& payload = {});
    bool forwardModuleUiEventAction(const UiAction& action,
                                    const QString& sourceModule = QString());
    void emitModuleUiEvent(const QString& eventName,
                           const QVariantMap& payload = {},
                           const QString& sourceModule = QString(),
                           const QString& sourceActionId = QString(),
                           LogicNotification::TargetScope scope = LogicNotification::ModuleList,
                           const QStringList& targetModules = {});
    void emitInvokeFailureNotification(const ModuleInvokeResult& result,
                                       const QString& targetModule,
                                       const QString& sourceActionId = QString());

private:
    const QString m_moduleId;
    QString m_defaultConnectionId;
    SceneGraph* m_sceneGraph = nullptr;
    IModuleInvoker* m_moduleInvoker = nullptr;
};
