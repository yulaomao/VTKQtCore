#include "PageManager.h"

PageManager::PageManager(QObject* parent)
    : QObject(parent)
    , m_stackWidget(nullptr)
{
}

void PageManager::setStackWidget(QStackedWidget* stack)
{
    m_stackWidget = stack;
}

void PageManager::registerPage(const QString& moduleId, QWidget* page)
{
    if (!page || moduleId.isEmpty()) {
        return;
    }
    m_pages.insert(moduleId, page);
    if (m_stackWidget) {
        m_stackWidget->addWidget(page);
    }
}

void PageManager::switchToPage(const QString& moduleId)
{
    auto it = m_pages.find(moduleId);
    if (it == m_pages.end() || !m_stackWidget) {
        return;
    }
    m_stackWidget->setCurrentWidget(it.value());
    m_currentPage = moduleId;
}

QString PageManager::getCurrentPage() const
{
    return m_currentPage;
}

QWidget* PageManager::getPage(const QString& moduleId) const
{
    return m_pages.value(moduleId, nullptr);
}
