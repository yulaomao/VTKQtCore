#pragma once

#include <QWidget>
#include <QVariantMap>

class QLineEdit;
class QPushButton;
class UiActionDispatcher;

class ParamsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ParamsPage(QWidget* parent = nullptr);

    void setActionDispatcher(UiActionDispatcher* dispatcher);

public slots:
    void setParameterStatus(bool valid, int parameterCount = -1);

private:
    QVariantMap collectParameters() const;

    UiActionDispatcher* m_actionDispatcher = nullptr;
    QLineEdit* m_patientNameEdit = nullptr;
    QLineEdit* m_studyIdEdit = nullptr;
    QLineEdit* m_procedureTypeEdit = nullptr;
    QPushButton* m_applyButton = nullptr;
    QPushButton* m_datagenTestButton = nullptr;
    class QLabel* m_statusLabel = nullptr;
};
