#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QWidget>

#include <functional>

class GlobalWidgetRegistry : public QObject
{
    Q_OBJECT

public:
    using WidgetFactory = std::function<QWidget*(QWidget* parent)>;

    explicit GlobalWidgetRegistry(QObject* parent = nullptr);
    ~GlobalWidgetRegistry() override = default;

    void registerFactory(const QString& widgetId, WidgetFactory factory);
    bool hasFactory(const QString& widgetId) const;
    QStringList registeredWidgetIds() const;
    QWidget* createWidget(const QString& widgetId, QWidget* parent = nullptr) const;

private:
    QMap<QString, WidgetFactory> m_factories;
};