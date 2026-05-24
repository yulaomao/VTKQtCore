#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QMap>
#include <QString>

class GlobalWidgetRegistry;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    QStackedWidget* getRootStack() const;
    QWidget* getGlobalOverlayLayer() const;
    QWidget* getGlobalToolHost() const;
    QWidget* getWorkspaceRootWidget() const;
    void setWorkspaceRootWidget(QWidget* workspaceRootWidget);

    GlobalWidgetRegistry* getGlobalWidgetRegistry() const;
    void setGlobalWidgetRegistry(GlobalWidgetRegistry* globalWidgetRegistry);

    void addFullPage(const QString& pageId, QWidget* page);
    void switchToPage(const QString& pageId);
    void switchToWorkspace();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    Ui::MainWindow* m_ui = nullptr;
    QWidget* m_workspaceRootWidget = nullptr;
    GlobalWidgetRegistry* m_globalWidgetRegistry = nullptr;
    QMap<QString, QWidget*> m_fullPages;
};
