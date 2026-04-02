#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QMap>
#include <QString>

class WorkspaceShell;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

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
    Ui::MainWindow* m_ui = nullptr;
    WorkspaceShell* m_workspaceShell = nullptr;
    QMap<QString, QWidget*> m_fullPages;
};
