#include <QApplication>
#include <QDebug>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QStringList>

#include "app/software/BaseSoftwareInitializer.h"
#include "app/software/RedisSoftwareResolver.h"
#include "app/software/SoftwareInitializerFactory.h"
#include "communication/hub/CommunicationHub.h"
#include "communication/hub/IRedisCommandAccess.h"
#include "communication/redis/RedisLogicCommandAccess.h"
#include "communication/redis/RedisGateway.h"
#include "logic/gateway/LocalLogicGateway.h"
#include "logic/runtime/LogicRuntime.h"
#include "shell/MainWindow.h"
#include "ui/globalui/AppStyleManager.h"

namespace {

QString softwareTypeFromProfile(const QVariantMap& profile)
{
    const QString profileType = profile.value(QStringLiteral("softwareType")).toString();
    if (!profileType.isEmpty()) {
        return profileType;
    }

    return profile.value(QStringLiteral("initializer")).toString();
}

QString styleThemeFromProfile(const QVariantMap& profile)
{
    const QString themeId = profile.value(QStringLiteral("styleTheme")).toString().trimmed();
    if (!themeId.isEmpty()) {
        return themeId;
    }

    return profile.value(QStringLiteral("globalStyleTheme")).toString().trimmed();
}

QString redisConnectionStateName(RedisGateway::ConnectionState state)
{
    switch (state) {
    case RedisGateway::Connected:
        return QStringLiteral("Connected");
    case RedisGateway::Reconnecting:
        return QStringLiteral("Reconnecting");
    case RedisGateway::Disconnected:
    default:
        return QStringLiteral("Disconnected");
    }
}

}

int main(int argc, char* argv[])
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);

    const QStringList arguments = QCoreApplication::arguments();
    const bool useRedisMode = true; // arguments.contains(QStringLiteral("--redis"));
    const RunMode runMode = useRedisMode ? RunMode::Redis : RunMode::Local;

    LogicRuntime logicRuntime;
    RedisGateway redisGateway;
    RedisLogicCommandAccess logicRedisCommandAccess;
    CommunicationHub communicationHub(&redisGateway);
    communicationHub.initialize();
    logicRuntime.setRedisCommandAccess(useRedisMode ? static_cast<IRedisCommandAccess*>(&logicRedisCommandAccess)
                                                    : nullptr);

    QObject::connect(&redisGateway, &RedisGateway::connectionStateChanged,
                     &app, [](RedisGateway::ConnectionState state) {
                         qInfo().noquote()
                             << QStringLiteral("[Redis] state changed -> %1")
                                    .arg(redisConnectionStateName(state));
                     });
    QObject::connect(&redisGateway, &RedisGateway::errorOccurred,
                     &app, [](const QString& errorMessage) {
                         qWarning().noquote()
                             << QStringLiteral("[Redis] error: %1").arg(errorMessage);
                     });
    QObject::connect(&logicRedisCommandAccess, &RedisLogicCommandAccess::errorOccurred,
                     &app, [](const QString& errorMessage) {
                         qWarning().noquote()
                             << QStringLiteral("[RedisLogicCommand] error: %1").arg(errorMessage);
                     });
    QObject::connect(&logicRedisCommandAccess, &RedisLogicCommandAccess::errorOccurred,
                     &logicRuntime, [&logicRuntime](const QString& errorMessage) {
                         logicRuntime.onCommunicationIssue(
                             QStringLiteral("RedisLogicCommandAccess"),
                             QStringLiteral("warning"),
                             QStringLiteral("COMM_REDIS_LOGIC_COMMAND_ERROR"),
                             errorMessage,
                             {{QStringLiteral("layer"), QStringLiteral("logic_command")}});
                     });

    bool redisReady = false;

    if (useRedisMode) {
        qInfo().noquote() << QStringLiteral("[Redis] connecting to 127.0.0.1:6379 ...");
        redisGateway.connectToServer(QStringLiteral("127.0.0.1"), 6379);
        logicRedisCommandAccess.connectToServer(QStringLiteral("127.0.0.1"), 6379);
        redisReady = redisGateway.waitForConnected(2000);
        if (redisReady) {
            qInfo().noquote() << QStringLiteral("[Redis] connected to 127.0.0.1:6379");
        } else {
            qWarning().noquote()
                << QStringLiteral("[Redis] connect failed or timed out, final state=%1")
                       .arg(redisConnectionStateName(redisGateway.getConnectionState()));
        }
    }

    LocalLogicGateway gateway(
        &logicRuntime,
        useRedisMode ? &communicationHub : nullptr,
        useRedisMode ? &redisGateway : nullptr);

    RedisSoftwareResolver resolver(useRedisMode && redisReady ? &redisGateway : nullptr);
    const QVariantMap softwareProfile = resolver.resolveSoftwareProfile();
    QString softwareType = softwareTypeFromProfile(softwareProfile);
    if (softwareType.isEmpty()) {
        softwareType = resolver.resolveSoftwareType();
    }

    AppStyleManager styleManager(&app, &app);
    styleManager.registerStyle(
        QStringLiteral("clinical-light"),
        QStringLiteral(":/styles/styles/app-theme.qss"));
    const QString requestedStyleTheme = styleThemeFromProfile(softwareProfile);
    if (!requestedStyleTheme.isEmpty()) {
        styleManager.applyStyle(requestedStyleTheme);
    }
    if (styleManager.currentStyleId().isEmpty()) {
        styleManager.applyStyle(QStringLiteral("clinical-light"));
    }

    MainWindow mainWindow;

    BaseSoftwareInitializer* initializer =
        SoftwareInitializerFactory::create(softwareType, runMode, &app);
    initializer->setSoftwareProfile(softwareProfile);
    initializer->initialize(&mainWindow, &logicRuntime, &gateway, &communicationHub);

    if (useRedisMode) {
        communicationHub.start();
    }

    mainWindow.show();
    const int exitCode = app.exec();

    if (useRedisMode) {
        communicationHub.stop();
    }

    return exitCode;
}