#include "GlobalWidgetRegistry.h"

GlobalWidgetRegistry::GlobalWidgetRegistry(QObject* parent)
    : QObject(parent)
{
}

void GlobalWidgetRegistry::registerFactory(const QString& widgetId, WidgetFactory factory)
{
    if (widgetId.trimmed().isEmpty() || !factory) {
        return;
    }

    m_factories.insert(widgetId.trimmed(), std::move(factory));
}

bool GlobalWidgetRegistry::hasFactory(const QString& widgetId) const
{
    return m_factories.contains(widgetId.trimmed());
}

QStringList GlobalWidgetRegistry::registeredWidgetIds() const
{
    return m_factories.keys();
}

QWidget* GlobalWidgetRegistry::createWidget(const QString& widgetId, QWidget* parent) const
{
    const auto it = m_factories.constFind(widgetId.trimmed());
    if (it == m_factories.constEnd()) {
        return nullptr;
    }

    return it.value()(parent);
}