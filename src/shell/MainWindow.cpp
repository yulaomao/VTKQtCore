#include "MainWindow.h"

#include "ui_MainWindow.h"

#include <QIcon>
#include <QResizeEvent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);
    setMinimumSize(1024, 768);
    setWindowIcon(QIcon(QStringLiteral(":/mainwindow/resources/app-window-icon.svg")));

    m_ui->globalOverlayLayer->raise();
    m_ui->globalOverlayLayer->hide();
    m_ui->globalToolHost->raise();
    m_ui->globalToolHost->hide();
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

QStackedWidget* MainWindow::getRootStack() const
{
    return m_ui->rootStack;
}

QWidget* MainWindow::getGlobalOverlayLayer() const
{
    return m_ui->globalOverlayLayer;
}

QWidget* MainWindow::getGlobalToolHost() const
{
    return m_ui->globalToolHost;
}

QWidget* MainWindow::getWorkspaceRootWidget() const
{
    return m_workspaceRootWidget;
}

void MainWindow::setWorkspaceRootWidget(QWidget* workspaceRootWidget)
{
    if (!workspaceRootWidget) {
        return;
    }

    if (m_workspaceRootWidget == workspaceRootWidget) {
        m_ui->rootStack->setCurrentWidget(workspaceRootWidget);
        return;
    }

    if (m_workspaceRootWidget) {
        m_ui->rootStack->removeWidget(m_workspaceRootWidget);
        m_workspaceRootWidget->deleteLater();
    }

    m_workspaceRootWidget = workspaceRootWidget;
    if (m_ui->rootStack->indexOf(workspaceRootWidget) < 0) {
        m_ui->rootStack->addWidget(workspaceRootWidget);
    }
    m_ui->rootStack->setCurrentWidget(workspaceRootWidget);
}

GlobalWidgetRegistry* MainWindow::getGlobalWidgetRegistry() const
{
    return m_globalWidgetRegistry;
}

void MainWindow::setGlobalWidgetRegistry(GlobalWidgetRegistry* globalWidgetRegistry)
{
    m_globalWidgetRegistry = globalWidgetRegistry;
}

void MainWindow::addFullPage(const QString& pageId, QWidget* page)
{
    if (!page || pageId.isEmpty()) {
        return;
    }
    m_fullPages.insert(pageId, page);
    m_ui->rootStack->addWidget(page);
}

void MainWindow::switchToPage(const QString& pageId)
{
    auto it = m_fullPages.find(pageId);
    if (it == m_fullPages.end()) {
        return;
    }
    m_ui->rootStack->setCurrentWidget(it.value());
}

void MainWindow::switchToWorkspace()
{
    if (m_workspaceRootWidget) {
        m_ui->rootStack->setCurrentWidget(m_workspaceRootWidget);
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    const QSize newSize = m_ui->mainWindowContainer->size();
    m_ui->rootStack->setGeometry(0, 0, newSize.width(), newSize.height());
    m_ui->globalOverlayLayer->setGeometry(0, 0, newSize.width(), newSize.height());
    m_ui->globalToolHost->setGeometry(0, 0, newSize.width(), newSize.height());
    m_ui->globalToolHost->raise();
    m_ui->globalOverlayLayer->raise();
}
