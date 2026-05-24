#include "ApplicationLauncher.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSurfaceFormat>
#include <QVariantMap>
#include <QVTKOpenGLNativeWidget.h>

#include <memory>

#include "ProductDefinition.h"
#include "app/software/BaseSoftwareInitializer.h"
#include "app/software/ProductSoftwareInitializer.h"
#include "communication/hub/CommunicationHub.h"
#include "logic/runtime/LogicRuntime.h"
#include "shell/MainWindow.h"
#include "ui/globalui/AppStyleManager.h"

namespace {

QString softwareTypeFromProfile(const QVariantMap& profile)
{
    const QString profileType = profile.value(QStringLiteral("softwareType")).toString().trimmed();
    if (!profileType.isEmpty()) {
        return profileType;
    }

    return profile.value(QStringLiteral("initializer")).toString().trimmed();
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
        if (argument == optionName) {
            if (index + 1 >= arguments.size()) {
                return fallback;
            }
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

quint16 socketPortFromArguments(const QStringList& arguments)
{
    const QString rawPort = argumentValue(
        arguments,
        QStringLiteral("--socket-port"),
        QStringLiteral("9000"));
    bool ok = false;
    const ushort port = rawPort.toUShort(&ok);
    if (!ok || port == 0) {
        qWarning().noquote()
            << QStringLiteral("[Startup] invalid --socket-port '%1', using 9000").arg(rawPort);
        return 9000;
    }
    return port;
}

void applyProductDefaults(QVariantMap* softwareProfile, const ProductDefinition& productDefinition)
{
    if (!softwareProfile) {
        return;
    }

    if (!productDefinition.softwareType.trimmed().isEmpty()) {
        softwareProfile->insert(QStringLiteral("softwareType"), productDefinition.softwareType);
    }

    if (styleThemeFromProfile(*softwareProfile).isEmpty() && !productDefinition.styleTheme.trimmed().isEmpty()) {
        softwareProfile->insert(QStringLiteral("styleTheme"), productDefinition.styleTheme);
    }
}

QVariantMap mergedVariantMap(const QVariantMap& base, const QVariantMap& overrideValues)
{
    QVariantMap result = base;
    for (auto it = overrideValues.constBegin(); it != overrideValues.constEnd(); ++it) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const bool isOverrideMap = it.value().typeId() == QMetaType::QVariantMap;
        const bool isBaseMap = result.value(it.key()).typeId() == QMetaType::QVariantMap;
#else
        const bool isOverrideMap = it.value().type() == QVariant::Map;
        const bool isBaseMap = result.value(it.key()).type() == QVariant::Map;
#endif
        if (isOverrideMap && isBaseMap) {
            result.insert(
                it.key(),
                mergedVariantMap(result.value(it.key()).toMap(), it.value().toMap()));
            continue;
        }
        result.insert(it.key(), it.value());
    }

    return result;
}

}

int runProductApplication(int argc, char* argv[], const ProductDefinition& productDefinition)
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(argc, argv);

    if (!productDefinition.productId.isEmpty()) {
        app.setApplicationName(productDefinition.productId);
    }
    if (!productDefinition.displayName.isEmpty()) {
        app.setApplicationDisplayName(productDefinition.displayName);
    }
    if (!productDefinition.organizationName.isEmpty()) {
        app.setOrganizationName(productDefinition.organizationName);
    }
    if (!productDefinition.versionString.isEmpty()) {
        app.setApplicationVersion(productDefinition.versionString);
    }

    const QStringList arguments = QCoreApplication::arguments();
    const bool useLocalMode = arguments.contains(QStringLiteral("--local"));
    const RunMode runMode = useLocalMode ? RunMode::Local : RunMode::Socket;

    LogicRuntime logicRuntime;
    std::unique_ptr<CommunicationHub> communicationHub;
    if (runMode == RunMode::Socket) {
        communicationHub = std::make_unique<CommunicationHub>();
        communicationHub->initialize();
        communicationHub->setServerEndpoint(
            argumentValue(arguments, QStringLiteral("--socket-host"), QStringLiteral("127.0.0.1")),
            socketPortFromArguments(arguments));
    }

    const QVariantMap defaultProfile = loadSoftwareProfile(productDefinition.defaultProfileResourcePath);
    QVariantMap softwareProfile = loadSoftwareProfile(
        argumentValue(arguments, QStringLiteral("--software-profile")));
    softwareProfile = mergedVariantMap(defaultProfile, softwareProfile);

    const QString requestedStyleTheme = argumentValue(arguments, QStringLiteral("--style-theme"));
    if (!requestedStyleTheme.isEmpty()) {
        softwareProfile.insert(QStringLiteral("styleTheme"), requestedStyleTheme);
    }

    applyProductDefaults(&softwareProfile, productDefinition);

    QString softwareType = softwareTypeFromProfile(softwareProfile);
    if (softwareType.isEmpty()) {
        softwareType = productDefinition.softwareType.isEmpty()
            ? QStringLiteral("default")
            : productDefinition.softwareType;
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
    if (!productDefinition.displayName.isEmpty()) {
        mainWindow.setWindowTitle(productDefinition.displayName);
    }
    if (!productDefinition.windowIconResourcePath.isEmpty()) {
        mainWindow.setWindowIcon(QIcon(productDefinition.windowIconResourcePath));
    }

    if (productDefinition.registerModulePacks) {
        productDefinition.registerModulePacks();
    }

    BaseSoftwareInitializer* initializer =
        new ProductSoftwareInitializer(productDefinition, defaultProfile, runMode, &app);
    initializer->setSoftwareProfile(softwareProfile);
    initializer->initialize(&mainWindow, &logicRuntime, &logicRuntime, communicationHub.get());

    if (communicationHub) {
        communicationHub->start();
    }

    mainWindow.show();
    const int exitCode = app.exec();

    if (communicationHub) {
        communicationHub->stop();
    }

    return exitCode;
}