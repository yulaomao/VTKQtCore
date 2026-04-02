#pragma once

#include <QWidget>
#include <QVariantMap>

class QLineEdit;
class QPushButton;

class ParamsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ParamsPage(QWidget* parent = nullptr);

signals:
    void parameterApplied(const QVariantMap& params);
    void datagenPointCreateRequested();

public slots:
    void setParameterStatus(bool valid, int parameterCount = -1);

private:
    QLineEdit* m_patientNameEdit = nullptr;
    QLineEdit* m_studyIdEdit = nullptr;
    QLineEdit* m_procedureTypeEdit = nullptr;
    QPushButton* m_applyButton = nullptr;
    QPushButton* m_datagenTestButton = nullptr;
    class QLabel* m_statusLabel = nullptr;
};
