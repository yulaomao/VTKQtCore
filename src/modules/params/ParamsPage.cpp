#include "ParamsPage.h"

#include "ParamsUiCommands.h"
#include "ui/coordination/UiActionDispatcher.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

ParamsPage::ParamsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel(QStringLiteral("Parameters Configuration"), this);
    mainLayout->addWidget(titleLabel);

    auto* formLayout = new QFormLayout;

    m_patientNameEdit = new QLineEdit(this);
    formLayout->addRow(QStringLiteral("Patient Name"), m_patientNameEdit);

    m_studyIdEdit = new QLineEdit(this);
    formLayout->addRow(QStringLiteral("Study ID"), m_studyIdEdit);

    m_procedureTypeEdit = new QLineEdit(this);
    formLayout->addRow(QStringLiteral("Procedure Type"), m_procedureTypeEdit);

    mainLayout->addLayout(formLayout);

    m_applyButton = new QPushButton(QStringLiteral("Apply Parameters"), this);
    mainLayout->addWidget(m_applyButton);

    m_datagenTestButton = new QPushButton(QStringLiteral("Test Datagen: Create Points"), this);
    mainLayout->addWidget(m_datagenTestButton);

    m_statusLabel = new QLabel(QStringLiteral("Status: Parameters not applied"), this);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    connect(m_applyButton, &QPushButton::clicked, this, [this]() {
        if (!m_actionDispatcher) {
            return;
        }

        m_actionDispatcher->sendCommand(
            ParamsUiCommands::applyParameters(),
            {{QStringLiteral("parameters"), collectParameters()}});
    });

    connect(m_datagenTestButton, &QPushButton::clicked, this, [this]() {
        if (!m_actionDispatcher) {
            return;
        }

        m_actionDispatcher->sendTargetedCommand(
            QStringLiteral("datagen"),
            QStringLiteral("create_node"),
            {{QStringLiteral("nodeType"), QStringLiteral("point")},
             {QStringLiteral("name"), QStringLiteral("Params Relay Points")},
             {QStringLiteral("count"), 4},
             {QStringLiteral("spacing"), 12.0},
             {QStringLiteral("relaySourceModule"), QStringLiteral("params")}});
    });
}

void ParamsPage::setActionDispatcher(UiActionDispatcher* dispatcher)
{
    m_actionDispatcher = dispatcher;
}

QVariantMap ParamsPage::collectParameters() const
{
    return {
        {QStringLiteral("patientName"), m_patientNameEdit ? m_patientNameEdit->text() : QString()},
        {QStringLiteral("studyId"), m_studyIdEdit ? m_studyIdEdit->text() : QString()},
        {QStringLiteral("procedureType"), m_procedureTypeEdit ? m_procedureTypeEdit->text() : QString()}
    };
}

void ParamsPage::setParameterStatus(bool valid, int parameterCount)
{
    QString text = valid
        ? QStringLiteral("Status: Parameters valid")
        : QStringLiteral("Status: Parameters incomplete");

    if (parameterCount >= 0) {
        text += QStringLiteral(" (%1 entries)").arg(parameterCount);
    }

    m_statusLabel->setText(text);
}
