#include "AppStyleManager.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>

AppStyleManager::AppStyleManager(QApplication* application, QObject* parent)
    : QObject(parent)
    , m_application(application)
{
}

void AppStyleManager::registerStyle(const QString& styleId,
                                    const QString& styleSheetResourcePath)
{
    if (styleId.isEmpty() || styleSheetResourcePath.isEmpty()) {
        return;
    }

    m_styleResources.insert(styleId, styleSheetResourcePath);
}

bool AppStyleManager::applyStyle(const QString& styleId)
{
    if (!m_application || !m_styleResources.contains(styleId)) {
        return false;
    }

    const QString styleSheet = loadStyleSheet(m_styleResources.value(styleId));
    if (styleSheet.isNull()) {
        return false;
    }

    m_application->setStyleSheet(styleSheet);
    m_currentStyleId = styleId;
    return true;
}

QString AppStyleManager::currentStyleId() const
{
    return m_currentStyleId;
}

QStringList AppStyleManager::availableStyles() const
{
    return m_styleResources.keys();
}

QString AppStyleManager::loadStyleSheet(const QString& styleSheetResourcePath) const
{
    QFile file(styleSheetResourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream stream(&file);
    return stream.readAll();
}