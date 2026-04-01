#include "MainWindow.h"
#include "WorkspaceShell.h"

#include <QVBoxLayout>
#include <QResizeEvent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("VTKQtCore Medical Software");
    setMinimumSize(1024, 768);

    m_container = new QWidget(this);
    setCentralWidget(m_container);

    m_rootStack = new QStackedWidget(m_container);

    m_workspaceShell = new WorkspaceShell(m_rootStack);
    m_rootStack->addWidget(m_workspaceShell);

    m_globalOverlayLayer = new QWidget(m_container);
    m_globalOverlayLayer->raise();
    m_globalOverlayLayer->hide();

    m_globalToolHost = new QWidget(m_container);
    m_globalToolHost->raise();
    m_globalToolHost->hide();
}

WorkspaceShell* MainWindow::getWorkspaceShell() const
{
    return m_workspaceShell;
}

QStackedWidget* MainWindow::getRootStack() const
{
    return m_rootStack;
}

QWidget* MainWindow::getGlobalOverlayLayer() const
{
    return m_globalOverlayLayer;
}

QWidget* MainWindow::getGlobalToolHost() const
{
    return m_globalToolHost;
}

void MainWindow::addFullPage(const QString& pageId, QWidget* page)
{
    if (!page || pageId.isEmpty()) {
        return;
    }
    m_fullPages.insert(pageId, page);
    m_rootStack->addWidget(page);
}

void MainWindow::switchToPage(const QString& pageId)
{
    auto it = m_fullPages.find(pageId);
    if (it == m_fullPages.end()) {
        return;
    }
    m_rootStack->setCurrentWidget(it.value());
}

void MainWindow::switchToWorkspace()
{
    m_rootStack->setCurrentWidget(m_workspaceShell);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    const QSize newSize = m_container->size();
    m_rootStack->setGeometry(0, 0, newSize.width(), newSize.height());
    m_globalOverlayLayer->setGeometry(0, 0, newSize.width(), newSize.height());
    m_globalToolHost->setGeometry(0, 0, newSize.width(), newSize.height());
}
