#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QStackedWidget>

class PageManager : public QObject
{
    Q_OBJECT

public:
    explicit PageManager(QObject* parent = nullptr);
    ~PageManager() override = default;

    void setStackWidget(QStackedWidget* stack);
    void registerPage(const QString& moduleId, QWidget* page);
    void switchToPage(const QString& moduleId);
    QString getCurrentPage() const;
    QWidget* getPage(const QString& moduleId) const;

private:
    QStackedWidget* m_stackWidget;
    QMap<QString, QWidget*> m_pages;
    QString m_currentPage;
};
