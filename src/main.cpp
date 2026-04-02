#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QStringList>

#include "app/software/BaseSoftwareInitializer.h"
#include "app/software/RedisSoftwareResolver.h"
#include "app/software/SoftwareInitializerFactory.h"
#include "communication/hub/CommunicationHub.h"
#include "communication/redis/RedisGateway.h"
#include "logic/gateway/LocalLogicGateway.h"
#include "logic/runtime/LogicRuntime.h"
#include "shell/MainWindow.h"

namespace {

QString softwareTypeFromProfile(const QVariantMap& profile)
{
    const QString profileType = profile.value(QStringLiteral("softwareType")).toString();
    if (!profileType.isEmpty()) {
        return profileType;
    }

    return profile.value(QStringLiteral("initializer")).toString();
}

}

int main(int argc, char* argv[])
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);

    const QStringList arguments = QCoreApplication::arguments();
    const bool useRedisMode = arguments.contains(QStringLiteral("--redis"));
    const RunMode runMode = useRedisMode ? RunMode::Redis : RunMode::Local;

    LogicRuntime logicRuntime;
    RedisGateway redisGateway;
    CommunicationHub communicationHub(&redisGateway);
    communicationHub.initialize();

    bool redisReady = false;

    if (useRedisMode) {
        redisGateway.connectToServer(QStringLiteral("127.0.0.1"), 6379);
        redisReady = redisGateway.waitForConnected(2000);
    }

    LocalLogicGateway gateway(
        &logicRuntime,
        useRedisMode ? &communicationHub : nullptr,
        useRedisMode ? &redisGateway : nullptr);
    MainWindow mainWindow;

    RedisSoftwareResolver resolver(useRedisMode && redisReady ? &redisGateway : nullptr);
    const QVariantMap softwareProfile = resolver.resolveSoftwareProfile();
    QString softwareType = softwareTypeFromProfile(softwareProfile);
    if (softwareType.isEmpty()) {
        softwareType = resolver.resolveSoftwareType();
    }

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