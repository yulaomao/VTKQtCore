#include <QApplication>
#include <QDebug>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QStringList>

#include "app/software/BaseSoftwareInitializer.h"
#include "app/software/RedisSoftwareResolver.h"
#include "app/software/SoftwareInitializerFactory.h"
#include "communication/MessageDispatchCenter.h"
#include "communication/RedisConnectionWorker.h"
#include "logic/gateway/LocalLogicGateway.h"
#include "logic/runtime/LogicRuntime.h"
#include "shell/MainWindow.h"
#include "ui/globalui/AppStyleManager.h"

int main(int argc, char* argv[])
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);

    const bool useRedisMode = true;
    const RunMode runMode = useRedisMode ? RunMode::Redis : RunMode::Local;

    // Core logic runtime.
    LogicRuntime logicRuntime;

    // Central message dispatch center (owns all Redis worker threads).
    MessageDispatchCenter dispatchCenter(logicRuntime.getModuleLogicRegistry());

    // Gateway that bridges the UI layer to the logic runtime.
    LocalLogicGateway gateway(&logicRuntime);

    // Read the software profile from Redis before starting the worker threads.
    // Use a temporary command-only worker on the default DB.
    QVariantMap softwareProfile;
    QString softwareType;

    if (useRedisMode) {
        RedisConnectionConfig probeConfig;
        probeConfig.connectionId = QStringLiteral("probe");
        probeConfig.host         = QStringLiteral("127.0.0.1");
        probeConfig.port         = 6379;
        probeConfig.db           = 0;
        // No polling groups or subscription channels – command-only.

        RedisConnectionWorker probeWorker(probeConfig);
        // Call start() directly from the main thread; no QTimer will be created
        // because pollingKeyGroups is empty. No subscriber thread because
        // subscriptionChannels is empty.
        probeWorker.start();

        RedisSoftwareResolver resolver(&probeWorker);
        softwareProfile = resolver.resolveSoftwareProfile();
        softwareType    = softwareProfile.value(QStringLiteral("softwareType")).toString();
        if (softwareType.isEmpty()) {
            softwareType = softwareProfile.value(QStringLiteral("initializer")).toString();
        }
        if (softwareType.isEmpty()) {
            softwareType = resolver.resolveSoftwareType();
        }

        probeWorker.stop();
    }

    if (softwareType.isEmpty()) {
        softwareType = QStringLiteral("default");
    }

    // Apply style theme.
    AppStyleManager styleManager(&app, &app);
    styleManager.registerStyle(
        QStringLiteral("clinical-light"),
        QStringLiteral(":/styles/styles/app-theme.qss"));
    const QString requestedStyleTheme =
        softwareProfile.value(QStringLiteral("styleTheme")).toString().trimmed();
    if (!requestedStyleTheme.isEmpty()) {
        styleManager.applyStyle(requestedStyleTheme);
    }
    if (styleManager.currentStyleId().isEmpty()) {
        styleManager.applyStyle(QStringLiteral("clinical-light"));
    }

    // Create and initialize the main window.
    MainWindow mainWindow;

    BaseSoftwareInitializer* initializer =
        SoftwareInitializerFactory::create(softwareType, runMode, &app);
    initializer->setSoftwareProfile(softwareProfile);
    initializer->initialize(&mainWindow, &logicRuntime, &gateway, &dispatchCenter);

    // Start all Redis connection worker threads.
    if (useRedisMode) {
        dispatchCenter.start();
    }

    mainWindow.show();
    const int exitCode = app.exec();

    if (useRedisMode) {
        dispatchCenter.stop();
    }

    return exitCode;
}
