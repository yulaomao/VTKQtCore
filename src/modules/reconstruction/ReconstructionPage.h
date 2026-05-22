#pragma once

#include <QWidget>
#include <QVariantMap>

class QLabel;
class QPushButton;
class UiActionDispatcher;

class ReconstructionPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReconstructionPage(QWidget* parent = nullptr);

    void setActionDispatcher(UiActionDispatcher* dispatcher);

public slots:
    void setReconstructionStatus(const QString& status, bool done = false);

private:
    UiActionDispatcher* m_actionDispatcher = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_resetButton = nullptr;
};
