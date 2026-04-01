#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QMap>
#include <QString>

class WorkspaceShell;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    WorkspaceShell* getWorkspaceShell() const;
    QStackedWidget* getRootStack() const;
    QWidget* getGlobalOverlayLayer() const;
    QWidget* getGlobalToolHost() const;

    void addFullPage(const QString& pageId, QWidget* page);
    void switchToPage(const QString& pageId);
    void switchToWorkspace();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* m_container;
    QStackedWidget* m_rootStack;
    WorkspaceShell* m_workspaceShell;
    QWidget* m_globalOverlayLayer;
    QWidget* m_globalToolHost;
    QMap<QString, QWidget*> m_fullPages;
};
