#include <QApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QStringList>

#include "app/software/BaseSoftwareInitializer.h"
#include "app/software/SoftwareInitializerFactory.h"
#include "communication/hub/CommunicationHub.h"
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

QString argumentValue(const QStringList& arguments, const QString& optionName, const QString& fallback = QString())
{
    const QString prefix = optionName + QStringLiteral("=");
    for (int index = 0; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index);
        if (argument.startsWith(prefix)) {
            return argument.mid(prefix.size()).trimmed();
        }
        if (argument == optionName && index + 1 < arguments.size()) {
            return arguments.at(index + 1).trimmed();
        }
    }
    return fallback;
}

QVariantMap loadSoftwareProfile(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote()
            << QStringLiteral("[Startup] failed to open profile file: %1").arg(path);
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning().noquote()
            << QStringLiteral("[Startup] invalid profile JSON: %1").arg(parseError.errorString());
        return {};
    }

    return doc.object().toVariantMap();
}

}

int main(int argc, char* argv[])
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);

    const QStringList arguments = QCoreApplication::arguments();
    const bool useLocalMode = arguments.contains(QStringLiteral("--local"));
    const RunMode runMode = useLocalMode ? RunMode::Local : RunMode::Socket;

    LogicRuntime logicRuntime;
    CommunicationHub communicationHub;
    communicationHub.initialize();
    communicationHub.setServerEndpoint(
        argumentValue(arguments, QStringLiteral("--socket-host"), QStringLiteral("127.0.0.1")),
        argumentValue(arguments, QStringLiteral("--socket-port"), QStringLiteral("9000")).toUShort());

    LocalLogicGateway gateway(
        &logicRuntime,
        useLocalMode ? nullptr : &communicationHub);

    QVariantMap softwareProfile = loadSoftwareProfile(
        argumentValue(arguments, QStringLiteral("--software-profile")));
    const QString requestedSoftwareType = argumentValue(arguments, QStringLiteral("--software-type"));
    if (!requestedSoftwareType.isEmpty()) {
        softwareProfile.insert(QStringLiteral("softwareType"), requestedSoftwareType);
    }
    const QString requestedStyleTheme = argumentValue(arguments, QStringLiteral("--style-theme"));
    if (!requestedStyleTheme.isEmpty()) {
        softwareProfile.insert(QStringLiteral("styleTheme"), requestedStyleTheme);
    }
    QString softwareType = softwareTypeFromProfile(softwareProfile);
    if (softwareType.isEmpty()) {
        softwareType = QStringLiteral("default");
    }

    AppStyleManager styleManager(&app, &app);
    styleManager.registerStyle(
        QStringLiteral("clinical-light"),
        QStringLiteral(":/styles/styles/app-theme.qss"));
    const QString styleTheme = styleThemeFromProfile(softwareProfile);
    if (!styleTheme.isEmpty()) {
        styleManager.applyStyle(styleTheme);
    }
    if (styleManager.currentStyleId().isEmpty()) {
        styleManager.applyStyle(QStringLiteral("clinical-light"));
    }

    MainWindow mainWindow;

    BaseSoftwareInitializer* initializer =
        SoftwareInitializerFactory::create(softwareType, runMode, &app);
    initializer->setSoftwareProfile(softwareProfile);
    initializer->initialize(&mainWindow, &logicRuntime, &gateway, &communicationHub);

    if (!useLocalMode) {
        communicationHub.start();
    }

    mainWindow.show();
    const int exitCode = app.exec();

    if (!useLocalMode) {
        communicationHub.stop();
    }

    return exitCode;
}
