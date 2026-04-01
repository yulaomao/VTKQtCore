#include "DefaultSoftwareInitializer.h"

#include "ModuleUiAssemblers.h"
#include "SoftwareInitializerFactory.h"
#include "LogicRuntime.h"
#include "communication/hub/CommunicationHub.h"
#include "communication/datasource/PollingTask.h"
#include "communication/datasource/SubscriptionSource.h"

#include "ParamsModuleLogicHandler.h"
#include "PointPickModuleLogicHandler.h"
#include "PlanningModuleLogicHandler.h"
#include "NavigationModuleLogicHandler.h"

namespace {

QString defaultSubscriptionChannel(const QString& moduleId)
{
    return QStringLiteral("state.%1").arg(moduleId);
}

QString defaultControlRoutingChannel()
{
    return QStringLiteral("control.downstream");
}

QString defaultControlPublishChannel()
{
    return QStringLiteral("control.upstream");
}

QString defaultAckChannel()
{
    return QStringLiteral("control.ack");
}

QString defaultPollingKey(const QString& moduleId)
{
    return QStringLiteral("state.%1.latest").arg(moduleId);
}

QVariantMap communicationProfile(const QVariantMap& profile)
{
    return profile.value(QStringLiteral("communication")).toMap();
}

QStringList stringListFromVariant(const QVariant& value)
{
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }

    QStringList result;
    const QVariantList list = value.toList();
    for (const QVariant& item : list) {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty()) {
            result.append(text);
        }
    }
    return result;
}

QStringList routingChannelsFromProfile(const QVariantMap& profile)
{
    const QStringList channels = stringListFromVariant(
        communicationProfile(profile).value(QStringLiteral("routingChannels")));
    return channels.isEmpty() ? QStringList{defaultControlRoutingChannel()} : channels;
}

QString outboundControlChannelFromProfile(const QVariantMap& profile)
{
    return communicationProfile(profile).value(
        QStringLiteral("controlPublishChannel")).toString(defaultControlPublishChannel());
}

QString ackChannelFromProfile(const QVariantMap& profile)
{
    return communicationProfile(profile).value(
        QStringLiteral("ackChannel")).toString(defaultAckChannel());
}

QString subscriptionChannelForModule(const QVariantMap& profile, const QString& moduleId)
{
    const QVariantMap channels = communicationProfile(profile).value(
        QStringLiteral("subscriptionChannels")).toMap();
    return channels.value(moduleId).toString(defaultSubscriptionChannel(moduleId));
}

QString pollingKeyForModule(const QVariantMap& profile, const QString& moduleId)
{
    const QVariantMap keys = communicationProfile(profile).value(
        QStringLiteral("pollingKeys")).toMap();
    return keys.value(moduleId).toString(defaultPollingKey(moduleId));
}

int pollingIntervalForModule(const QVariantMap& profile, const QString& moduleId)
{
    const QVariantMap intervals = communicationProfile(profile).value(
        QStringLiteral("pollingIntervalsMs")).toMap();
    const int interval = intervals.value(moduleId).toInt();
    return interval > 0 ? interval : 100;
}

double dispatchRateForModule(const QVariantMap& profile, const QString& moduleId)
{
    const QVariantMap rates = communicationProfile(profile).value(
        QStringLiteral("maxDispatchRateHz")).toMap();
    const double rate = rates.value(moduleId).toDouble();
    return rate > 0.0 ? rate : 30.0;
}

const bool s_registered = [] {
    SoftwareInitializerFactory::registerInitializer(
        QStringLiteral("default"),
        [](const QString& softwareType, RunMode mode, QObject* parent) -> BaseSoftwareInitializer* {
            return new DefaultSoftwareInitializer(softwareType, mode, parent);
        });
    return true;
}();

}

DefaultSoftwareInitializer::DefaultSoftwareInitializer(const QString& softwareType,
                                                       RunMode mode,
                                                       QObject* parent)
    : BaseSoftwareInitializer(softwareType, mode, parent)
{
}

QStringList DefaultSoftwareInitializer::getEnabledModules() const
{
    return {
        QStringLiteral("params"),
        QStringLiteral("pointpick"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
}

QStringList DefaultSoftwareInitializer::getWorkflowSequence() const
{
    return {
        QStringLiteral("params"),
        QStringLiteral("pointpick"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
}

QString DefaultSoftwareInitializer::getInitialModule() const
{
    return QStringLiteral("params");
}

void DefaultSoftwareInitializer::registerModuleLogicHandlers(LogicRuntime* runtime)
{
    if (isModuleEnabled(QStringLiteral("params"))) {
        runtime->registerModuleHandler(new ParamsModuleLogicHandler(runtime));
    }
    if (isModuleEnabled(QStringLiteral("pointpick"))) {
        runtime->registerModuleHandler(new PointPickModuleLogicHandler(runtime));
    }
    if (isModuleEnabled(QStringLiteral("planning"))) {
        runtime->registerModuleHandler(new PlanningModuleLogicHandler(runtime));
    }
    if (isModuleEnabled(QStringLiteral("navigation"))) {
        runtime->registerModuleHandler(new NavigationModuleLogicHandler(runtime));
    }
}

void DefaultSoftwareInitializer::registerModuleUIs(MainWindow* mainWindow,
                                                   LogicRuntime* runtime,
                                                   ApplicationCoordinator* appCoord,
                                                   ILogicGateway* gateway)
{
    const ModuleUiAssemblyContext context{
        mainWindow,
        runtime,
        appCoord,
        gateway,
        m_pageManager,
        m_globalUiManager
    };

    if (isModuleEnabled(QStringLiteral("params"))) {
        registerParamsModuleUi(context);
    }
    if (isModuleEnabled(QStringLiteral("pointpick"))) {
        registerPointPickModuleUi(context);
    }
    if (isModuleEnabled(QStringLiteral("planning"))) {
        registerPlanningModuleUi(context);
    }
    if (isModuleEnabled(QStringLiteral("navigation"))) {
        registerNavigationModuleUi(context);
    }
}

void DefaultSoftwareInitializer::registerCommunicationSources(CommunicationHub* commHub)
{
    if (!commHub) {
        return;
    }

    const QVariantMap profile = getSoftwareProfile();
    commHub->setOutboundChannels(
        outboundControlChannelFromProfile(profile),
        ackChannelFromProfile(profile));
    for (const QString& routingChannel : routingChannelsFromProfile(profile)) {
        commHub->addRoutingChannel(routingChannel);
    }

    const QStringList modules = configuredEnabledModules();
    for (const QString& moduleId : modules) {
        const QString channel = subscriptionChannelForModule(profile, moduleId);
        if (!channel.isEmpty()) {
            commHub->addSubscriptionSource(new SubscriptionSource(
                QStringLiteral("%1_subscription").arg(moduleId),
                channel,
                moduleId));
        }

        const QString pollingKey = pollingKeyForModule(profile, moduleId);
        if (!pollingKey.isEmpty()) {
            auto* task = new PollingTask(
                QStringLiteral("%1_poll").arg(moduleId),
                moduleId,
                pollingKey,
                pollingIntervalForModule(profile, moduleId));
            task->setLatestWins(true);
            task->setChangeDetection(true);
            task->setMaxDispatchRateHz(dispatchRateForModule(profile, moduleId));
            commHub->addPollingTask(task);
        }
    }
}

